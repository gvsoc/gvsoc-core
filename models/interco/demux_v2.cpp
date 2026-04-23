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
#include <interco/demux_config.hpp>

class Demux : public vp::Component
{
public:
    Demux(vp::ComponentConf &conf);
    void reset(bool active) override;

    DemuxConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id);

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
};


Demux::Demux(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
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


void Demux::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    Demux *_this = (Demux *)__this;
    // Stateless forward: reply on the single upstream port. The request
    // object itself carries any latency the downstream annotated, so there
    // is nothing to add here.
    _this->input_itf.resp(req);
}


void Demux::output_retry(vp::Block *__this, int id)
{
    Demux *_this = (Demux *)__this;
    if (_this->denied_output == id)
    {
        _this->denied_output = -1;
        _this->input_itf.retry();
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Demux(config);
}
