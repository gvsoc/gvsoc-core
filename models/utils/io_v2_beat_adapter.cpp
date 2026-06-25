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

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, burst_id=%ld)\n",
        req, req->get_addr(), size, req->get_is_write() ? 1 : 0,
        (long)req->burst_id);

    // READ: the upstream beat master sends a single read descriptor carrying
    // the full burst size. Chop it into beat-sized sub-reads issued downstream
    // one per cycle, so the big-packet interconnect sees per-cycle beat
    // traffic; each completed sub-read emits one upstream beat.
    if (!req->get_is_write())
    {
        self->enqueue_read_burst(req);
        // Fire off as many sub-reads as the downstream will take right now
        // (bounded by max_sub_outstanding), so reads pipeline instead of
        // waiting one round-trip per beat.
        self->issue_pending_sub_reads();
        self->reschedule_fsm();
        return vp::IO_REQ_GRANTED;
    }

    // WRITE: forwarded as-is (the beat master already sends per-beat writes;
    // a big-packet write master sends one packet). Register the in-flight slot
    // before forwarding so the slave can respond inline or same-stack.
    uint64_t burst_addr = req->get_addr();
    auto inserted = self->in_flight.emplace(req, InFlight{size, 0, burst_addr});
    if (!inserted.second)
    {
        self->trace.force_warning(
            "Resubmit of in-flight req (req=%p) — resetting bookkeeping\n", req);
        inserted.first->second = InFlight{size, 0, burst_addr};
    }

    vp::IoReqStatus st = self->out.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Sync big-packet — req->data is already consumed. Synthesize the
        // per-beat resp() stream now so the upstream master sees the uniform
        // "GRANTED then resp() per beat" semantic.
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

    // Async completion of one of our in-flight read sub-reads? Mark it done,
    // drain any now-contiguous completed beats in order, and refill the
    // pipeline. (Searching is cheap: max_sub_outstanding is small.)
    for (auto &e : self->sub_inflight)
    {
        if (e.req == req)
        {
            e.completed = true;
            e.status    = req->get_resp_status();
            e.latency   = req->get_latency();
            self->drain_completed_sub_reads();
            self->issue_pending_sub_reads();
            self->reschedule_fsm();
            return;
        }
    }

    // Write path: the slave responds on the master's own req object.
    self->schedule_chunk(req, req->get_data(), req->get_size(),
                         req->get_latency());
}


void IoV2BeatAdapter::retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);

    // A held downstream read sub-request can now be re-issued. The io_v2
    // contract requires the re-send to happen synchronously inside retry().
    if (self->sub_read_denied)
    {
        self->sub_read_denied = false;
        self->issue_sub_read(self->denied_job);
        self->issue_pending_sub_reads();
        self->reschedule_fsm();
    }

    // Forward upstream too: the beat master may have its own DENIED writes to
    // re-send.
    self->in.retry(channel);
}


void IoV2BeatAdapter::enqueue_read_burst(vp::IoReq *req)
{
    uint64_t burst_addr = req->get_addr();
    uint8_t *data       = req->get_data();
    uint64_t total      = req->get_size();
    int64_t  burst_id   = req->burst_id;

    if (total == 0)
    {
        // Degenerate empty read: emit a single zero-size completion beat so the
        // upstream master still sees is_first=is_last on its burst.
        this->read_jobs.push_back(SubReadJob{
            req, 0, 0, burst_addr, data, true, true, burst_id});
        return;
    }

    for (uint64_t offset = 0; offset < total; offset += this->beat_width)
    {
        uint64_t beat = std::min<uint64_t>(total - offset,
                                           (uint64_t)this->beat_width);
        this->read_jobs.push_back(SubReadJob{
            req, offset, beat, burst_addr + offset, data + offset,
            offset == 0, offset + beat == total, burst_id});
    }
}


void IoV2BeatAdapter::issue_sub_read(const SubReadJob &job)
{
    // Each in-flight sub-read needs its own object so several can coexist
    // downstream. Freed when the entry is drained (in issue/offset order).
    vp::IoReq *r = new vp::IoReq();
    r->prepare();
    r->set_addr(job.addr);
    r->set_data(job.data);
    r->set_size(job.beat_bytes);
    r->set_is_write(false);
    r->is_first = true;
    r->is_last  = true;
    r->burst_id = -1;

    vp::IoReqStatus st = this->out.req(r);

    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream full: hold this job, re-issue on retry, issue nothing more
        // until then. The job was already popped from read_jobs by the caller,
        // so it lives in denied_job now and is not lost.
        this->sub_read_denied = true;
        this->denied_job = job;
        delete r;
        return;
    }

    // Track in issue order. GRANTED completes later in resp_handler; DONE is
    // already done — record it and let the in-order drain pick it up (it may
    // still have to wait behind an earlier not-yet-completed sub-read).
    this->sub_inflight.push_back(InflightSubRead{r, job, false,
                                                 vp::IO_RESP_OK, 0});
    if (st == vp::IO_REQ_DONE)
    {
        InflightSubRead &e = this->sub_inflight.back();
        e.completed = true;
        e.status    = r->get_resp_status();
        e.latency   = r->get_latency();
        this->drain_completed_sub_reads();
    }
}


void IoV2BeatAdapter::issue_pending_sub_reads()
{
    // Pace issuance at one sub-read per cycle (see read_issue_last_cycle): the
    // fsm re-ticks every cycle while read_jobs remain (reschedule_fsm), so the
    // rest drip out on subsequent cycles. This keeps a series of bandwidth
    // routers from compounding their per-request waits while still keeping
    // round-trip-many sub-reads outstanding.
    int64_t now = this->clock.get_cycles();
    if (now <= this->read_issue_last_cycle)
    {
        return;
    }
    if (this->sub_read_denied
        || (int)this->sub_inflight.size() >= this->max_sub_outstanding
        || this->read_jobs.empty())
    {
        return;
    }
    SubReadJob job = this->read_jobs.front();
    this->read_jobs.pop_front();
    this->read_issue_last_cycle = now;
    // issue_sub_read sets sub_read_denied on a downstream DENY (held in
    // denied_job and re-issued from retry_handler).
    this->issue_sub_read(job);
}


void IoV2BeatAdapter::drain_completed_sub_reads()
{
    // Emit beats strictly in issue (offset) order: only the head, and only
    // once it has completed, so a later sub-read that finished early cannot
    // make is_last reach the upstream master before earlier beats.
    while (!this->sub_inflight.empty() && this->sub_inflight.front().completed)
    {
        InflightSubRead e = this->sub_inflight.front();
        this->sub_inflight.pop_front();
        this->complete_sub_read(e.job, e.status, e.latency);
        delete e.req;
    }
}


void IoV2BeatAdapter::complete_sub_read(const SubReadJob &job,
                                        vp::IoRespStatus status,
                                        int64_t latency_cycles)
{
    int64_t now = this->clock.get_cycles();
    if (this->read_last_sched_cycle < now)
        this->read_last_sched_cycle = now;

    int64_t ready = now + std::max((int64_t)1, latency_cycles);
    if (ready <= this->read_last_sched_cycle)
        ready = this->read_last_sched_cycle + 1;
    this->read_last_sched_cycle = ready;

    this->pending.push_back(PendingBeat{
        job.up_req,
        job.data,
        job.beat_bytes,
        job.offset,
        job.addr,
        ready,
        job.is_first,
        job.is_last,
        status == vp::IO_RESP_INVALID ? vp::IO_RESP_INVALID : vp::IO_RESP_OK,
        job.burst_id,
    });

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Read beat ready (req=%p, offset=%lu, size=%lu, ready=%ld, first=%d, last=%d)\n",
        job.up_req, job.offset, job.beat_bytes, (long)ready,
        job.is_first ? 1 : 0, job.is_last ? 1 : 0);

    this->reschedule_fsm();
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

    // Serialise across calls: the slave may deliver a whole burst's response in
    // several chunks within the same cycle, but a beat channel carries at most
    // one beat per cycle. Keep beats strictly increasing in ready cycle. The
    // write channel has its own cursor so it does not serialise against reads.
    if (write_last_sched_cycle < now)
        write_last_sched_cycle = now;

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
        if (ready <= write_last_sched_cycle)
            ready = write_last_sched_cycle + 1;
        write_last_sched_cycle = ready;

        PendingBeat ev{
            req,
            data + cursor,
            beat,
            offset,
            inf.burst_addr + offset,
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
    // A response delivered as a single beat never aliases (one resp(), one
    // object, delivered once), so round-trip the master's own req — many
    // masters (e.g. a CPU LSU) key their completion on getting that exact
    // object back. Writes are also one-resp-per-req. Only a multi-beat read
    // splits one request into N deliveries and so needs a distinct object per
    // beat (below).
    if (ev.req->get_is_write() || (ev.is_first && ev.is_last))
    {
        vp::IoReq *req = ev.req;
        req->set_addr(ev.addr);
        req->set_data(ev.data);
        req->set_size(ev.size);
        req->burst_id = ev.burst_id;
        req->is_first = ev.is_first;
        req->is_last = ev.is_last;
        req->set_resp_status(ev.status);

        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Emit write ack (req=%p, offset=%lu, size=%lu, first=%d, last=%d)\n",
            req, ev.offset, ev.size, ev.is_first ? 1 : 0, ev.is_last ? 1 : 0);

        this->in.resp(req);
        return;
    }

    // Read response beat: a multi-beat read answers one burst with N beats, so
    // the beats cannot share the burst's req object (a downstream that queues a
    // response — e.g. a clock bridge — would alias the reused object). Each beat
    // is its own heap object, freed by the terminal master. The burst's own req
    // is owned by the adapter and freed on the last beat — the master must
    // therefore hand the adapter a freeable (heap-allocated) request, not a
    // pooled/borrowed object, and never gets it back.
    vp::IoReq *beat = new vp::IoReq();
    beat->set_addr(ev.addr);
    beat->set_data(ev.data);
    beat->set_size(ev.size);
    beat->set_is_write(false);
    beat->burst_id = ev.burst_id;
    beat->is_first = ev.is_first;
    beat->is_last = ev.is_last;
    beat->set_resp_status(ev.status);
    beat->initiator = ev.req->initiator;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit read beat (beat=%p, burst=%p, offset=%lu, size=%lu, first=%d, last=%d)\n",
        beat, ev.req, ev.offset, ev.size, ev.is_first ? 1 : 0, ev.is_last ? 1 : 0);

    this->in.resp(beat);

    if (ev.is_last)
    {
        delete ev.req;
    }
}


void IoV2BeatAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    // Emit any upstream beats that are due this cycle.
    while (!self->pending.empty() && self->pending.front().ready_cycle <= now)
    {
        PendingBeat ev = self->pending.front();
        self->pending.pop_front();
        self->emit_beat(ev);
    }

    // Refill the downstream read pipeline (a slot may have freed since the
    // last tick, or back-pressure may have cleared).
    self->issue_pending_sub_reads();

    self->reschedule_fsm();
}


void IoV2BeatAdapter::reschedule_fsm()
{
    if (this->fsm_event.is_enqueued())
    {
        return;
    }
    int64_t now = this->clock.get_cycles();
    int64_t next = INT64_MAX;
    if (!this->pending.empty())
    {
        next = std::min(next, this->pending.front().ready_cycle);
    }
    // A queued read sub-request that can still be issued (room in the
    // in-flight window and not held on back-pressure) wants the next cycle.
    if (!this->read_jobs.empty() && !this->sub_read_denied
        && (int)this->sub_inflight.size() < this->max_sub_outstanding)
    {
        next = std::min(next, now + 1);
    }
    if (next == INT64_MAX)
    {
        return;
    }
    this->fsm_event.enqueue(std::max(next - now, (int64_t)1));
}


vp::DebugMemIf *IoV2BeatAdapter::resolve_debug_mem()
{
    std::vector<vp::SlavePort *> finals = this->out.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}


int IoV2BeatAdapter::debug_mem_access(uint64_t addr, uint8_t *data,
                                      uint64_t size, bool is_write)
{
    vp::DebugMemIf *target = this->resolve_debug_mem();
    return target ? target->debug_mem_access(addr, data, size, is_write) : -1;
}


void IoV2BeatAdapter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *target = this->resolve_debug_mem();
    if (target != nullptr)
    {
        target->debug_mem_regions(regions, local_base, window_size, entry_base,
            depth + 1);
    }
}


void IoV2BeatAdapter::reset(bool active)
{
    if (active)
    {
        this->pending.clear();
        this->in_flight.clear();
        this->read_jobs.clear();
        for (auto &e : this->sub_inflight)
        {
            delete e.req;
        }
        this->sub_inflight.clear();
        this->sub_read_denied = false;
        this->read_last_sched_cycle = -1;
        this->write_last_sched_cycle = -1;
        this->read_issue_last_cycle = -1;
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
