// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Bit-field address demultiplexer on the io_v2 protocol.
 *
 * Direct port of demux.cpp to io_v2. Functionally unchanged: a slice of the
 * request address (``width`` bits starting at bit ``offset``) selects one of
 * ``1 << width`` downstream master ports; every other bit of the request is
 * forwarded verbatim. The demux is fully stateless — it does not buffer
 * requests, does not touch the address, and does not add latency.
 *
 * What changed vs v1:
 *   - Single-master slave port. Replies travel back via
 *     ``input_itf.resp(req)`` rather than v1's ``req->resp_port`` lookup.
 *   - Status codes are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``.
 *   - AXI-like deny/retry handshake: on a downstream DENY we return DENIED
 *     to the master and remember which output denied us (``denied_output``).
 *     When that same output fires ``retry()`` we propagate ``retry()`` on
 *     the input — only then, not on any unrelated output's retry. This
 *     avoids spurious upstream retries when multiple outputs happen to
 *     toggle their ready state concurrently.
 *
 * Timing model: big-packet, no annotation. The demux never touches
 * ``req->latency`` — whatever the downstream reports is what the upstream
 * sees. Sync (``IO_REQ_DONE``) and async (``IO_REQ_GRANTED`` + deferred
 * ``resp()``) downstream replies are both supported; the demux just wires
 * them through.
 */

#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/signal.hpp>
#include <vp/debug_mem.hpp>
#include <interco/demux_v2/demux_config.hpp>

class Demux : public vp::Component, public vp::DebugMemIf
{
public:
    Demux(vp::ComponentConf &conf);
    void reset(bool active) override;

    // Backdoor debug access (vp/debug_mem.hpp). The default
    // debug_mem_regions() is kept: like on the timed path the address is
    // forwarded verbatim (select bits included), so emitting per-output
    // regions would alias; the demux advertises itself as a terminal and
    // debug_mem_access() redoes the output select per access.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;

    DemuxConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id, vp::IoRetryChannel);

    // Pulse the per-instance VCD traces (addr / size / is_write) for one
    // cycle, with a sub-cycle delay spread so multiple accesses in the same
    // cycle stay visible. Same idiom used by the v2 routers and udma_core.
    void log_access(uint64_t addr, uint64_t size, bool is_write);

    vp::Trace trace;

    vp::IoSlave input_itf{&Demux::input_req};

    // One master per downstream. Muxed so the shared resp/retry callbacks can
    // identify which output fired back.
    std::vector<std::unique_ptr<vp::IoMaster>> outputs;

    // Id of the downstream that denied the most recent upstream request, or
    // -1 if we have no outstanding upstream DENY. When this output later
    // fires ``retry()`` we propagate the retry on the input port so the
    // upstream master re-sends. At most one upstream request can be in the
    // denied state at a time (single slave port), so one int is enough.
    int denied_output = -1;

    // ---- VCD traces ----
    // One pulse triplet per demux instance — addr/size/is_write of every
    // forwarded request. The demux is stateless and never modifies the
    // address, so the logged value is the raw upstream address.
    vp::Signal<uint64_t> req_addr;
    vp::Signal<uint64_t> req_size;
    vp::Signal<bool>     req_is_write;
    int64_t              last_logged_access = -1;
    int                  nb_logged_access_in_same_cycle = 0;

    // Per-output backdoor targets, resolved on first debug access through
    // the output ports' final bindings. nullptr where the downstream does
    // not support backdoor accesses.
    std::vector<vp::DebugMemIf *> output_debug_mem;
    bool output_debug_mem_resolved = false;
};


Demux::Demux(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      req_addr(*this, "addr", 64, vp::SignalCommon::ResetKind::HighZ),
      req_size(*this, "size", 64, vp::SignalCommon::ResetKind::HighZ),
      req_is_write(*this, "is_write", 1, vp::SignalCommon::ResetKind::HighZ)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input", &this->input_itf);

    int nb_outs = 1 << this->cfg.width;
    this->outputs.reserve(nb_outs);
    for (int i = 0; i < nb_outs; i++)
    {
        auto m = std::make_unique<vp::IoMaster>(
            i, &Demux::output_retry, &Demux::output_resp);
        this->new_master_port("output_" + std::to_string(i), m.get());
        this->outputs.push_back(std::move(m));
    }
}

void Demux::reset(bool active)
{
    if (active)
    {
        this->denied_output = -1;
    }
}


vp::IoReqStatus Demux::input_req(vp::Block *__this, vp::IoReq *req)
{
    Demux *_this = (Demux *)__this;

    uint64_t offset = req->get_addr();
    int mask = (1 << _this->cfg.width) - 1;
    int output_id = (int)((offset >> _this->cfg.offset) & (uint64_t)mask);

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Routing IO req (req: %p, addr: 0x%llx, size: 0x%llx, is_write: %d) "
        "to output %d\n",
        req, (unsigned long long)offset,
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0, output_id);

    _this->log_access(offset, req->get_size(), req->get_is_write());

    vp::IoReqStatus st = _this->outputs[output_id]->req(req);

    if (st == vp::IO_REQ_DENIED)
    {
        // Remember who denied us so we can propagate the eventual retry to
        // the upstream master. We only ever latch one denied output — if the
        // upstream resends to a different route, that route's own DENY path
        // will overwrite this field.
        _this->denied_output = output_id;
    }
    return st;
}


vp::IoRespAck Demux::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    Demux *_this = (Demux *)__this;
    // Stateless forward: reply on the single upstream port and propagate its
    // accept/deny back to the downstream. The request object itself carries any
    // latency the downstream annotated, so there is nothing to add here.
    return _this->input_itf.resp(req);
}


void Demux::output_retry(vp::Block *__this, int id, vp::IoRetryChannel)
{
    Demux *_this = (Demux *)__this;
    if (_this->denied_output == id)
    {
        _this->denied_output = -1;
        _this->input_itf.retry();
    }
}


void Demux::log_access(uint64_t addr, uint64_t size, bool is_write)
{
    int64_t cycles = this->clock.get_cycles();
    if (cycles > this->last_logged_access)
        this->nb_logged_access_in_same_cycle = 0;

    int64_t time_delay = 0;
    if (this->nb_logged_access_in_same_cycle > 0)
    {
        int64_t period = this->clock.get_period();
        time_delay = period - (period >> this->nb_logged_access_in_same_cycle);
    }
    this->req_addr.set_and_release(addr,     0, time_delay);
    this->req_size.set_and_release(size,     0, time_delay);
    this->req_is_write.set_and_release(is_write, 0, time_delay);
    this->nb_logged_access_in_same_cycle++;
    this->last_logged_access = cycles;
}


int Demux::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
    bool is_write)
{
    if (!this->output_debug_mem_resolved)
    {
        this->output_debug_mem_resolved = true;
        this->output_debug_mem.assign(this->outputs.size(), nullptr);
        for (size_t i = 0; i < this->outputs.size(); i++)
        {
            std::vector<vp::SlavePort *> finals =
                this->outputs[i]->get_final_ports();
            if (!finals.empty() && finals[0]->get_owner() != nullptr)
            {
                this->output_debug_mem[i] = finals[0]->get_owner()->debug_mem_if();
            }
        }
    }

    // Split the access at output-select boundaries; within one chunk the
    // address is forwarded verbatim, exactly like the timed path.
    uint64_t mask = (1ULL << this->cfg.width) - 1;
    uint64_t select_granule = 1ULL << this->cfg.offset;
    while (size > 0)
    {
        int output_id = (int)((addr >> this->cfg.offset) & mask);
        uint64_t chunk = select_granule - (addr & (select_granule - 1));
        if (chunk > size)
        {
            chunk = size;
        }

        if (this->output_debug_mem[output_id] == nullptr ||
            this->output_debug_mem[output_id]->debug_mem_access(
                addr, data, chunk, is_write))
        {
            return -1;
        }

        addr += chunk;
        data += chunk;
        size -= chunk;
    }

    return 0;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Demux(config);
}
