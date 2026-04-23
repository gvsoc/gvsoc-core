// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Window-based address remapper on the io_v2 protocol.
//
// Port of remapper.cpp to io_v2. Functionally unchanged: requests whose
// address falls in ``[base, base + size)`` have their address rewritten to
// ``target_base + (addr - base)``; all other requests are forwarded with
// the address untouched. No data is buffered, no latency is added.
//
// What changed vs v1:
//   - Single-master slave port; replies travel back via
//     ``input_itf.resp(req)`` rather than v1's ``req->resp_port`` lookup.
//   - Status codes are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``.
//   - AXI-like deny/retry handshake. When the downstream DENIEs we
//     restore the request's original address (so the master's re-send
//     looks exactly like the first attempt) and set ``input_needs_retry``.
//     The flag is cleared when the output fires ``retry()``, at which
//     point we propagate ``retry()`` on the input.
//
// Timing model: big-packet, zero-latency. Whatever latency the downstream
// reports (via ``req->latency`` on a sync ``IO_REQ_DONE`` or via when it
// schedules ``resp()`` on an async path) is observed directly by the
// upstream master — the remapper adds no cycle.

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <interco/remapper_config.hpp>

class Remapper : public vp::Component
{
public:
    Remapper(vp::ComponentConf &conf);
    void reset(bool active) override;

    RemapperConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req);
    static void            output_retry(vp::Block *__this);

    vp::Trace trace;

    vp::IoSlave  input_itf{&Remapper::input_req};
    vp::IoMaster output_itf{&Remapper::output_retry, &Remapper::output_resp};

    // True when we have returned IO_REQ_DENIED upstream and owe an
    // ``input_itf.retry()`` as soon as the downstream recovers. At most one
    // such pending retry can be outstanding because the single upstream
    // master port can only hold one denied request.
    bool input_needs_retry = false;
};


Remapper::Remapper(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input",   &this->input_itf);
    this->new_master_port("output", &this->output_itf);
}

void Remapper::reset(bool active)
{
    if (active)
    {
        this->input_needs_retry = false;
    }
}


vp::IoReqStatus Remapper::input_req(vp::Block *__this, vp::IoReq *req)
{
    Remapper *_this = (Remapper *)__this;

    uint64_t addr = req->get_addr();
    uint64_t base = (uint64_t)_this->cfg.base;
    uint64_t size = (uint64_t)_this->cfg.size;

    // The window test is deliberately half-open: `base <= addr < base + size`.
    // With size == 0 no address is ever "inside", which is the expected
    // behaviour for a disabled remap.
    bool in_window = (size > 0) && (addr >= base) && (addr < base + size);

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "IO req (req: %p, addr: 0x%llx, size: 0x%llx, write: %d, in_window: %d)\n",
        req, (unsigned long long)addr,
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0, in_window ? 1 : 0);

    if (in_window)
    {
        req->set_addr(addr - base + (uint64_t)_this->cfg.target_base);
    }

    vp::IoReqStatus st = _this->output_itf.req(req);

    if (st == vp::IO_REQ_DENIED)
    {
        // Nothing went through — restore the original address so that when
        // the upstream master retries, its request looks byte-for-byte like
        // the first attempt. Mark that we owe an upstream retry once our
        // own output port retries.
        if (in_window)
        {
            req->set_addr(addr);
        }
        _this->input_needs_retry = true;
    }
    return st;
}


void Remapper::output_resp(vp::Block *__this, vp::IoReq *req)
{
    Remapper *_this = (Remapper *)__this;
    // Stateless forward. The request object still carries whatever address
    // the downstream saw (rewritten if we mapped it); we do not restore it
    // on DONE/RESP because the upstream master typically does not re-read
    // req->addr after completion.
    _this->input_itf.resp(req);
}


void Remapper::output_retry(vp::Block *__this)
{
    Remapper *_this = (Remapper *)__this;
    if (_this->input_needs_retry)
    {
        _this->input_needs_retry = false;
        _this->input_itf.retry();
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Remapper(config);
}
