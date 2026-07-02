// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2SingleReq stub target for IoV2BeatToSingleReqAdapter. Answers each request
 * with a SINGLE-BEAT response in one of three forms:
 *   - inline IO_REQ_DONE (default),
 *   - async IO_REQ_GRANTED + exactly one resp() (async_resp=true), or
 *   - IO_REQ_DENIED + retry() for the first `deny_count` requests (request-path
 *     back-pressure).
 * It never streams a multi-beat response — that is the IoV2SingleReq contract
 * the adapter checks in asserts builds.
 *
 * Read data is filled with an addr-derived pattern; latency (and optionally
 * duration) is annotated, and IO_RESP_INVALID can be reported. It POLICES the
 * adapter with traces.assert: every request it receives is a single beat
 * (is_first && is_last — the adapter chops reads into beat-sized sub-reads and
 * forwards whole writes), addressed within range.
 *
 * Config keys: latency, duration, error(bool), async_resp(bool), deny_count,
 * retry_delay, base, size, logname.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <deque>
#include <string>

class StubTarget : public vp::Component
{
public:
    StubTarget(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void resp_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void retry_event_handler(vp::Block *__this, vp::ClockEvent *event);

    void serve(vp::IoReq *req);

    struct Pending { vp::IoReq *req; int64_t resp_cycle; };

    vp::IoSlave in;
    vp::ClockEvent resp_event;
    vp::ClockEvent retry_event;
    vp::Trace trace;
    std::string logname;
    int64_t latency = 0;
    int64_t duration = 0;
    bool error = false;
    bool async_resp = false;
    int deny_count_cfg = 0;
    int deny_remaining = 0;
    int retry_delay = 2;
    bool reorder_resp = false;
    uint64_t base = 0;
    uint64_t size = 0;
    std::deque<Pending> pending;   // outstanding async responses, in FIFO order
    vp::IoReq *swap_held = nullptr; // reorder_resp: first request of a pair
};

StubTarget::StubTarget(vp::ComponentConf &config)
    : vp::Component(config),
      in(&StubTarget::req_handler),
      resp_event(this, &StubTarget::resp_event_handler),
      retry_event(this, &StubTarget::retry_event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();
    this->latency = this->get_js_config()->get_child_int("latency");
    this->duration = this->get_js_config()->get_child_int("duration");
    this->error = this->get_js_config()->get_child_bool("error");
    this->async_resp = this->get_js_config()->get_child_bool("async_resp");
    this->deny_count_cfg = (int)this->get_js_config()->get_child_int("deny_count");
    this->retry_delay = (int)this->get_js_config()->get_child_int("retry_delay");
    if (this->retry_delay <= 0) this->retry_delay = 2;
    this->reorder_resp = this->get_js_config()->get_child_bool("reorder_resp");
    this->base = (uint64_t)this->get_js_config()->get_child_int("base");
    this->size = (uint64_t)this->get_js_config()->get_child_int("size");
}

void StubTarget::reset(bool active)
{
    if (active)
    {
        this->pending.clear();
        this->deny_remaining = this->deny_count_cfg;
        this->swap_held = nullptr;
        this->resp_event.cancel();
        this->retry_event.cancel();
    }
}

void StubTarget::serve(vp::IoReq *req)
{
    uint64_t addr = req->get_addr();
    uint64_t sz = req->get_size();
    if (!req->get_is_write())
    {
        uint8_t *d = req->get_data();
        for (uint64_t i = 0; i < sz; i++) d[i] = (uint8_t)((addr + i) & 0xff);
    }
    req->inc_latency(this->latency);
    if (this->duration > 0) req->set_duration(this->duration);
    req->set_resp_status(this->error ? vp::IO_RESP_INVALID : vp::IO_RESP_OK);
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

    // ---- Protocol assertions (the adapter behaves correctly) ----
    // The adapter chops reads into beat-sized sub-reads and forwards writes as a
    // single request, so a single-req slave always sees one single-beat access.
    _this->traces.assert(req->is_first && req->is_last,
        "single-req slave must receive single-beat requests (first=%d last=%d)",
        req->is_first ? 1 : 0, req->is_last ? 1 : 0);
    _this->traces.assert(addr >= _this->base && addr + sz <= _this->base + _this->size,
        "access [0x%lx,+%lu) outside target range [0x%lx,+%lu)",
        addr, sz, _this->base, _this->size);

    // ---- Request-path back-pressure: deny the first deny_count requests ----
    // The adapter must hold the request and re-send it (synchronously) when we
    // call in.retry() below; the re-send is then served normally.
    if (_this->deny_remaining > 0)
    {
        _this->deny_remaining--;
        printf("[%ld] %s DENY addr=0x%lx (retry in %d)\n",
            now, _this->logname.c_str(), addr, _this->retry_delay);
        _this->retry_event.enqueue(_this->retry_delay);
        return vp::IO_REQ_DENIED;
    }

    _this->serve(req);

    // Inline completion: the response (data + status + latency) is already in req.
    if (!_this->async_resp)
    {
        return vp::IO_REQ_DONE;
    }

    // Negative-test path: deliberately answer out of issue order by swapping
    // adjacent pairs. Stash the first request of each pair; when its partner
    // arrives, schedule the SECOND-arrived one to respond first and the first
    // one second — proving the adapter's in-order assert fires.
    if (_this->reorder_resp)
    {
        if (_this->swap_held == nullptr)
        {
            _this->swap_held = req;     // wait for the partner before responding
            return vp::IO_REQ_GRANTED;
        }
        vp::IoReq *first = _this->swap_held;
        _this->swap_held = nullptr;
        _this->pending.push_back({req,   now + 1});   // 2nd-arrived → earlier resp
        _this->pending.push_back({first, now + 2});   // 1st-arrived → later resp
        _this->resp_event.enqueue(1);
        return vp::IO_REQ_GRANTED;
    }

    // Async completion: take ownership and deliver exactly one resp() later.
    int64_t resp_cycle = now + 1;
    _this->pending.push_back({req, resp_cycle});
    // enqueue() keeps the earliest pending cycle if already enqueued.
    _this->resp_event.enqueue(resp_cycle - now);
    return vp::IO_REQ_GRANTED;
}

void StubTarget::resp_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();

    while (!_this->pending.empty() && _this->pending.front().resp_cycle <= now)
    {
        vp::IoReq *req = _this->pending.front().req;
        _this->pending.pop_front();
        printf("[%ld] %s RESP addr=0x%lx size=%lu status=%d\n",
            now, _this->logname.c_str(), req->get_addr(),
            (unsigned long)req->get_size(), (int)req->get_resp_status());
        // The adapter buffers and never back-pressures the downstream response.
        _this->traces.assert(_this->in.resp(req) == vp::IO_RESP_ACCEPTED,
            "adapter must accept the single-req slave's resp()");
    }

    if (!_this->pending.empty())
    {
        _this->resp_event.enqueue(_this->pending.front().resp_cycle - now);
    }
}

void StubTarget::retry_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s RETRY\n", now, _this->logname.c_str());
    // Tell the adapter (our master) it can re-send; per the io_v2 contract it
    // re-issues the held request synchronously inside this call.
    _this->in.retry();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubTarget(config);
}
