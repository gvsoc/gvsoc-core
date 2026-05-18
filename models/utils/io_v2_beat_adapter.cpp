// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_adapter.hpp"

#include <algorithm>


BeatResponseAdapter::BeatResponseAdapter(vp::Block *parent, std::string name,
                                        int beat_width, Handler *handler)
    : vp::Block(parent, name),
      beat_width(beat_width),
      handler(handler),
      downstream(&BeatResponseAdapter::retry_handler,
                 &BeatResponseAdapter::resp_handler),
      fsm_event(this, &BeatResponseAdapter::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    if (beat_width <= 0)
    {
        this->trace.fatal("BeatResponseAdapter requires beat_width > 0 (got %d)\n", beat_width);
    }
}


vp::IoReqStatus BeatResponseAdapter::submit(vp::IoReq *req)
{
    uint64_t size = req->get_size();

    // Register the in-flight tracking slot before forwarding so the slave can
    // legitimately respond inline (DONE) or even synchronously via a same-stack
    // resp() during the req() call.
    auto inserted = this->in_flight.emplace(req, InFlight{size, 0});
    if (!inserted.second)
    {
        // The initiator resubmitted a request pointer that's already in flight
        // here. That's almost certainly a model bug; warn loudly and reset the
        // slot rather than silently corrupting beat accounting.
        this->trace.force_warning(
            "Resubmit of in-flight req (req=%p) — resetting bookkeeping\n", req);
        inserted.first->second = InFlight{size, 0};
    }

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, burst_id=%ld)\n",
        req, req->get_addr(), size, req->get_is_write() ? 1 : 0,
        (long)req->burst_id);

    vp::IoReqStatus st = this->downstream.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Sync big-packet — req->data is already filled for reads / consumed
        // for writes. Synthesize the per-beat callback stream now so the
        // initiator sees the uniform "GRANTED then on_beat" semantic.
        this->schedule_chunk(req, req->get_data(), size, req->get_latency());
        return vp::IO_REQ_GRANTED;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        this->in_flight.erase(req);
    }
    return st;
}


void BeatResponseAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<BeatResponseAdapter *>(__this);
    self->schedule_chunk(req, req->get_data(), req->get_size(),
                         req->get_latency());
}


void BeatResponseAdapter::retry_handler(vp::Block *__this)
{
    auto *self = static_cast<BeatResponseAdapter *>(__this);
    self->handler->on_retry();
}


void BeatResponseAdapter::schedule_chunk(vp::IoReq *req, uint8_t *data,
                                        uint64_t size, int64_t latency_cycles)
{
    auto it = this->in_flight.find(req);
    if (it == this->in_flight.end())
    {
        // Either the request was already retired (cumulative bytes complete and
        // the entry erased) or the slave is responding to a request the adapter
        // never accepted. In both cases there's nothing legitimate we can do —
        // warn and drop the chunk.
        this->trace.force_warning(
            "Response for unknown req (req=%p, size=%lu) — dropping\n",
            req, size);
        return;
    }
    InFlight &inf = it->second;

    int64_t now = this->clock.get_cycles();
    // Number of beats this chunk will produce.
    int64_t n = (int64_t)((size + this->beat_width - 1) / this->beat_width);
    if (n <= 0) n = 1;

    // Bandwidth model: the slave's latency annotation is the time-to-
    // completion of the *whole* chunk. The adapter spreads the n beats so
    // that the LAST one lands at now+latency, with a minimum 1-cycle gap
    // between beats. For a single-beat chunk (n == 1, the legacy per-req
    // case) this is just "beat at now+latency". For a wide chunk where the
    // slave's annotation already accounts for the bandwidth-imposed cost of
    // delivering all n beats (e.g. router_v2_bandwidth's
    // burst_duration = size / bandwidth), this avoids double-counting the
    // per-beat spread.
    int64_t step = (n > 1) ? std::max((int64_t)1, latency_cycles / n) : (int64_t)1;
    int64_t first_ready = now + latency_cycles - (n - 1) * step;
    if (first_ready < now + 1) first_ready = now + 1;

    uint64_t cursor = 0;
    int64_t beat_idx = 0;
    while (cursor < size)
    {
        uint64_t beat = std::min<uint64_t>(size - cursor,
                                           (uint64_t)this->beat_width);
        uint64_t offset = inf.bytes_routed + cursor;
        bool is_first = (offset == 0);
        bool is_last  = (offset + beat == inf.total_size);

        int64_t ready = first_ready + beat_idx * step;

        BeatEvent ev{
            req,
            data + cursor,
            beat,
            offset,
            ready,
            is_first,
            is_last,
            req->get_resp_status(),
            req->burst_id,
        };
        this->pending.push_back(ev);

        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Schedule beat (req=%p, offset=%lu, size=%lu, ready=%ld, first=%d, last=%d)\n",
            req, offset, beat, (long)ready,
            is_first ? 1 : 0, is_last ? 1 : 0);

        cursor += beat;
        beat_idx++;
    }
    inf.bytes_routed += size;

    if (inf.bytes_routed >= inf.total_size)
    {
        if (inf.bytes_routed > inf.total_size)
        {
            this->trace.force_warning(
                "Slave responded with more bytes than requested (req=%p, total=%lu, got=%lu)\n",
                req, inf.total_size, inf.bytes_routed);
        }
        this->in_flight.erase(it);
    }

    this->reschedule_fsm();
}


void BeatResponseAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<BeatResponseAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    while (!self->pending.empty() && self->pending.front().ready_cycle <= now)
    {
        BeatEvent ev = self->pending.front();
        self->pending.pop_front();
        self->handler->on_beat(ev);
    }

    if (!self->pending.empty())
    {
        int64_t d = self->pending.front().ready_cycle - now;
        self->fsm_event.enqueue(std::max(d, (int64_t)1));
    }
}


void BeatResponseAdapter::reschedule_fsm()
{
    if (this->pending.empty() || this->fsm_event.is_enqueued())
    {
        return;
    }
    int64_t now = this->clock.get_cycles();
    int64_t delay = this->pending.front().ready_cycle - now;
    this->fsm_event.enqueue(std::max(delay, (int64_t)1));
}


void BeatResponseAdapter::reset(bool active)
{
    if (active)
    {
        this->pending.clear();
        this->in_flight.clear();
        if (this->fsm_event.is_enqueued())
        {
            this->fsm_event.cancel();
        }
    }
}
