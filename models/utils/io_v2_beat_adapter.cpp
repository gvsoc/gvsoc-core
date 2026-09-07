// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_adapter.hpp"

#include <algorithm>
#include <cstring>
#include <set>


IoV2BeatAdapter::IoV2BeatAdapter(vp::ComponentConf &config)
    : vp::Component(config),
      in(&IoV2BeatAdapter::req_handler, &IoV2BeatAdapter::resp_retry_in_handler),
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

    this->beat_allocator = vp::IoReqAllocator::get(this->beat_width);

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

    // ATOMICS (get_is_write() is true for every non-READ opcode, but only
    // opcode == WRITE follows the per-burst ack contract): they need response
    // data back in the master's own object, so they keep the classic
    // round-trip — forward as-is and relay the completion as a single
    // upstream resp() of the same object. Nobody frees the request.
    if (req->get_opcode() != vp::WRITE)
    {
        vp::IoReqStatus st = self->out.req(req);
        if (st == vp::IO_REQ_DONE)
        {
            // Relay the inline completion as an async single resp at
            // now + full_latency, through the pending queue (write cursor),
            // so the upstream master never sees a raw DONE.
            self->schedule_atomic(req, req->get_full_latency());
            return vp::IO_REQ_GRANTED;
        }
        return st;
    }

    // WRITE: forwarded as-is (the downstream big-packet plane round-trips the
    // object and never frees it). Under the per-burst ack contract the
    // adapter is the consumer of the master's allocator-backed write beats:
    // once a beat's downstream leg completes it is freed (or parked for
    // recycling as the ack if it is the burst's last beat), and the burst is
    // acknowledged upstream exactly once.
    uint64_t burst_addr = req->get_addr();
    bool beat_last = req->is_last;

    self->traces.assert(req->allocator != nullptr,
        "write beat is not allocator-backed (req=%p) — unported master", req);

    if (req->is_first || self->open_wr_burst == nullptr)
    {
        if (!req->is_first)
        {
            self->trace.force_warning(
                "Write-burst continuation without an open burst (req=%p) — "
                "opening one\n", req);
        }
        else if (self->open_wr_burst != nullptr)
        {
            self->trace.force_warning(
                "Write burst opened while another is still accepting beats "
                "(req=%p)\n", req);
        }
        self->open_wr_burst = new WriteBurst();
        self->open_wr_burst->base_addr = burst_addr;
        self->open_wr_burst->burst_id = req->burst_id;
        self->open_wr_burst->initiator = req->initiator;
    }
    WriteBurst *burst = self->open_wr_burst;
    self->traces.assert(burst->initiator == req->initiator,
        "write beats of one burst must carry the same initiator (req=%p)", req);

    burst->total += size;
    if (beat_last)
    {
        burst->last_seen = true;
        // Submission-complete: the next is_first opens a fresh burst even
        // while this one still awaits downstream completions (pipelining).
        self->open_wr_burst = nullptr;
    }

    auto inserted = self->in_flight.emplace(req,
        InFlight{size, 0, burst_addr, beat_last, burst});
    if (!inserted.second)
    {
        self->trace.force_warning(
            "Resubmit of in-flight req (req=%p) — resetting bookkeeping\n", req);
        inserted.first->second = InFlight{size, 0, burst_addr, beat_last, burst};
    }

    vp::IoReqStatus st = self->out.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Sync big-packet — req->data is already consumed. Run the (now
        // mostly silent) per-beat scheduling so the burst ack fires on the
        // cycle the last per-beat ack fired before the protocol change.
        // get_full_latency() = head latency + bandwidth occupancy (duration):
        // a bandwidth slave reports its transfer cost via set_duration(), which
        // is max-combined across hops, so reading latency alone would miss it.
        self->schedule_chunk(req, size, req->get_full_latency());
        return vp::IO_REQ_GRANTED;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        // Ownership never transferred: roll the framing bookkeeping back —
        // the master re-sends this same beat from retry().
        self->in_flight.erase(req);
        burst->total -= size;
        if (beat_last)
        {
            burst->last_seen = false;
            self->open_wr_burst = burst;
        }
        if (burst->total == 0 && burst->completed == 0 && req->is_first)
        {
            delete burst;
            self->open_wr_burst = nullptr;
        }
    }
    return st;
}


vp::IoRespAck IoV2BeatAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);

    // The adapter always accepts the downstream response (it buffers it and
    // paces the upstream stream itself), so it never back-pressures downstream.

    // Async completion of one of our in-flight read sub-reads? Mark it done,
    // drain any now-contiguous completed beats in order, and refill the
    // pipeline. (Searching is cheap: max_sub_outstanding is small.)
    // A big-packet / single-req / sync downstream rounds-trip the sub-read
    // object itself (identity match). A BEAT downstream instead answers with
    // DISTINCT allocator-backed beats whose payload carries the data,
    // correlated back to our sub-read via req->initiator (set at issue) —
    // copy each payload slice into the sub-read's own payload at the fill
    // cursor, free the beat, and complete on its last beat.
    for (auto &e : self->sub_inflight)
    {
        if (e.req == req)
        {
            e.completed = true;
            e.status    = req->get_resp_status();
            e.latency   = req->get_full_latency();
            self->drain_completed_sub_reads();
            self->issue_pending_sub_reads();
            self->reschedule_fsm();
            return vp::IO_RESP_ACCEPTED;
        }
    }
    if (!req->get_is_write() && req->initiator != nullptr && req->initiator != req)
    {
        vp::IoReq *own = (vp::IoReq *)req->initiator;
        for (auto &e : self->sub_inflight)
        {
            if (e.req == own)
            {
                if (req->get_size() > 0)
                {
                    memcpy(own->get_data() + e.filled, req->get_data(),
                           req->get_size());
                    e.filled += req->get_size();
                }
                if (req->get_resp_status() == vp::IO_RESP_INVALID)
                {
                    e.status = vp::IO_RESP_INVALID;
                }
                bool last = req->is_last;
                int64_t latency = req->get_full_latency();
                req->free();
                if (last)
                {
                    e.completed = true;
                    e.latency   = latency;
                    self->drain_completed_sub_reads();
                    self->issue_pending_sub_reads();
                    self->reschedule_fsm();
                }
                return vp::IO_RESP_ACCEPTED;
            }
        }
    }

    // Atomic round-trip: relay the master's own object upstream once.
    if (req->get_opcode() != vp::WRITE)
    {
        self->schedule_atomic(req, req->get_full_latency());
        return vp::IO_RESP_ACCEPTED;
    }

    // Write path: the downstream round-trips the beat we lent it. Run the
    // (silent) scheduling — the burst ack fires once the whole burst has
    // completed — and consume the beat (schedule_chunk frees or parks it).
    self->schedule_chunk(req, req->get_size(), req->get_full_latency());
    return vp::IO_RESP_ACCEPTED;
}


void IoV2BeatAdapter::schedule_atomic(vp::IoReq *req, int64_t latency_cycles)
{
    int64_t now = this->clock.get_cycles();
    if (this->write_last_sched_cycle < now)
        this->write_last_sched_cycle = now;
    int64_t ready = now + std::max((int64_t)1, latency_cycles);
    if (ready <= this->write_last_sched_cycle)
        ready = this->write_last_sched_cycle + 1;
    this->write_last_sched_cycle = ready;

    this->pending.push_back(PendingBeat{req, nullptr, req->get_size(), 0,
        req->get_addr(), ready, true, true, req->get_resp_status(),
        req->burst_id, nullptr, false, req->initiator});
    this->reschedule_fsm();
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
    // Read burst requests carry no data (data == NULL): each sub-read's
    // allocator-provided payload holds one beat of read data and travels
    // upstream inside the response beat.
    uint64_t burst_addr = req->get_addr();
    uint64_t total      = req->get_size();
    int64_t  burst_id   = req->burst_id;

    if (total == 0)
    {
        // Degenerate empty read: emit a single zero-size completion beat so the
        // upstream master still sees is_first=is_last on its burst.
        this->read_jobs.push_back(SubReadJob{
            req, 0, 0, burst_addr, true, true, burst_id});
        return;
    }

    for (uint64_t offset = 0; offset < total; offset += this->beat_width)
    {
        uint64_t beat = std::min<uint64_t>(total - offset,
                                           (uint64_t)this->beat_width);
        this->read_jobs.push_back(SubReadJob{
            req, offset, beat, burst_addr + offset,
            offset == 0, offset + beat == total, burst_id});
    }
}


void IoV2BeatAdapter::issue_sub_read(const SubReadJob &job)
{
    // Each in-flight sub-read needs its own object so several can coexist
    // downstream. Drawn from the beat allocator: its co-allocated payload
    // receives the read data (the downstream target fills it directly) and
    // the same object is then forwarded upstream as the response beat, where
    // the terminal master frees it. data is the allocator's payload and is
    // never repointed.
    vp::IoReq *r = this->beat_allocator->alloc();
    r->prepare();
    r->set_addr(job.addr);
    r->set_size(job.beat_bytes);
    r->set_is_write(false);
    r->is_first = true;
    r->is_last  = true;
    r->burst_id = -1;
    // Self-correlator for a BEAT downstream (form 3), whose distinct response
    // beats reference this sub-read via initiator. Overwritten with the
    // upstream master's initiator at emit time (emit_beat).
    r->initiator = r;

    vp::IoReqStatus st = this->out.req(r);

    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream full: hold this job, re-issue on retry, issue nothing more
        // until then. The job was already popped from read_jobs by the caller,
        // so it lives in denied_job now and is not lost.
        this->sub_read_denied = true;
        this->denied_job = job;
        r->free();
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
        e.latency   = r->get_full_latency();
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
    // make is_last reach the upstream master before earlier beats. The
    // sub-read object itself is handed over to the pending queue: it carries
    // the read data in its payload and goes upstream as the response beat.
    while (!this->sub_inflight.empty() && this->sub_inflight.front().completed)
    {
        InflightSubRead e = this->sub_inflight.front();
        this->sub_inflight.pop_front();
        this->complete_sub_read(e.job, e.req, e.status, e.latency);
    }
}


void IoV2BeatAdapter::complete_sub_read(const SubReadJob &job, vp::IoReq *beat,
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
        nullptr,
        job.beat_bytes,
        job.offset,
        job.addr,
        ready,
        job.is_first,
        job.is_last,
        status == vp::IO_RESP_INVALID ? vp::IO_RESP_INVALID : vp::IO_RESP_OK,
        job.burst_id,
        beat,
    });

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Read beat ready (req=%p, offset=%lu, size=%lu, ready=%ld, first=%d, last=%d)\n",
        job.up_req, job.offset, job.beat_bytes, (long)ready,
        job.is_first ? 1 : 0, job.is_last ? 1 : 0);

    this->reschedule_fsm();
}


void IoV2BeatAdapter::schedule_chunk(vp::IoReq *req, uint64_t size,
                                     int64_t latency_cycles)
{
    auto it = this->in_flight.find(req);
    if (it == this->in_flight.end())
    {
        this->trace.force_warning(
            "Response for unknown req (req=%p, size=%lu) — dropping\n",
            req, size);
        return;
    }
    // Snapshot: the entry may be erased (and the beat freed/parked) below,
    // before the scheduling loop runs.
    InFlight inf = it->second;
    WriteBurst *burst = inf.burst;

    // Snapshot everything still read after the retire block below — the beat
    // may be freed there.
    vp::IoRespStatus req_status = req->get_resp_status();
    int64_t req_burst_id = req->burst_id;

    if (req_status == vp::IO_RESP_INVALID)
    {
        burst->status = vp::IO_RESP_INVALID;
    }

    bool req_completes = (inf.bytes_routed + size >= inf.total_size);
    // The burst closes when this chunk completes its last outstanding bytes
    // and the is_last beat has been submitted (last_seen is set at submit
    // time, so out-of-order downstream completions cannot lose the close).
    bool burst_closes = req_completes && burst->last_seen
        && (burst->completed + inf.total_size == burst->total);

    // Retire the request before scheduling so the burst's last beat is
    // parked (available for recycling as the ack) by the time the ack entry
    // is built.
    if (req_completes)
    {
        if (inf.bytes_routed + size > inf.total_size)
        {
            this->trace.force_warning(
                "Slave responded with more bytes than requested (req=%p, total=%lu, got=%lu)\n",
                req, inf.total_size, inf.bytes_routed + size);
        }
        this->in_flight.erase(it);
        burst->completed += inf.total_size;
        if (inf.burst_last)
        {
            burst->last_beat = req;
        }
        else
        {
            req->free();
        }
    }
    else
    {
        it->second.bytes_routed += size;
    }

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
    // Under the per-burst ack contract all entries but the burst-final one are
    // SILENT virtual acks: they advance this cursor exactly as the per-beat
    // acks did, so the single real ack fires on the same cycle the last
    // per-beat ack fired.
    if (write_last_sched_cycle < now)
        write_last_sched_cycle = now;

    uint64_t cursor = 0;
    int64_t beat_idx = 0;
    bool ack_pushed = false;
    while (cursor < size)
    {
        uint64_t beat = std::min<uint64_t>(size - cursor,
                                           (uint64_t)this->beat_width);
        uint64_t offset = inf.bytes_routed + cursor;
        bool chunk_last = (offset + beat == inf.total_size);
        bool is_ack = burst_closes && chunk_last;

        int64_t ready = first_ready + beat_idx * step;
        if (ready <= write_last_sched_cycle)
            ready = write_last_sched_cycle + 1;
        write_last_sched_cycle = ready;

        PendingBeat ev{
            nullptr,
            nullptr,
            is_ack ? burst->total : beat,
            offset,
            is_ack ? burst->base_addr : inf.burst_addr + offset,
            ready,
            true,
            true,
            is_ack ? burst->status : req_status,
            is_ack ? burst->burst_id : req_burst_id,
            is_ack ? burst->last_beat : nullptr,
            !is_ack,
            is_ack ? burst->initiator : nullptr,
        };
        this->pending.push_back(ev);
        ack_pushed |= is_ack;

        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Schedule write %s (req=%p, offset=%lu, size=%lu, ready=%ld)\n",
            is_ack ? "ack" : "tick", req, offset, beat, (long)ready);

        cursor += beat;
        beat_idx++;
    }

    // Degenerate zero-size chunk closing the burst (e.g. a zero-size write):
    // the loop pushed nothing, so schedule the ack explicitly.
    if (burst_closes && !ack_pushed)
    {
        int64_t ready = now + std::max((int64_t)1, latency_cycles);
        if (ready <= write_last_sched_cycle)
            ready = write_last_sched_cycle + 1;
        write_last_sched_cycle = ready;
        this->pending.push_back(PendingBeat{
            nullptr, nullptr, burst->total, 0, burst->base_addr, ready,
            true, true, burst->status, burst->burst_id, burst->last_beat,
            false, burst->initiator});
    }

    if (burst_closes)
    {
        delete burst;
    }

    this->reschedule_fsm();
}


void IoV2BeatAdapter::emit_beat(const PendingBeat &ev)
{
    // Write entries (req == nullptr): silent virtual-ack ticks pace the write
    // cursor and emit nothing; the burst-final entry releases the burst's
    // last beat and emits the single data-less ack as a dedicated request
    // (see io_v2.hpp, "The write ack").
    if (ev.req == nullptr)
    {
        if (ev.silent)
        {
            return;
        }
        vp::IoReq *ack = vp::io_v2_write_ack(ev.beat);
        ack->set_addr(ev.addr);
        ack->set_size(ev.size);
        ack->burst_id = ev.burst_id;
        ack->set_resp_status(ev.status);
        ack->initiator = ev.initiator;

        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Emit write burst ack (ack=%p, addr=0x%lx, size=%lu, status=%d)\n",
            ack, ev.addr, ev.size, ev.status);

        if (this->in.resp(ack) == vp::IO_RESP_DENIED)
        {
            // Upstream busy: hold the ack and re-send it on resp_retry.
            this->resp_held = true;
            this->held_req = ack;
            this->held_is_ours = true;
        }
        return;
    }

    // Atomic round-trip entry (req set, no beat): hand the master its own
    // object back with the completion status. Never freed here.
    if (ev.beat == nullptr)
    {
        vp::IoReq *req = ev.req;
        req->is_first = true;
        req->is_last = true;
        req->burst_id = ev.burst_id;
        req->set_resp_status(ev.status);

        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Emit atomic completion (req=%p, addr=0x%lx, size=%lu)\n",
            req, ev.addr, ev.size);

        if (this->in.resp(req) == vp::IO_RESP_DENIED)
        {
            this->resp_held = true;
            this->held_req = req;
            this->held_is_ours = false;
        }
        return;
    }

    // Read response beat (initiator-owned request convention): every read beat —
    // single- or multi-beat — is a distinct allocator-backed object the terminal
    // master frees (req->free()) as it consumes it, after copying the payload out
    // of beat->data. The master's burst request is NEVER round-tripped as a read
    // beat and is NEVER freed by the adapter: the initiator owns it and frees it
    // on the last response, correlating each beat back to its request by
    // req->initiator (copied below), not by object identity. The object is the
    // completed sub-read itself — its payload already holds the read data.
    vp::IoReq *beat = ev.beat;
    beat->set_addr(ev.addr);
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

    if (this->in.resp(beat) == vp::IO_RESP_DENIED)
    {
        // Upstream busy: hold this freshly-built beat and re-send it on
        // resp_retry. The master's request is the initiator's to free, not ours.
        this->resp_held = true;
        this->held_req = beat;
        this->held_is_ours = true;
        return;
    }
}


void IoV2BeatAdapter::resp_retry_in_handler(vp::Block *__this,
                                            vp::IoRetryChannel /*channel*/)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    if (!self->resp_held)
    {
        return;
    }
    // The io_v2 contract requires the re-send to happen synchronously inside the
    // retry callback.
    if (self->in.resp(self->held_req) == vp::IO_RESP_DENIED)
    {
        // Still not ready — keep holding and wait for the next resp_retry.
        return;
    }
    self->resp_held = false;
    self->held_req = nullptr;
    // Resume draining the rest of the pending beats (on the next cycle — the
    // master accepts one beat per cycle).
    self->reschedule_fsm();
}


void IoV2BeatAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    // Emit any upstream beats that are due this cycle. Stop the instant a beat
    // is back-pressured (emit_beat sets resp_held): the held beat must be
    // re-sent first, from resp_retry_in_handler, before any later beat.
    while (!self->resp_held && !self->pending.empty()
           && self->pending.front().ready_cycle <= now)
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
    // Blocked on upstream back-pressure: nothing can drain until resp_retry
    // releases the held beat, which reschedules us itself. Don't arm a timer.
    if (this->resp_held)
    {
        return;
    }
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
        // Read beats waiting in the pending queue are ours (allocator-backed
        // sub-reads), and so is a pending write burst ack (the recycled last
        // beat) — return them to their pool. Silent write entries carry
        // nothing.
        for (auto &ev : this->pending)
        {
            if (ev.beat != nullptr)
            {
                ev.beat->free();
            }
        }
        this->pending.clear();
        // Live write bursts: the one still accepting beats plus any awaiting
        // downstream completions (referenced by in_flight). Their in-flight
        // beats were lent to the downstream (which never frees them) — the
        // adapter consumes them here. A parked last beat is freed with its
        // burst.
        {
            std::set<WriteBurst *> bursts;
            if (this->open_wr_burst != nullptr)
            {
                bursts.insert(this->open_wr_burst);
            }
            for (auto &kv : this->in_flight)
            {
                kv.first->free();
                bursts.insert(kv.second.burst);
            }
            for (WriteBurst *b : bursts)
            {
                if (b->last_beat != nullptr)
                {
                    b->last_beat->free();
                }
                delete b;
            }
            this->open_wr_burst = nullptr;
        }
        this->in_flight.clear();
        this->read_jobs.clear();
        for (auto &e : this->sub_inflight)
        {
            e.req->free();
        }
        this->sub_inflight.clear();
        this->sub_read_denied = false;
        // Drop any held (back-pressured) beat — a held read beat and a held
        // write ack are allocator-backed and ours to free; a held atomic is
        // the master's own object and is left alone.
        if (this->resp_held && this->held_req != nullptr && this->held_is_ours)
        {
            this->held_req->free();
        }
        this->resp_held = false;
        this->held_req = nullptr;
        this->held_is_ours = false;
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
