// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Logarithmic (bank-interleaved) crossbar on the io_v2 protocol.
//
// Port of log_ico.cpp to io_v2. The crossbar connects ``nb_masters`` input
// master ports to ``nb_slaves`` output bank ports. Every incoming request
// has:
//   - ``remove_offset`` subtracted from its byte address
//   - the middle ``slave_bits = ceil_log2(nb_slaves)`` bits sliced out to
//     pick a bank
//   - the remaining bits recomposed into a bank-local address by placing
//     the high bits (above the bank selector) above the ``interleaving_width``
//     low bits
//
// ::
//
//     offset      = addr - remove_offset
//     bank_id     = (offset >> interleaving_width) & (nb_slaves - 1)
//     bank_offset = ((offset >> (slave_bits + interleaving_width)) << interleaving_width)
//                   | (offset & ((1 << interleaving_width) - 1))
//
// The request's address is rewritten to ``bank_offset`` before forwarding,
// so the bank sees its local address space starting at 0.
//
// IO-side plumbing differs from v1:
//   - Muxed slave ports per master. The shared ``input_req`` callback gets
//     the master id as its ``int id`` argument.
//   - Muxed master ports per bank. The shared ``output_resp`` /
//     ``output_retry`` callbacks get the bank id as their ``int id``
//     argument.
//   - Replies travel back through the muxed slave port via
//     ``inputs[m]->resp(req)`` — no v1 ``resp_port`` lookup.
//   - Back-pressure uses the AXI-like ``DENIED``/``retry()`` handshake.
//     Each master has an independent denied state; a bank that is DENYing
//     one master does not affect traffic from other masters to other banks.
//
// Request flow
// ============
//
// - **DONE** from a bank: we free the InFlight bookkeeping and return
//   DONE up the same master. No further callback.
// - **GRANTED** from a bank: we install an ``InFlight`` on ``req->initiator``
//   so the eventual ``resp()`` from the bank knows which master to reply
//   on. We forward GRANTED up to that master.
// - **DENIED** from a bank: we restore the original address, free the
//   in-flight slot, record ``denied_on_bank[m] = bank_id``, and return
//   DENIED up to that master. The master must hold the request until we
//   call ``retry()`` on its input port.
// - **Bank retry**: when bank ``b`` fires ``retry()`` to us, we iterate
//   over every master ``m`` with ``denied_on_bank[m] == b`` and call
//   ``inputs[m]->retry()``. Other masters blocked on other banks are
//   untouched.
//
// Timing model: big-packet, zero-latency. No extra ``inc_latency``, no
// event scheduling. Whatever latency a bank annotates on ``req->latency``
// or whatever wall-clock delay it introduces via a deferred ``resp()`` is
// observed directly by the calling master.

#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <interco/log_ico_config.hpp>

struct LogIcoInFlight
{
    // Master id that issued this request.
    int input_id;
    // Original ``initiator`` pointer, restored on resp/deny.
    void *saved_initiator;
    // Original address before bank-local rewrite, restored on DENIED so the
    // master's re-send finds the same incoming address.
    uint64_t saved_addr;
};

class LogIco : public vp::Component
{
public:
    LogIco(vp::ComponentConf &conf);
    void reset(bool active) override;

    LogIcoConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id);

    LogIcoInFlight *alloc_inflight();
    void            free_inflight(LogIcoInFlight *ifl);

    vp::Trace trace;

    std::vector<std::unique_ptr<vp::IoSlave>>  inputs;
    std::vector<std::unique_ptr<vp::IoMaster>> outputs;

    // Bit width of the bank selector (``ceil_log2(nb_slaves)``). Cached
    // once because it depends only on the config.
    int slave_bits = 0;

    // Per-master: bank id that last denied this master, or -1 when the
    // master is not currently holding a denied request.
    std::vector<int> denied_on_bank;

    // Free list of in-flight bookkeeping slots.
    LogIcoInFlight *inflight_free = nullptr;
};


static int ceil_log2_u(unsigned int n)
{
    if (n <= 1) return 0;
    return 32 - __builtin_clz(n - 1);
}


LogIco::LogIco(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    int nb_masters = (int)this->cfg.nb_masters;
    int nb_slaves  = (int)this->cfg.nb_slaves;
    this->slave_bits = ceil_log2_u((unsigned int)nb_slaves);

    this->outputs.reserve(nb_slaves);
    for (int i = 0; i < nb_slaves; i++)
    {
        auto m = std::make_unique<vp::IoMaster>(
            i, &LogIco::output_retry, &LogIco::output_resp);
        this->new_master_port("output_" + std::to_string(i), m.get());
        this->outputs.push_back(std::move(m));
    }

    this->inputs.reserve(nb_masters);
    for (int i = 0; i < nb_masters; i++)
    {
        auto s = std::make_unique<vp::IoSlave>(i, &LogIco::input_req);
        this->new_slave_port("input_" + std::to_string(i), s.get());
        this->inputs.push_back(std::move(s));
    }

    this->denied_on_bank.assign((size_t)nb_masters, -1);
}

void LogIco::reset(bool active)
{
    if (active)
    {
        std::fill(this->denied_on_bank.begin(),
                  this->denied_on_bank.end(), -1);
    }
}


LogIcoInFlight *LogIco::alloc_inflight()
{
    LogIcoInFlight *ifl = this->inflight_free;
    if (ifl != nullptr)
    {
        this->inflight_free = (LogIcoInFlight *)ifl->saved_initiator;
        return ifl;
    }
    return new LogIcoInFlight();
}

void LogIco::free_inflight(LogIcoInFlight *ifl)
{
    // Reuse saved_initiator as the free-list link — it will be overwritten
    // on the next alloc.
    ifl->saved_initiator = (void *)this->inflight_free;
    this->inflight_free = ifl;
}


vp::IoReqStatus LogIco::input_req(vp::Block *__this, vp::IoReq *req, int id)
{
    LogIco *_this = (LogIco *)__this;

    uint64_t addr = req->get_addr();
    uint64_t offset = addr - (uint64_t)_this->cfg.remove_offset;

    int mask = (1 << _this->slave_bits) - 1;
    int bank_id = 0;
    if (_this->slave_bits > 0)
    {
        bank_id = (int)((offset >> _this->cfg.interleaving_width) & (uint64_t)mask);
    }

    uint64_t iw = (uint64_t)_this->cfg.interleaving_width;
    uint64_t iw_mask = (iw == 0) ? 0 : ((((uint64_t)1) << iw) - 1);
    uint64_t hi_shift = (uint64_t)_this->slave_bits + iw;
    uint64_t bank_offset = ((offset >> hi_shift) << iw) | (offset & iw_mask);

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Routing IO req (input: %d, addr: 0x%llx -> bank %d bank_addr: 0x%llx, "
        "size: 0x%llx, write: %d)\n",
        id, (unsigned long long)addr, bank_id,
        (unsigned long long)bank_offset,
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0);

    // Install InFlight so response / deny can find the originating master
    // and we can roll the addr back on DENIED.
    LogIcoInFlight *ifl = _this->alloc_inflight();
    ifl->input_id        = id;
    ifl->saved_initiator = req->initiator;
    ifl->saved_addr      = addr;
    req->initiator = ifl;

    req->set_addr(bank_offset);

    vp::IoReqStatus st = _this->outputs[bank_id]->req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Inline completion: restore initiator, free InFlight, return DONE.
        req->initiator = ifl->saved_initiator;
        _this->free_inflight(ifl);
        return vp::IO_REQ_DONE;
    }

    if (st == vp::IO_REQ_DENIED)
    {
        // Nothing actually went through. Roll the request back to its
        // pre-forward state so the master's re-send looks like the
        // original issue.
        req->initiator = ifl->saved_initiator;
        req->set_addr(ifl->saved_addr);
        _this->free_inflight(ifl);

        _this->denied_on_bank[id] = bank_id;
        return vp::IO_REQ_DENIED;
    }

    // IO_REQ_GRANTED: keep InFlight active; the bank will call resp() later.
    return vp::IO_REQ_GRANTED;
}


void LogIco::output_resp(vp::Block *__this, vp::IoReq *req, int /*bank_id*/)
{
    LogIco *_this = (LogIco *)__this;
    LogIcoInFlight *ifl = (LogIcoInFlight *)req->initiator;
    int input_id = ifl->input_id;

    req->initiator = ifl->saved_initiator;
    _this->free_inflight(ifl);

    _this->inputs[input_id]->resp(req);
}


void LogIco::output_retry(vp::Block *__this, int id)
{
    LogIco *_this = (LogIco *)__this;

    // Wake every master that is currently blocked on bank ``id``. A master
    // blocked on a different bank stays blocked — its retry will come when
    // its own bank retries.
    for (size_t m = 0; m < _this->denied_on_bank.size(); m++)
    {
        if (_this->denied_on_bank[m] == id)
        {
            _this->denied_on_bank[m] = -1;
            _this->inputs[m]->retry();
        }
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new LogIco(config);
}
