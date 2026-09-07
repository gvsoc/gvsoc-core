// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_to_sync_adapter.hpp"

#include <algorithm>


IoV2BeatToSyncAdapter::IoV2BeatToSyncAdapter(vp::ComponentConf &config)
    : vp::Component(config),
      in(&IoV2BeatToSyncAdapter::req_handler, &IoV2BeatToSyncAdapter::resp_retry_in_handler),
      out(&IoV2BeatToSyncAdapter::retry_handler, &IoV2BeatToSyncAdapter::resp_handler),
      fsm_event(this, &IoV2BeatToSyncAdapter::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->beat_width = this->get_js_config()->get_child_int("beat_width");
    if (this->beat_width <= 0)
    {
        this->trace.fatal("IoV2BeatToSyncAdapter requires beat_width > 0 (got %d)\n",
                          this->beat_width);
    }

    this->beat_allocator = vp::IoReqAllocator::get(this->beat_width);

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}


vp::IoReqStatus IoV2BeatToSyncAdapter::req_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatToSyncAdapter *>(__this);

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, burst_id=%ld)\n",
        req, req->get_addr(), req->get_size(), req->get_is_write() ? 1 : 0,
        (long)req->burst_id);

    uint64_t burst_addr = req->get_addr();
    uint64_t total      = req->get_size();
    int64_t  burst_id   = req->burst_id;
    int64_t  latency    = 0;
    // Only (re)start the FSM if it is idle. While it is already streaming the
    // appended entries drain behind the active ones; a fresh enqueue could
    // pull the next paced beat earlier than its slot.
    bool     was_idle   = self->entries.empty() && !self->resp_held;

    if (req->get_is_write() && req->get_opcode() != vp::WRITE)
    {
        // Atomic opcodes (LR/SC/SWAP/...) are exempt from the per-burst write
        // ack: they carry response data, so they keep the classic round-trip —
        // the master's own request goes back via resp() and nobody else frees
        // it. Keyed on opcode, not get_is_write() (true for every non-READ
        // opcode). Forward inline, then queue the round-trip entry.
        vp::IoReqStatus st = self->out.req(req);
        self->traces.assert(st == vp::IO_REQ_DONE,
            "sync slave must reply IO_REQ_DONE inline (got %d)", (int)st);
        latency = req->get_full_latency();

        self->entries.push_back(StreamEntry{StreamEntry::ATOMIC, req,
            0, 0, -1, vp::IO_RESP_OK, nullptr});
    }
    else if (req->get_is_write())
    {
        // WRITE beat. Under the per-burst ack contract the adapter is the
        // consumer of the master's allocator-backed beats: forward each one
        // inline to the sync slave (single-beat framing — the sync plane
        // knows no bursts) and consume it on the spot (the payload was read
        // inline): free a non-last beat, recycle the last one as the single
        // data-less burst ack. Every beat still queues its beat-width worth
        // of SILENT entries in the shared FIFO so the ack drains on exactly
        // the cycle the last per-beat ack fired before the protocol change.
        self->traces.assert(req->allocator != nullptr,
            "write beat is not allocator-backed (req=%p) — unported master", req);

        bool beat_last = req->is_last;

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
            self->open_wr_burst->burst_id = burst_id;
            self->open_wr_burst->initiator = req->initiator;
        }
        WriteBurst *burst = self->open_wr_burst;
        self->traces.assert(burst->initiator == req->initiator,
            "write beats of one burst must carry the same initiator (req=%p)",
            req);

        burst->total += total;

        // The downstream plane is not a beat binding: each forwarded beat is
        // an independent single-beat request there.
        req->is_first = true;
        req->is_last = true;

        vp::IoReqStatus st = self->out.req(req);
        self->traces.assert(st == vp::IO_REQ_DONE,
            "sync slave must reply IO_REQ_DONE inline (got %d)", (int)st);

        latency = req->get_full_latency();
        if (req->get_resp_status() == vp::IO_RESP_INVALID)
        {
            burst->status = vp::IO_RESP_INVALID;
        }

        // Consume the beat: its payload was read inline by the sync slave.
        // The burst's last beat is recycled as the ack object instead of
        // being freed (the ack entry below carries it).
        if (!beat_last)
        {
            req->free();
        }

        uint64_t offset = 0;
        do
        {
            // A zero-size beat still consumes a single FIFO slot (and a
            // zero-size burst still gets its single zero-size ack).
            uint64_t beat = std::min<uint64_t>(total - offset,
                                               (uint64_t)self->beat_width);
            bool is_ack = beat_last && (offset + beat >= total);
            if (is_ack)
            {
                self->entries.push_back(StreamEntry{StreamEntry::WRITE_ACK,
                    req, burst->base_addr, burst->total, burst->burst_id,
                    burst->status, burst->initiator});
            }
            else
            {
                self->entries.push_back(StreamEntry{StreamEntry::WRITE_TICK,
                    nullptr, 0, 0, -1, vp::IO_RESP_OK, nullptr});
            }
            offset += beat;
        } while (offset < total);

        if (beat_last)
        {
            // Burst complete (every beat completed inline): the record's
            // snapshot now lives in the ack entry.
            delete burst;
            self->open_wr_burst = nullptr;
        }
    }
    else
    {
        // Read burst requests carry no data: nothing is read now. Queue one
        // lightweight descriptor per beat; each beat is read from the sync
        // slave in emit_entry, at its own delivery cycle, and paced by its own
        // bandwidth occupancy (get_duration()). No filled beats are held.
        size_t first_idx = self->entries.size();
        uint64_t offset = 0;
        do
        {
            // A zero-size burst still gets a single zero-size completion beat.
            uint64_t beat = std::min<uint64_t>(total - offset,
                                               (uint64_t)self->beat_width);
            self->entries.push_back(StreamEntry{StreamEntry::READ_BEAT,
                nullptr, burst_addr + offset, beat, burst_id, vp::IO_RESP_OK,
                req->initiator, offset == 0, offset + beat >= total});
            offset += beat;
        } while (offset < total);

        // Head beat: only when the stream was idle do we pay a head latency.
        // Issue it now (the address phase) purely to sample get_latency(); its
        // data is stashed on the entry and delivered after that latency (the
        // FSM start below). Following beats are read lazily at their slots.
        if (was_idle)
        {
            StreamEntry &e0 = self->entries[first_idx];
            vp::IoReq *b0 = self->beat_allocator->alloc();
            b0->prepare();
            b0->set_addr(e0.addr);
            b0->set_size(e0.size);
            b0->set_is_write(false);
            b0->is_first = true;
            b0->is_last  = true;
            b0->burst_id = -1;

            vp::IoReqStatus st = self->out.req(b0);
            self->traces.assert(st == vp::IO_REQ_DONE,
                "sync slave must reply IO_REQ_DONE inline (got %d)", (int)st);

            e0.beat = b0;               // reused at emit, not re-issued
            latency = b0->get_latency();
        }
    }

    // Start the FSM after the head latency (reads) / full latency (writes,
    // atomics), but only if it was idle — otherwise the active stream drains
    // the appended entries at their own pace (see was_idle / schedule_next).
    if (was_idle)
    {
        self->fsm_event.enqueue(std::max((int64_t)1, latency));
    }

    return vp::IO_REQ_GRANTED;
}


vp::IoRespAck IoV2BeatToSyncAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatToSyncAdapter *>(__this);
    // A sync slave never responds asynchronously.
    self->trace.fatal("Unexpected resp() from a sync slave (req=%p)\n", req);
    return vp::IO_RESP_ACCEPTED;
}


void IoV2BeatToSyncAdapter::retry_handler(vp::Block *__this, vp::IoRetryChannel)
{
    auto *self = static_cast<IoV2BeatToSyncAdapter *>(__this);
    // A sync slave never denies, so it never retries.
    self->trace.fatal("Unexpected retry() from a sync slave\n");
}


bool IoV2BeatToSyncAdapter::emit_entry(const StreamEntry &e)
{
    // Silent virtual ack: consumes its 1-entry/cycle FIFO slot (pacing the
    // stream exactly as the per-beat write acks did) and emits nothing.
    if (e.kind == StreamEntry::WRITE_TICK)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Consume write tick\n");
        this->stream_interval = 1;
        return true;
    }

    vp::IoReq *r = e.beat;
    if (e.kind == StreamEntry::READ_BEAT)
    {
        // Read the beat from the sync slave NOW (at its delivery cycle), unless
        // it was pre-issued at submit as the burst's head beat. Downstream is
        // single-beat framing; the upstream burst framing is stamped after.
        if (r == nullptr)
        {
            r = this->beat_allocator->alloc();
            r->prepare();
            r->set_addr(e.addr);
            r->set_size(e.size);
            r->set_is_write(false);
            r->is_first = true;
            r->is_last  = true;
            r->burst_id = -1;

            vp::IoReqStatus st = this->out.req(r);
            this->traces.assert(st == vp::IO_REQ_DONE,
                "sync slave must reply IO_REQ_DONE inline (got %d)", (int)st);
        }
        r->is_first = e.is_first;
        r->is_last  = e.is_last;
        r->burst_id = e.burst_id;
        r->initiator = e.initiator;
        // Pace the next beat by this beat's own bandwidth occupancy (floored at
        // one cycle so a no-duration slave streams one per cycle). Set before
        // in.resp() so a back-pressure hold still resumes at the right pace.
        this->stream_interval = std::max((int64_t)1, r->get_duration());
    }
    else if (e.kind == StreamEntry::WRITE_ACK)
    {
        // The single data-less burst ack: a dedicated request, the burst's
        // consumed last beat goes back to its own pool (see io_v2.hpp, "The
        // write ack"). The initiator frees the ack.
        r = vp::io_v2_write_ack(r);
        r->set_addr(e.addr);
        r->set_size(e.size);
        r->burst_id = e.burst_id;
        r->set_resp_status(e.status);
        r->initiator = e.initiator;
        this->stream_interval = 1;
    }
    else
    {
        // ATOMIC: the master's own request, emitted unmutated (classic
        // round-trip; it already carries data and status).
        this->stream_interval = 1;
    }

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit beat (req=%p, addr=0x%lx, size=%lu, first=%d, last=%d, write=%d)\n",
        r, r->get_addr(), r->get_size(), r->is_first ? 1 : 0, r->is_last ? 1 : 0,
        r->get_is_write() ? 1 : 0);

    if (this->in.resp(r) == vp::IO_RESP_DENIED)
    {
        // Upstream back-pressure: hold this exact object and re-send on
        // resp_retry. The entry was already popped by the caller, so nothing
        // else advances until the held beat is accepted.
        this->resp_held = true;
        this->held_req = r;
        this->held_owned = (e.kind != StreamEntry::ATOMIC);
        return false;
    }
    return true;
}


void IoV2BeatToSyncAdapter::resp_retry_in_handler(vp::Block *__this,
                                                vp::IoRetryChannel /*channel*/)
{
    auto *self = static_cast<IoV2BeatToSyncAdapter *>(__this);
    if (!self->resp_held)
    {
        return;
    }
    // io_v2 requires the re-send to happen synchronously inside the callback.
    if (self->in.resp(self->held_req) == vp::IO_RESP_DENIED)
    {
        return;   // still busy; keep holding
    }
    self->resp_held = false;
    self->held_req = nullptr;
    self->held_owned = false;
    self->schedule_next();
}


void IoV2BeatToSyncAdapter::schedule_next()
{
    if (this->entries.empty())
    {
        return;
    }
    // stream_interval was set by the entry just emitted (a read beat's own
    // bandwidth occupancy, one otherwise).
    this->fsm_event.enqueue(this->stream_interval);
}


void IoV2BeatToSyncAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatToSyncAdapter *>(__this);

    // Blocked on upstream back-pressure: the held beat must be re-sent first
    // (from resp_retry_in_handler), so don't stream anything now.
    if (self->resp_held)
    {
        return;
    }

    if (self->entries.empty())
    {
        return;
    }

    StreamEntry e = self->entries.front();
    self->entries.pop_front();

    if (!self->emit_entry(e))
    {
        // Held on upstream back-pressure; resp_retry_in_handler resumes once
        // the master accepts.
        return;
    }

    self->schedule_next();
}


vp::DebugMemIf *IoV2BeatToSyncAdapter::resolve_debug_mem()
{
    std::vector<vp::SlavePort *> finals = this->out.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}


int IoV2BeatToSyncAdapter::debug_mem_access(uint64_t addr, uint8_t *data,
                                          uint64_t size, bool is_write)
{
    vp::DebugMemIf *target = this->resolve_debug_mem();
    return target ? target->debug_mem_access(addr, data, size, is_write) : -1;
}


void IoV2BeatToSyncAdapter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
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


void IoV2BeatToSyncAdapter::reset(bool active)
{
    if (active)
    {
        // Queued read beats and pending burst acks (the recycled last beats)
        // are ours (allocator-backed) — return them to their pool. Silent
        // ticks carry nothing; atomic entries reference the master's own
        // request, not ours to free.
        for (auto &e : this->entries)
        {
            if ((e.kind == StreamEntry::READ_BEAT
                 || e.kind == StreamEntry::WRITE_ACK)
                && e.beat != nullptr)
            {
                e.beat->free();
            }
        }
        this->entries.clear();
        this->stream_interval = 1;
        // A burst still accepting beats: its already-received beats were
        // consumed (freed) inline, only the record remains.
        delete this->open_wr_burst;
        this->open_wr_burst = nullptr;
        // Same for a held (back-pressured) object: a read beat or recycled
        // burst ack is ours to free; a master-owned atomic round-trip is
        // left alone.
        if (this->resp_held && this->held_req != nullptr && this->held_owned)
        {
            this->held_req->free();
        }
        this->resp_held = false;
        this->held_req = nullptr;
        this->held_owned = false;
        this->fsm_event.cancel();   // safe even if not enqueued
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatToSyncAdapter(config);
}
