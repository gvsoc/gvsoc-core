// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_adapter.hpp"

#include <algorithm>


IoV2BeatAdapter::IoV2BeatAdapter(vp::ComponentConf &config)
    : vp::Component(config),
      in(&IoV2BeatAdapter::req_handler),
      out(&IoV2BeatAdapter::retry_handler, &IoV2BeatAdapter::resp_handler),
      fsm_event(this, &IoV2BeatAdapter::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->beat_width = this->get_js_config()->get_child_int("beat_width");
    if (this->beat_width <= 0)
    {
        this->trace.fatal("IoV2BeatAdapter requires beat_width > 0 (got %d)\n",
                          this->beat_width);
    }

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}


vp::IoReqStatus IoV2BeatAdapter::req_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    uint64_t size = req->get_size();

    // Register the in-flight tracking slot before forwarding so the slave can
    // legitimately respond inline (DONE) or synchronously via a same-stack
    // resp() during the req() call.
    auto inserted = self->in_flight.emplace(req, InFlight{size, 0});
    if (!inserted.second)
    {
        self->trace.force_warning(
            "Resubmit of in-flight req (req=%p) — resetting bookkeeping\n", req);
        inserted.first->second = InFlight{size, 0};
    }

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, burst_id=%ld)\n",
        req, req->get_addr(), size, req->get_is_write() ? 1 : 0,
        (long)req->burst_id);

    vp::IoReqStatus st = self->out.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Sync big-packet — req->data is already filled for reads / consumed
        // for writes. Synthesize the per-beat resp() stream now so the
        // upstream master sees the uniform "GRANTED then resp() per beat"
        // semantic.
        self->schedule_chunk(req, req->get_data(), size, req->get_latency());
        return vp::IO_REQ_GRANTED;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        self->in_flight.erase(req);
    }
    return st;
}


void IoV2BeatAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    self->schedule_chunk(req, req->get_data(), req->get_size(),
                         req->get_latency());
}


void IoV2BeatAdapter::retry_handler(vp::Block *__this)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    self->in.retry();
}


void IoV2BeatAdapter::schedule_chunk(vp::IoReq *req, uint8_t *data,
                                     uint64_t size, int64_t latency_cycles)
{
    auto it = this->in_flight.find(req);
    if (it == this->in_flight.end())
    {
        this->trace.force_warning(
            "Response for unknown req (req=%p, size=%lu) — dropping\n",
            req, size);
        return;
    }
    InFlight &inf = it->second;

    int64_t now = this->clock.get_cycles();
    int64_t n = (int64_t)((size + this->beat_width - 1) / this->beat_width);
    if (n <= 0) n = 1;

    // Bandwidth model: the slave's latency annotation is the time-to-
    // completion of the *whole* chunk. Spread the n beats so the LAST one
    // lands at now+latency, with a 1-cycle minimum gap between beats. For
    // n == 1 this collapses to "beat at now+latency". For a wide chunk where
    // the slave's annotation already accounts for the bandwidth cost of
    // delivering all n beats (e.g. burst_duration = size / bandwidth), this
    // avoids double-counting via the per-beat spread.
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

        PendingBeat ev{
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


void IoV2BeatAdapter::emit_beat(const PendingBeat &ev)
{
    vp::IoReq *req = ev.req;
    req->set_data(ev.data);
    req->set_size(ev.size);
    req->burst_id = ev.burst_id;
    req->is_first = ev.is_first;
    req->is_last = ev.is_last;
    req->set_resp_status(ev.status);

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit beat (req=%p, offset=%lu, size=%lu, first=%d, last=%d)\n",
        req, ev.offset, ev.size, ev.is_first ? 1 : 0, ev.is_last ? 1 : 0);

    this->in.resp(req);
}


void IoV2BeatAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    while (!self->pending.empty() && self->pending.front().ready_cycle <= now)
    {
        PendingBeat ev = self->pending.front();
        self->pending.pop_front();
        self->emit_beat(ev);
    }

    if (!self->pending.empty())
    {
        int64_t d = self->pending.front().ready_cycle - now;
        self->fsm_event.enqueue(std::max(d, (int64_t)1));
    }
}


void IoV2BeatAdapter::reschedule_fsm()
{
    if (this->pending.empty() || this->fsm_event.is_enqueued())
    {
        return;
    }
    int64_t now = this->clock.get_cycles();
    int64_t delay = this->pending.front().ready_cycle - now;
    this->fsm_event.enqueue(std::max(delay, (int64_t)1));
}


void IoV2BeatAdapter::reset(bool active)
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


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatAdapter(config);
}
