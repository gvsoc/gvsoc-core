// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2Sync stub target for IoV2BeatToSyncAdapter. Always completes inline with
 * IO_REQ_DONE (the sync contract): fills read data with an addr-derived
 * pattern, annotates latency (and optionally duration), and optionally reports
 * IO_RESP_INVALID. It POLICES the adapter/initiator with traces.assert: each
 * request is a single whole-burst forward (is_first && is_last), addressed
 * within range, and never re-entrant (a sync slave is never pipelined).
 *
 * Config keys: latency, duration, error(bool), base, size, logname.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <string>

class StubTarget : public vp::Component
{
public:
    StubTarget(vp::ComponentConf &conf);

private:
    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);

    vp::IoSlave in;
    vp::Trace trace;
    std::string logname;
    int64_t latency = 0;
    int64_t duration = 0;
    bool error = false;
    uint64_t base = 0;
    uint64_t size = 0;
    bool in_call = false;
};

StubTarget::StubTarget(vp::ComponentConf &config)
    : vp::Component(config),
      in(&StubTarget::req_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();
    this->latency = this->get_js_config()->get_child_int("latency");
    this->duration = this->get_js_config()->get_child_int("duration");
    this->error = this->get_js_config()->get_child_bool("error");
    this->base = (uint64_t)this->get_js_config()->get_child_int("base");
    this->size = (uint64_t)this->get_js_config()->get_child_int("size");
}

vp::IoReqStatus StubTarget::req_handler(vp::Block *__this, vp::IoReq *req)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();
    uint64_t addr = req->get_addr();
    uint64_t sz = req->get_size();

    printf("[%ld] %s REQ addr=0x%lx size=%lu write=%d first=%d last=%d burst_id=%ld\n",
        now, _this->logname.c_str(), addr, sz, req->get_is_write() ? 1 : 0,
        req->is_first ? 1 : 0, req->is_last ? 1 : 0, (long)req->burst_id);

    // ---- Protocol assertions (the adapter / initiator behaves correctly) ----
    // A sync slave is never pipelined: every request completes inline, so a
    // second request can never arrive mid-flight.
    _this->traces.assert(!_this->in_call,
        "sync slave received a re-entrant request");
    _this->in_call = true;
    // The beat->sync adapter forwards the whole burst as one request.
    _this->traces.assert(req->is_first && req->is_last,
        "sync slave must receive a single whole-burst request (first=%d last=%d)",
        req->is_first ? 1 : 0, req->is_last ? 1 : 0);
    _this->traces.assert(addr >= _this->base && addr + sz <= _this->base + _this->size,
        "access [0x%lx,+%lu) outside target range [0x%lx,+%lu)",
        addr, sz, _this->base, _this->size);

    // Serve inline.
    if (!req->get_is_write())
    {
        uint8_t *d = req->get_data();
        for (uint64_t i = 0; i < sz; i++) d[i] = (uint8_t)((addr + i) & 0xff);
    }
    req->inc_latency(_this->latency);
    if (_this->duration > 0) req->set_duration(_this->duration);
    req->set_resp_status(_this->error ? vp::IO_RESP_INVALID : vp::IO_RESP_OK);

    _this->in_call = false;
    return vp::IO_REQ_DONE;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubTarget(config);
}
