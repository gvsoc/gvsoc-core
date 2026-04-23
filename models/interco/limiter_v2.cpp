// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Bandwidth-shaping limiter on the io_v2 protocol.
 *
 * Direct port of limiter.cpp to io_v2. Scope and behaviour are unchanged; only
 * the IO-side plumbing differs:
 *
 *   - Single-master slave port; replies via `input_itf.resp(req)` on our own
 *     slave port (no v1 `resp_port` indirection).
 *   - Status codes: IO_REQ_DONE / IO_REQ_GRANTED / IO_REQ_DENIED, with error
 *     reporting via `req->set_resp_status(IO_RESP_INVALID) + IO_REQ_DONE`.
 *   - No arg stack: the parent pointer on each emitted sub-request links back
 *     to the CPU request, and per-parent state (remaining response bytes) is
 *     kept in `parent->remaining_size`.
 *   - Deny/retry handshake: upstream DENY replaces the v1 grant-on-accept
 *     protocol (see ``input_needs_retry``); downstream DENY is parked in
 *     ``denied_sub`` and replayed from the output's retry callback.
 *
 * Model-level behaviour (unchanged from v1):
 *   - Only one CPU request may be *in emission* at a time. While a CPU request
 *     still has bytes left to emit, any new CPU request is denied.
 *   - The in-emission request is split into one sub-request per cycle, each of
 *     at most `bandwidth` bytes. Sub-requests are forwarded through the output
 *     master port.
 *   - Multiple *responses* may be in flight simultaneously: once all bytes of a
 *     CPU request have been emitted we free the emission slot (accepting new
 *     CPU requests) even if some sub-response chunks are still outstanding.
 *     Each sub-request carries a `parent` pointer and decrements
 *     `parent->remaining_size`; when the counter reaches zero the CPU request
 *     is completed.
 *
 * Timing:
 *   - Emission is paced by a single ClockEvent that re-arms every cycle while
 *     there are bytes left to send. One chunk goes out per cycle.
 *   - Latency is not annotated on the CPU request — the time to complete is
 *     simply the wall-clock time at which the last sub-response lands (plus
 *     whatever latency the downstream itself annotated on each chunk).
 */

#include <algorithm>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/signal.hpp>
#include <interco/limiter_config.hpp>

class Limiter : public vp::Component
{
public:
    Limiter(vp::ComponentConf &conf);
    void reset(bool active) override;

    LimiterConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req);
    static void            output_retry(vp::Block *__this);
    static void            event_handler(vp::Block *__this, vp::ClockEvent *event);

    // Internal helpers.
    vp::IoReq *alloc_sub();
    void       free_sub(vp::IoReq *sub);
    void       finish_chunk(vp::IoReq *sub);
    void       check_last_chunk(vp::IoReq *parent);

    vp::Trace trace;

    // io_v2 ports. Method pointers are passed at construction.
    vp::IoSlave  input_itf{&Limiter::input_req};
    vp::IoMaster output_itf{&Limiter::output_retry, &Limiter::output_resp};

    // Emission pacing event.
    vp::ClockEvent event;

    // Free list of sub-request objects. Linked via the ``next`` field on IoReq.
    vp::IoReq *free_list = nullptr;

    // Current CPU request being emitted. Cleared as soon as its last byte has
    // been sent (responses may still be outstanding — tracked via the parent's
    // own ``remaining_size`` counter).
    vp::IoReq *pending_req  = nullptr;
    uint64_t   pending_size = 0;
    uint64_t   pending_addr = 0;
    uint8_t   *pending_data = nullptr;

    // Set when we denied an upstream CPU request. Cleared — and input_itf.retry()
    // fired — as soon as we are free again.
    bool input_needs_retry = false;

    // Sub-request that was DENIED by the output and must be replayed from
    // output_retry. Nullptr when we are not stalled.
    vp::IoReq *denied_sub = nullptr;

    // Earliest cycle at which the next chunk may be emitted. A signal so the
    // current value shows up in the trace database.
    vp::Signal<int64_t> next_req_timestamp;
};


Limiter::Limiter(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      event(this, &Limiter::event_handler),
      next_req_timestamp(*this, "next_req_timestamp", 64,
                         vp::SignalCommon::ResetKind::Value, 0)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input",   &this->input_itf);
    this->new_master_port("output", &this->output_itf);
}

void Limiter::reset(bool active)
{
    if (active)
    {
        this->pending_req       = nullptr;
        this->pending_size      = 0;
        this->pending_addr      = 0;
        this->pending_data      = nullptr;
        this->input_needs_retry = false;
        this->denied_sub        = nullptr;
    }
}


vp::IoReq *Limiter::alloc_sub()
{
    if (this->free_list == nullptr)
    {
        return new vp::IoReq();
    }
    vp::IoReq *sub = this->free_list;
    this->free_list = sub->get_next();
    return sub;
}

void Limiter::free_sub(vp::IoReq *sub)
{
    sub->set_next(this->free_list);
    this->free_list = sub;
}


vp::IoReqStatus Limiter::input_req(vp::Block *__this, vp::IoReq *req)
{
    Limiter *_this = (Limiter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        req, (unsigned long long)req->get_addr(),
        (unsigned long long)req->get_size(), req->get_is_write() ? 1 : 0);

    // One CPU request in emission at a time.
    if (_this->pending_req != nullptr)
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    _this->pending_req  = req;
    _this->pending_size = req->get_size();
    _this->pending_addr = req->get_addr();
    _this->pending_data = req->get_data();

    // Track outstanding response bytes on the parent itself — the sub-request's
    // ``parent`` pointer will be enough to complete the CPU request from the
    // response path.
    req->remaining_size = req->get_size();
    req->set_resp_status(vp::IO_RESP_OK);

    if (!_this->event.is_enqueued())
    {
        _this->event.enqueue(1);
    }

    return vp::IO_REQ_GRANTED;
}


void Limiter::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Limiter *_this = (Limiter *)__this;

    // If the output is still stalled from a previous DENY, wait for retry.
    if (_this->denied_sub != nullptr)
    {
        return;
    }

    // Nothing left to emit — nothing to do. The event is not re-armed here.
    if (_this->pending_req == nullptr || _this->pending_size == 0)
    {
        return;
    }

    int64_t now = _this->clock.get_cycles();
    if (_this->next_req_timestamp.get() > now)
    {
        // Respect the pacing watermark (set by the previous emission). Normally
        // we will never be more than one cycle ahead, so this is just a guard.
        _this->event.enqueue(_this->next_req_timestamp.get() - now);
        return;
    }

    uint64_t chunk = std::min(_this->pending_size,
                              (uint64_t)_this->cfg.bandwidth);

    vp::IoReq *sub = _this->alloc_sub();
    sub->prepare();
    sub->parent      = _this->pending_req;
    sub->set_addr(_this->pending_addr);
    sub->set_size(chunk);
    sub->set_data(_this->pending_data);
    sub->set_opcode(_this->pending_req->get_opcode());
    sub->set_resp_status(vp::IO_RESP_OK);

    _this->pending_addr += chunk;
    _this->pending_data += chunk;
    _this->pending_size -= chunk;
    _this->next_req_timestamp.set(now + 1);

    // The CPU request is free to be replaced as soon as its last chunk is
    // emitted — responses track completion via parent->remaining_size.
    if (_this->pending_size == 0)
    {
        _this->pending_req = nullptr;
    }

    vp::IoReqStatus st = _this->output_itf.req(sub);

    if (st == vp::IO_REQ_DONE)
    {
        _this->finish_chunk(sub);
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Hold the chunk until the output retries. Neither ``pending_req`` nor
        // ``pending_size`` rolls back — this chunk has already "left" emission;
        // only its forward attempt failed. The retry path re-sends the exact
        // same ``sub``.
        _this->denied_sub = sub;
        return;
    }
    // IO_REQ_GRANTED: response will come later via output_resp.

    // If more chunks remain, re-arm for the next cycle. If this was the last
    // chunk of the current CPU request, and another one was denied earlier,
    // fire retry() immediately so the master can push a new one. The master
    // will hit us with a fresh req on the next cycle, which will re-arm the
    // event itself.
    if (_this->pending_req != nullptr && _this->pending_size > 0)
    {
        _this->event.enqueue(1);
    }
    else if (_this->input_needs_retry)
    {
        _this->input_needs_retry = false;
        _this->input_itf.retry();
    }
}


void Limiter::finish_chunk(vp::IoReq *sub)
{
    vp::IoReq *parent = sub->parent;

    // Propagate per-chunk errors to the parent. We latch the first INVALID and
    // keep it until the parent's last response lands.
    if (sub->get_resp_status() == vp::IO_RESP_INVALID)
    {
        parent->set_resp_status(vp::IO_RESP_INVALID);
    }
    parent->remaining_size -= sub->get_size();

    this->free_sub(sub);

    this->check_last_chunk(parent);
}


void Limiter::check_last_chunk(vp::IoReq *parent)
{
    if (parent->remaining_size != 0)
    {
        return;
    }
    // All responses are in. Reply upstream and — if we owe a retry for a
    // previously denied CPU request — fire it now.
    this->input_itf.resp(parent);

    if (this->pending_req == nullptr && this->input_needs_retry)
    {
        this->input_needs_retry = false;
        this->input_itf.retry();
    }
}


void Limiter::output_resp(vp::Block *__this, vp::IoReq *sub)
{
    Limiter *_this = (Limiter *)__this;
    _this->finish_chunk(sub);
}


void Limiter::output_retry(vp::Block *__this)
{
    Limiter *_this = (Limiter *)__this;

    if (_this->denied_sub == nullptr)
    {
        return;
    }

    vp::IoReq *sub = _this->denied_sub;
    _this->denied_sub = nullptr;

    vp::IoReqStatus st = _this->output_itf.req(sub);
    if (st == vp::IO_REQ_DONE)
    {
        _this->finish_chunk(sub);
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Still stalled. Keep holding the chunk for the next retry.
        _this->denied_sub = sub;
        return;
    }
    // IO_REQ_GRANTED: response will come later via output_resp.

    // If more chunks still need to be emitted, resume pacing from the next
    // cycle. If the denied chunk was the last one, the event is left idle and
    // everything is driven from the response path.
    if (_this->pending_req != nullptr && _this->pending_size > 0
        && !_this->event.is_enqueued())
    {
        _this->event.enqueue(1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Limiter(config);
}
