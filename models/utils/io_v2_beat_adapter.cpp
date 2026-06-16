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
        // Kick off the first sub-read inline, mirroring the write path's
        // out.req() in req_handler: a single-beat read then completes with the
        // same timing as an equivalent write (no extra FSM fill cycle). The
        // FSM / resp-chain pace the remaining beats one per cycle. Only the
        // upstream resp() is deferred to the FSM, never a reentrant in.resp().
        if (!self->sub_read_outstanding && !self->sub_read_denied
            && !self->read_jobs.empty())
        {
            SubReadJob job = self->read_jobs.front();
            self->read_jobs.pop_front();
            self->issue_sub_read(job);
        }
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

    // Async completion of one of our downstream read sub-requests.
    if (req == &self->sub_req)
    {
        self->sub_read_outstanding = false;
        self->complete_sub_read(self->current_job, req->get_resp_status(),
                                req->get_latency());
        // Issue the next sub-read right now (not via a +1 FSM re-arm): a
        // multi-cycle downstream just freed up this cycle, so the next sub-read
        // can start immediately and reads pace at the downstream completion
        // rate (matching the write path), with no extra idle cycle per beat.
        // If the downstream is busy it DENIES and we wait for its retry; if it
        // completes inline the FSM picks up the rest at one per cycle.
        if (!self->sub_read_outstanding && !self->sub_read_denied
            && !self->read_jobs.empty())
        {
            SubReadJob job = self->read_jobs.front();
            self->read_jobs.pop_front();
            self->issue_sub_read(job);
        }
        self->reschedule_fsm();
        return;
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
        self->issue_sub_read(self->current_job);
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
    this->current_job = job;

    this->sub_req.prepare();
    this->sub_req.set_addr(job.addr);
    this->sub_req.set_data(job.data);
    this->sub_req.set_size(job.beat_bytes);
    this->sub_req.set_is_write(false);
    // prepare() does not reset these — set them explicitly on every reuse.
    this->sub_req.is_first = true;
    this->sub_req.is_last  = true;
    this->sub_req.burst_id = -1;

    this->sub_read_outstanding = true;

    vp::IoReqStatus st = this->out.req(&this->sub_req);

    if (st == vp::IO_REQ_DONE)
    {
        this->sub_read_outstanding = false;
        this->complete_sub_read(job, this->sub_req.get_resp_status(),
                                this->sub_req.get_latency());
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Hold this job and stop issuing until the slave retries.
        this->sub_read_outstanding = false;
        this->sub_read_denied = true;
    }
    // IO_REQ_GRANTED: stays outstanding; resp_handler will complete it.
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
    vp::IoReq *req = ev.req;
    req->set_addr(ev.addr);
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

    // Emit any upstream beats that are due this cycle.
    while (!self->pending.empty() && self->pending.front().ready_cycle <= now)
    {
        PendingBeat ev = self->pending.front();
        self->pending.pop_front();
        self->emit_beat(ev);
    }

    // Issue at most one downstream read sub-request this cycle, so the
    // big-packet interconnect sees per-cycle beat traffic. An inline DONE
    // schedules its upstream beat for a later cycle (>= now+1), keeping the
    // upstream stream at one beat per cycle too.
    if (!self->sub_read_outstanding && !self->sub_read_denied
        && !self->read_jobs.empty())
    {
        SubReadJob job = self->read_jobs.front();
        self->read_jobs.pop_front();
        self->issue_sub_read(job);
    }

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
    // A queued read sub-request that can be issued (no sub-read in flight or
    // held) wants the next cycle.
    if (!this->read_jobs.empty() && !this->sub_read_outstanding
        && !this->sub_read_denied)
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
        this->sub_read_outstanding = false;
        this->sub_read_denied = false;
        this->read_last_sched_cycle = -1;
        this->write_last_sched_cycle = -1;
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
