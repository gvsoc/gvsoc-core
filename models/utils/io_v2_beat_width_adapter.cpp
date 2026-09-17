// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_width_adapter.hpp"

#include <algorithm>
#include <cstring>

// ===========================================================================
// Method roster / call graph
// ===========================================================================
// Two io_v2 ports — the slave "input" (`in`, faces the upstream IoV2Beat
// master of width input_width) and the master "output" (`out`, faces the
// downstream IoV2Beat slave of width output_width) — plus one ClockEvent:
//
//   in.req           -> req_handler          (upstream submits a read/write)
//   in.resp_retry    -> resp_retry_in_handler(upstream can take responses again)
//   out.resp         -> resp_handler         (downstream read beat / write ack)
//   out.retry        -> retry_handler        (downstream ready again after a DENY)
//   fsm_event        -> fsm_handler          (per-cycle pump)
//
// READ flow:  submit_read (forward data-less descriptor) -> [downstream
//   streams output_width beats] -> consume_read_beat (buffer the beat in its
//   burst's response FIFO, deny when the FIFO is full) -> fsm_handler ->
//   emit_read_beat (extract ONE input_width beat from the buffered bytes and
//   send it upstream, 1/cycle).
// WRITE flow: submit_write (chop/pack payload into output_width chunks,
//   free the consumed upstream beat, DENY upstream while the chunk backlog is
//   full) -> issue_pending_chunks (1/cycle downstream, one framed downstream
//   burst per upstream burst) -> complete_chunk (GRANTED: chunk consumed by
//   the target; last-chunk DONE or [burst ack] in resp_handler) ->
//   complete_write_burst -> fsm_handler -> emit_ack (ONE data-less size-0-pool
//   ack per burst, freed by the upstream master).
// ===========================================================================


IoV2BeatWidthAdapter::IoV2BeatWidthAdapter(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      in(&IoV2BeatWidthAdapter::req_handler, &IoV2BeatWidthAdapter::resp_retry_in_handler),
      out(&IoV2BeatWidthAdapter::retry_handler, &IoV2BeatWidthAdapter::resp_handler),
      fsm_event(this, &IoV2BeatWidthAdapter::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->input_width = (int)this->cfg.input_width;
    this->output_width = (int)this->cfg.output_width;
    if (this->input_width <= 0 || this->output_width <= 0)
    {
        this->trace.fatal("IoV2BeatWidthAdapter requires positive widths (got %d/%d)\n",
                          this->input_width, this->output_width);
    }
    if (std::max(this->input_width, this->output_width)
        % std::min(this->input_width, this->output_width) != 0)
    {
        this->trace.fatal(
            "IoV2BeatWidthAdapter requires the wider width to be a multiple of "
            "the narrower one (got %d/%d)\n",
            this->input_width, this->output_width);
    }

    this->read_pending_limit = this->cfg.read_fifo_depth > 0
        ? (size_t)this->cfg.read_fifo_depth
        : 2 * (size_t)std::max(1, this->output_width / this->input_width);
    this->write_chunk_limit = this->cfg.write_fifo_depth > 0
        ? (size_t)this->cfg.write_fifo_depth
        : 2;

    this->in_beat_allocator = vp::IoReqAllocator::get(this->input_width);
    this->out_beat_allocator = vp::IoReqAllocator::get(this->output_width);
    this->desc_allocator = vp::IoReqAllocator::get(0);

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}


// ---------------------------------------------------------------------------
// Freelist pools
// ---------------------------------------------------------------------------

IoV2BeatWidthAdapter::ReadBurst *IoV2BeatWidthAdapter::alloc_read_burst()
{
    if (!this->read_burst_pool.empty())
    {
        ReadBurst *burst = this->read_burst_pool.back();
        this->read_burst_pool.pop_back();
        *burst = ReadBurst{};
        return burst;
    }
    return new ReadBurst();
}

void IoV2BeatWidthAdapter::free_read_burst(ReadBurst *burst)
{
    this->read_burst_pool.push_back(burst);
}

IoV2BeatWidthAdapter::WriteBurst *IoV2BeatWidthAdapter::alloc_write_burst()
{
    if (!this->write_burst_pool.empty())
    {
        WriteBurst *burst = this->write_burst_pool.back();
        this->write_burst_pool.pop_back();
        *burst = WriteBurst{};
        return burst;
    }
    return new WriteBurst();
}

void IoV2BeatWidthAdapter::free_write_burst(WriteBurst *burst)
{
    this->write_burst_pool.push_back(burst);
}

IoV2BeatWidthAdapter::WriteChunk *IoV2BeatWidthAdapter::alloc_chunk()
{
    WriteChunk *chunk;
    if (!this->chunk_pool.empty())
    {
        chunk = this->chunk_pool.back();
        this->chunk_pool.pop_back();
    }
    else
    {
        chunk = new WriteChunk();
    }
    chunk->req = nullptr;
    chunk->burst = nullptr;
    chunk->addr = 0;
    chunk->fill = 0;
    chunk->is_first = false;
    chunk->is_last = false;
    chunk->burst_id = -1;
    return chunk;
}

void IoV2BeatWidthAdapter::free_chunk(WriteChunk *chunk)
{
    this->chunk_pool.push_back(chunk);
}


// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

vp::IoReqStatus IoV2BeatWidthAdapter::req_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatWidthAdapter *>(__this);

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, first=%d, last=%d, burst_id=%ld)\n",
        req, req->get_addr(), req->get_size(), req->get_is_write() ? 1 : 0,
        req->is_first ? 1 : 0, req->is_last ? 1 : 0, (long)req->burst_id);

    if (req->get_opcode() == vp::READ)
    {
        return self->submit_read(req);
    }
    // The write rules are keyed on opcode == WRITE, not get_is_write():
    // atomics carry response data and keep the classic round-trip — but the
    // width adapter cannot chop/repack them, so they are not supported here.
    self->traces.assert(req->get_opcode() == vp::WRITE,
        "atomic opcodes are not supported by the beat width adapter (req=%p)",
        req);
    return self->submit_write(req);
}


vp::IoRespAck IoV2BeatWidthAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatWidthAdapter *>(__this);

    // The downstream burst's single write ack: a DISTINCT data-less object
    // (never one of our own chunks — those were consumed and freed by the
    // target), correlated back to the burst record via initiator, which we
    // copied onto every chunk. We initiated the downstream burst, so we free
    // the ack. Keyed on opcode == WRITE per the io_v2.hpp write-ack rules.
    if (req->get_opcode() == vp::WRITE)
    {
        self->traces.assert(req->is_last && req->get_data() == NULL,
            "write ack must be a data-less is_last beat (req=%p)", req);
        WriteBurst *burst = (WriteBurst *)req->initiator;
        self->traces.assert(burst != nullptr
            && std::find(self->live_bursts.begin(), self->live_bursts.end(),
                         burst) != self->live_bursts.end(),
            "write ack does not correlate to a live burst (req=%p)", req);
        if (req->get_resp_status() == vp::IO_RESP_INVALID)
        {
            burst->status = vp::IO_RESP_INVALID;
        }
        int64_t latency = req->get_full_latency();
        req->free();
        self->complete_write_burst(burst, latency);
        self->reschedule_fsm();
        return vp::IO_RESP_ACCEPTED;
    }

    // Read beat from the downstream stream.
    return self->consume_read_beat(req);
}


void IoV2BeatWidthAdapter::retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    auto *self = static_cast<IoV2BeatWidthAdapter *>(__this);

    // A held downstream write chunk must be re-sent synchronously inside retry().
    if (self->held_chunk != nullptr && channel != vp::IO_RETRY_READ)
    {
        WriteChunk *chunk = self->held_chunk;
        self->held_chunk = nullptr;
        vp::IoReqStatus st = self->out.req(chunk->req);
        if (st == vp::IO_REQ_DENIED)
        {
            self->held_chunk = chunk;
        }
        else
        {
            int64_t now = self->clock.get_cycles();
            if (self->chunk_issue_cursor < now) self->chunk_issue_cursor = now;
            self->complete_chunk(chunk, st);
        }
        self->reschedule_fsm();
    }

    // Forward upstream: the beat master may hold its own DENIED requests (a
    // read we refused because the downstream refused the descriptor, or a
    // write we refused on the chunk-backlog bound).
    self->in.retry(channel);
}


// ---------------------------------------------------------------------------
// READ path
// ---------------------------------------------------------------------------

vp::IoReqStatus IoV2BeatWidthAdapter::submit_read(vp::IoReq *req)
{
    ReadBurst *burst = this->alloc_read_burst();
    burst->up_req = req;
    burst->burst_addr = req->get_addr();
    burst->total = req->get_size();
    burst->burst_id = req->burst_id;
    burst->up_initiator = req->initiator;

    // Forward our own data-less descriptor (initiator-owned: we free it once
    // its response stream completes). Downstream beats reference `burst`
    // through their initiator, copied from the descriptor by the producer.
    vp::IoReq *dn = this->desc_allocator->alloc();
    dn->prepare();
    dn->set_addr(burst->burst_addr);
    dn->set_size(burst->total);
    dn->set_data(nullptr);
    dn->set_is_write(false);
    dn->is_first = true;
    dn->is_last = true;
    dn->burst_id = burst->burst_id;
    dn->initiator = burst;
    burst->dn_req = dn;

    vp::IoReqStatus st = this->out.req(dn);

    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream busy: the upstream master holds its descriptor and
        // re-sends it on the retry we forward.
        dn->free();
        this->free_read_burst(burst);
        return vp::IO_REQ_DENIED;
    }

    this->live_reads.push_back(burst);

    if (st == vp::IO_REQ_DONE)
    {
        // A beat slave cannot answer a data-less read inline with data: only a
        // zero-size read or an error can complete this way (cf. the collapse
        // adapter's boundary assert).
        this->traces.assert(burst->total == 0
                || dn->get_resp_status() == vp::IO_RESP_INVALID,
            "beat slave answered a data-less read inline (req=%p, size=%lu)",
            req, burst->total);
        this->complete_read_inline(burst, dn->get_full_latency());
    }

    return vp::IO_REQ_GRANTED;
}


// Downstream answered the whole descriptor inline (zero-size read or error):
// synthesize the upstream beat stream without any payload bytes.
void IoV2BeatWidthAdapter::complete_read_inline(ReadBurst *burst, int64_t latency)
{
    burst->status = burst->dn_req->get_resp_status();
    burst->dn_req->free();
    burst->dn_req = nullptr;
    burst->bytes_received = burst->total;
    // No payload: the pump synthesizes the upstream beats, one per cycle.
    burst->dn_done = true;
    burst->synth = true;
    burst->done_ready = this->clock.get_cycles() + std::max((int64_t)1, latency);

    this->reschedule_fsm();
}


vp::IoRespAck IoV2BeatWidthAdapter::consume_read_beat(vp::IoReq *beat)
{
    ReadBurst *burst = (ReadBurst *)beat->initiator;
    this->traces.assert(burst != nullptr && burst->dn_req != nullptr,
        "read beat with no live burst (beat=%p)", beat);
    // Initiator-owned convention: read beats are distinct producer objects,
    // never our own descriptor round-tripped.
    this->traces.assert(beat != burst->dn_req,
        "downstream round-tripped our read descriptor as a beat (req=%p)", beat);

    // Back-pressure the downstream producer while the response FIFO is full
    // (the buffered bytes drain one upstream beat per cycle). Refuse before
    // consuming anything: the producer holds the exact beat and re-sends it
    // on our resp_retry().
    if (this->rx_bytes >= this->read_pending_limit * (size_t)this->input_width
        && this->out.is_resp_retry_bound())
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Deny downstream read beat (buffered=%zu bytes, limit=%zu beats)\n",
            this->rx_bytes, this->read_pending_limit);
        this->dn_read_blocked = true;
        return vp::IO_RESP_DENIED;
    }

    uint64_t bytes = beat->get_size();
    int64_t latency = beat->get_full_latency();
    bool last = beat->is_last;
    int64_t ready = this->clock.get_cycles() + std::max((int64_t)1, latency);

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Consume downstream read beat (beat=%p, burst=%p, size=%lu, last=%d)\n",
        beat, burst, bytes, last ? 1 : 0);

    // A stream longer than the burst it answers is a protocol violation of the
    // downstream. Fatal in every build: carrying on would deliver bytes nobody
    // asked for.
    if (burst->dn_done || burst->bytes_received + bytes > burst->total)
    {
        this->trace.fatal("downstream read stream overran its burst (burst=%p, "
            "received=%lu, beat=%lu, total=%lu, done=%d)\n", burst,
            burst->bytes_received, bytes, burst->total, burst->dn_done ? 1 : 0);
        return vp::IO_RESP_ACCEPTED;
    }

    if (beat->get_resp_status() == vp::IO_RESP_INVALID)
    {
        burst->status = vp::IO_RESP_INVALID;
    }

    burst->bytes_received += bytes;
    if (bytes > 0)
    {
        // Keep the downstream beat as is; the pump extracts upstream beats
        // from it and frees it with its last byte.
        burst->rx.push_back(RxSegment{beat, bytes, 0, ready});
        burst->rx_avail += bytes;
        this->rx_bytes += bytes;
    }
    else
    {
        beat->free();
    }

    if (last)
    {
        this->traces.assert(burst->bytes_received >= burst->total,
            "downstream read stream ended short (burst=%p, got=%lu, total=%lu)",
            burst, burst->bytes_received, burst->total);
        burst->dn_done = true;
        burst->done_ready = ready;
        // Degenerate zero-size burst: the single (zero-size, is_last)
        // downstream beat produces the single zero-size upstream beat.
        burst->synth = burst->total == 0;
        // The response stream is over: our descriptor is dead, free it.
        burst->dn_req->free();
        burst->dn_req = nullptr;
    }

    this->reschedule_fsm();
    return vp::IO_RESP_ACCEPTED;
}


int64_t IoV2BeatWidthAdapter::next_read_beat_ready(ReadBurst *burst)
{
    if (burst->synth)
    {
        return burst->done_ready;
    }
    uint64_t want = std::min<uint64_t>(burst->total - burst->bytes_emitted,
                                       (uint64_t)this->input_width);
    if (want == 0 || burst->rx_avail < want)
    {
        return INT64_MAX;
    }
    // The beat is due once the buffered beat holding its last byte is.
    int64_t ready = 0;
    uint64_t covered = 0;
    for (const RxSegment &seg : burst->rx)
    {
        ready = std::max(ready, seg.ready_cycle);
        covered += seg.size - seg.consumed;
        if (covered >= want)
        {
            break;
        }
    }
    return ready;
}


void IoV2BeatWidthAdapter::emit_read_beat(ReadBurst *burst)
{
    uint64_t offset = burst->bytes_emitted;
    uint64_t size = std::min<uint64_t>(burst->total - offset,
                                       (uint64_t)this->input_width);

    vp::IoReq *beat = this->in_beat_allocator->alloc();
    beat->prepare();

    // Extract this beat's bytes from the front of the response FIFO. Every
    // pass takes at least one byte (no empty beat is ever buffered) and
    // rx_avail >= size was checked, so this ends within the few downstream
    // beats one upstream beat can span.
    uint64_t fill = 0;
    while (!burst->synth && fill < size)
    {
        RxSegment &seg = burst->rx.front();
        uint64_t copy = std::min(size - fill, seg.size - seg.consumed);
        memcpy(beat->get_data() + fill, seg.beat->get_data() + seg.consumed, copy);
        fill += copy;
        seg.consumed += copy;
        if (seg.consumed == seg.size)
        {
            seg.beat->free();
            burst->rx.pop_front();
        }
    }
    if (!burst->synth)
    {
        burst->rx_avail -= size;
        this->rx_bytes -= size;
    }
    burst->bytes_emitted += size;

    bool is_last = burst->bytes_emitted >= burst->total;
    beat->set_addr(burst->burst_addr + offset);
    beat->set_size(size);
    beat->set_is_write(false);
    beat->burst_id = burst->burst_id;
    beat->is_first = offset == 0;
    beat->is_last = is_last;
    beat->set_resp_status(burst->status);
    beat->initiator = burst->up_initiator;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit read beat (beat=%p, addr=0x%lx, size=%lu, first=%d, last=%d)\n",
        beat, burst->burst_addr + offset, size, offset == 0 ? 1 : 0, is_last ? 1 : 0);

    // The burst bookkeeping is done before sending: the beat carries
    // everything upstream needs, so a hold/re-send never touches the burst.
    if (is_last)
    {
        this->retire_read_burst(burst);
    }

    if (this->in.resp(beat) == vp::IO_RESP_DENIED)
    {
        this->resp_held = true;
        this->held_req = beat;
    }
}


void IoV2BeatWidthAdapter::retire_read_burst(ReadBurst *burst)
{
    auto it = std::find(this->live_reads.begin(), this->live_reads.end(), burst);
    if (it != this->live_reads.end())
    {
        this->live_reads.erase(it);
    }
    // The upstream descriptor is initiator-owned: the upstream master frees it
    // when it consumes the last beat — never us.
    this->free_read_burst(burst);
}


// ---------------------------------------------------------------------------
// WRITE path
// ---------------------------------------------------------------------------

vp::IoReqStatus IoV2BeatWidthAdapter::submit_write(vp::IoReq *req)
{
    // Bound the un-issued downstream chunk backlog (the write FIFO): once it
    // is full, a wide upstream writer is throttled to the downstream's
    // one-beat-per-cycle bandwidth. The master holds the request and re-sends
    // it on the retry(WRITE) we raise once the backlog drains. Checked before
    // any burst bookkeeping, so a DENY never needs a rollback.
    if (this->chunk_queue.size() >= this->write_chunk_limit)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Write denied — chunk backlog full (%zu)\n", this->chunk_queue.size());
        this->up_write_blocked = true;
        return vp::IO_REQ_DENIED;
    }

    // We consume and free the beat below, so it must be pool-backed
    // (io_v2.hpp "Request allocation").
    this->traces.assert(req->allocator != nullptr,
        "write beat is not allocator-backed (req=%p) — unported master", req);

    // Write beats of one burst arrive back-to-back (bursts do not interleave
    // on a link): a burst-opening request must not land while the previous
    // burst is still being packed.
    this->traces.assert(!(req->is_first && this->cur_chunk != nullptr),
        "new write burst started while the previous one is still packing (req=%p)",
        req);

    // Per-burst record: opened by the is_first beat, submission-closed by the
    // is_last beat. It survives the beats (which are freed at submit) until
    // the downstream burst completes and the single upstream ack is scheduled.
    if (req->is_first || this->open_burst == nullptr)
    {
        if (!req->is_first)
        {
            this->trace.force_warning(
                "Write-burst continuation without an open burst (req=%p) — "
                "opening one\n", req);
        }
        else if (this->open_burst != nullptr)
        {
            this->trace.force_warning(
                "Write burst opened while another is still accepting beats "
                "(req=%p)\n", req);
        }
        this->open_burst = this->alloc_write_burst();
        this->open_burst->base_addr = req->get_addr();
        this->open_burst->burst_id = req->burst_id;
        this->open_burst->initiator = req->initiator;
        this->live_bursts.push_back(this->open_burst);
    }
    WriteBurst *burst = this->open_burst;
    this->traces.assert(burst->initiator == req->initiator,
        "write beats of one burst must carry the same initiator (req=%p)", req);

    // Snapshot everything before freeing the beat (ownership travels with it,
    // buffer included).
    uint64_t addr = req->get_addr();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool up_first = req->is_first;
    bool up_last = req->is_last;
    int64_t burst_id = req->burst_id;

    burst->total += size;
    if (up_last)
    {
        burst->last_seen = true;
        // Submission-complete: the next is_first opens a fresh burst even
        // while this one still awaits its downstream completion (pipelining).
        this->open_burst = nullptr;
    }

    if (size == 0)
    {
        // Degenerate zero-size write beat: one zero-size downstream chunk,
        // completing (when up_last) into one zero-size upstream ack.
        this->traces.assert(this->cur_chunk == nullptr,
            "zero-size write inside a packed burst (req=%p)", req);
        this->cur_chunk = this->alloc_chunk();
        this->cur_chunk->req = this->out_beat_allocator->alloc();
        this->cur_chunk->burst = burst;
        this->cur_chunk->addr = addr;
        this->cur_chunk->is_first = up_first;
        this->cur_chunk->burst_id = burst_id;
        this->finish_chunk(up_last);
    }

    // Chop/pack the payload into output_width chunks. The chunk payload is
    // copied (allocator co-allocated): upstream buffers are not contiguous
    // across beats, so the general repack needs its own storage.
    // TODO(follow-up): a chunk whose byte range lies entirely inside one
    // upstream beat could alias that beat's buffer (holding the unfreed beat
    // alive instead of freeing it here) and skip the copy.
    uint64_t off = 0;
    while (off < size)
    {
        if (this->cur_chunk == nullptr)
        {
            this->cur_chunk = this->alloc_chunk();
            this->cur_chunk->req = this->out_beat_allocator->alloc();
            this->cur_chunk->burst = burst;
            this->cur_chunk->addr = addr + off;
            this->cur_chunk->is_first = up_first && off == 0;
            this->cur_chunk->burst_id = burst_id;
        }
        WriteChunk *chunk = this->cur_chunk;
        // A downstream beat never crosses an output_width boundary, exactly
        // like an AXI upsizer: the wider bus has fixed byte lanes, so a burst
        // starting mid-word only fills the lanes from its address to the next
        // boundary in its first beat. Without this, a write starting at an odd
        // address produced a full-width beat at that odd address, which no
        // downstream width-aware model can express (the SoC write splitter
        // rejects it as needing more chunks than it has lanes).
        uint64_t capacity = (uint64_t)this->output_width
            - chunk->addr % (uint64_t)this->output_width;
        uint64_t copy = std::min(size - off, capacity - chunk->fill);
        memcpy(chunk->req->get_data() + chunk->fill, data + off, copy);
        chunk->fill += copy;
        off += copy;
        bool burst_ends_here = up_last && off == size;
        if (chunk->fill == capacity || burst_ends_here)
        {
            this->finish_chunk(burst_ends_here);
        }
    }

    // The payload is copied into our chunks: the beat is fully consumed —
    // take ownership and free it right here, non-last and last beats alike
    // (the single burst ack is emitted from the size-0 pool, never by
    // recycling a beat).
    req->free();

    this->issue_pending_chunks();
    this->reschedule_fsm();
    return vp::IO_REQ_GRANTED;
}


void IoV2BeatWidthAdapter::finish_chunk(bool is_last)
{
    WriteChunk *chunk = this->cur_chunk;
    this->cur_chunk = nullptr;
    chunk->is_last = is_last;
    this->chunk_queue.push_back(chunk);
}


void IoV2BeatWidthAdapter::issue_pending_chunks()
{
    // Pace issuance at one downstream chunk per cycle; the fsm re-ticks every
    // cycle while chunks remain.
    if (this->held_chunk != nullptr || this->chunk_queue.empty())
    {
        return;
    }
    int64_t now = this->clock.get_cycles();
    if (now <= this->chunk_issue_cursor)
    {
        return;
    }

    WriteChunk *chunk = this->chunk_queue.front();
    this->chunk_queue.pop_front();
    this->chunk_issue_cursor = now;
    this->maybe_unblock_write();

    vp::IoReq *r = chunk->req;
    r->prepare();
    r->set_addr(chunk->addr);
    r->set_size(chunk->fill);
    r->set_is_write(true);
    r->is_first = chunk->is_first;
    r->is_last = chunk->is_last;
    r->burst_id = chunk->burst_id;
    // Every chunk of one downstream burst carries the SAME initiator — the
    // burst record — so the burst's single data-less ack correlates back to
    // it (io_v2.hpp "The write ack").
    r->initiator = chunk->burst;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Issue write chunk (chunk=%p, addr=0x%lx, size=%lu, first=%d, last=%d)\n",
        chunk, chunk->addr, chunk->fill, chunk->is_first ? 1 : 0,
        chunk->is_last ? 1 : 0);

    vp::IoReqStatus st = this->out.req(r);

    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream full: hold this chunk and re-send it synchronously inside
        // retry(); nothing more is issued until it is accepted.
        this->held_chunk = chunk;
        return;
    }
    this->complete_chunk(chunk, st);
}


// Downstream outcome (GRANTED or DONE) of an issued chunk, per the per-burst
// write-ack contract.
void IoV2BeatWidthAdapter::complete_chunk(WriteChunk *chunk, vp::IoReqStatus st)
{
    WriteBurst *burst = chunk->burst;
    vp::IoReq *r = chunk->req;

    if (st == vp::IO_REQ_GRANTED)
    {
        // The target consumed the chunk and frees it. A non-last chunk gets
        // no resp at all; the last chunk's burst ack arrives in resp_handler,
        // correlated by initiator. Either way the wrapper is done.
        chunk->req = nullptr;
        this->free_chunk(chunk);
        return;
    }

    // Inline DONE: ownership never transferred — the chunk req is still ours.
    bool is_last = chunk->is_last;
    int64_t latency = r->get_full_latency();
    if (r->get_resp_status() == vp::IO_RESP_INVALID)
    {
        burst->status = vp::IO_RESP_INVALID;
    }
    else
    {
        // On a non-last chunk an inline DONE is only legal as the INVALID
        // escape hatch (io_v2.hpp "Error escape hatch").
        this->traces.assert(is_last,
            "inline DONE with OK status on a non-last write chunk (req=%p)", r);
    }
    r->free();
    chunk->req = nullptr;
    this->free_chunk(chunk);

    if (is_last)
    {
        // Inline burst completion: final status and latency taken from the
        // last chunk itself.
        this->complete_write_burst(burst, latency);
    }
    // Aborted burst (non-last INVALID DONE): simplest behavior — latch the
    // error and keep issuing the remaining chunks; the single final ack
    // carries INVALID.
}


// The whole downstream burst has completed (last-chunk inline DONE or the
// downstream burst ack): schedule the SINGLE upstream ack.
//
// TIMING: this is the "final stride only" degradation described in the file
// header — the pre-change per-stride schedule cannot be reconstructed (the
// downstream chunk acks that drove it no longer exist), so the ack is
// scheduled from the burst completion with the pre-existing per-ack
// arithmetic: ready = now + max(1, latency), serialized on ack_cursor.
void IoV2BeatWidthAdapter::complete_write_burst(WriteBurst *burst,
                                                int64_t latency_cycles)
{
    int64_t now = this->clock.get_cycles();
    if (this->ack_cursor < now)
        this->ack_cursor = now;
    int64_t ready = now + std::max((int64_t)1, latency_cycles);
    if (ready <= this->ack_cursor)
        ready = this->ack_cursor + 1;
    this->ack_cursor = ready;

    this->ack_pending.push_back(PendingAck{
        burst->base_addr, burst->total, burst->burst_id,
        burst->initiator, burst->status, ready,
    });

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Write burst complete (addr=0x%lx, size=%lu, status=%d, ack_ready=%ld)\n",
        burst->base_addr, burst->total, burst->status, (long)ready);

    // Everything the ack needs is snapshotted — release the record.
    auto it = std::find(this->live_bursts.begin(), this->live_bursts.end(),
                        burst);
    if (it != this->live_bursts.end())
    {
        this->live_bursts.erase(it);
    }
    this->free_write_burst(burst);

    this->reschedule_fsm();
}


void IoV2BeatWidthAdapter::emit_ack(const PendingAck &ack)
{
    // The single per-burst upstream ack: a distinct data-less object drawn
    // from the size-0 pool (data is caller-managed there — set it NULL on
    // every allocation). The upstream master consumes the status and frees
    // it. addr/size carry the burst base/total, informational only.
    vp::IoReq *r = this->desc_allocator->alloc();
    r->prepare();
    r->set_addr(ack.addr);
    r->set_data(nullptr);
    r->set_size(ack.size);
    r->set_opcode(vp::WRITE);
    r->is_first = true;
    r->is_last = true;
    r->burst_id = ack.burst_id;
    r->set_resp_status(ack.status);
    r->initiator = ack.initiator;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit write burst ack (ack=%p, addr=0x%lx, size=%lu, status=%d)\n",
        r, ack.addr, ack.size, ack.status);

    if (this->in.resp(r) == vp::IO_RESP_DENIED)
    {
        // Upstream busy: the ack is ours until accepted — hold it and re-send
        // it on resp_retry.
        this->resp_held = true;
        this->held_req = r;
    }
}


void IoV2BeatWidthAdapter::maybe_unblock_write()
{
    if (this->up_write_blocked && this->chunk_queue.size() < this->write_chunk_limit)
    {
        this->up_write_blocked = false;
        // The master re-sends its held write synchronously inside this call.
        this->in.retry(vp::IO_RETRY_WRITE);
    }
}


// ---------------------------------------------------------------------------
// Pump
// ---------------------------------------------------------------------------

void IoV2BeatWidthAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatWidthAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    // Emit due upstream beats/acks. Stop the instant one is back-pressured
    // (emit_* sets resp_held): the held beat must be re-sent first, from
    // resp_retry_in_handler, before anything else goes upstream.
    // One upstream read beat per cycle, from the burst whose next beat has
    // been due the longest (the bursts in creation order on a tie).
    if (!self->resp_held && self->read_cursor < now)
    {
        ReadBurst *pick = nullptr;
        int64_t pick_ready = INT64_MAX;
        for (ReadBurst *burst : self->live_reads)
        {
            int64_t ready = self->next_read_beat_ready(burst);
            if (ready <= now && ready < pick_ready)
            {
                pick = burst;
                pick_ready = ready;
            }
        }
        if (pick != nullptr)
        {
            self->read_cursor = now;
            self->emit_read_beat(pick);
        }
    }
    while (!self->resp_held && !self->ack_pending.empty()
           && self->ack_pending.front().ready_cycle <= now)
    {
        PendingAck ack = self->ack_pending.front();
        self->ack_pending.pop_front();
        self->emit_ack(ack);
    }

    self->issue_pending_chunks();

    // Room freed in the upstream read backlog: let the downstream producer
    // re-send the beat it is holding (synchronously inside resp_retry()).
    if (self->dn_read_blocked
        && self->rx_bytes < self->read_pending_limit * (size_t)self->input_width)
    {
        self->dn_read_blocked = false;
        self->out.resp_retry(vp::IO_RETRY_READ);
    }

    self->reschedule_fsm();
}


void IoV2BeatWidthAdapter::reschedule_fsm()
{
    // Blocked on upstream back-pressure: nothing can drain until resp_retry
    // releases the held beat, which reschedules us itself.
    if (this->resp_held)
    {
        return;
    }
    int64_t now = this->clock.get_cycles();
    int64_t next = INT64_MAX;
    for (ReadBurst *burst : this->live_reads)
    {
        int64_t ready = this->next_read_beat_ready(burst);
        if (ready != INT64_MAX)
        {
            // Not before the cycle after the last upstream read beat.
            next = std::min(next, std::max(ready, this->read_cursor + 1));
        }
    }
    if (!this->ack_pending.empty())
    {
        next = std::min(next, this->ack_pending.front().ready_cycle);
    }
    if (!this->chunk_queue.empty() && this->held_chunk == nullptr)
    {
        next = std::min(next, std::max(now, this->chunk_issue_cursor) + 1);
    }
    if (this->dn_read_blocked
        && this->rx_bytes < this->read_pending_limit * (size_t)this->input_width)
    {
        next = std::min(next, now + 1);
    }
    if (next == INT64_MAX)
    {
        return;
    }
    this->fsm_event.enqueue(std::max(next - now, (int64_t)1));
}


void IoV2BeatWidthAdapter::resp_retry_in_handler(vp::Block *__this,
                                                 vp::IoRetryChannel /*channel*/)
{
    auto *self = static_cast<IoV2BeatWidthAdapter *>(__this);
    if (!self->resp_held)
    {
        return;
    }
    // The io_v2 contract requires the re-send to happen synchronously inside
    // the retry callback.
    if (self->in.resp(self->held_req) == vp::IO_RESP_DENIED)
    {
        return;   // still busy; keep holding
    }
    self->resp_held = false;
    self->held_req = nullptr;
    self->reschedule_fsm();
}


// ---------------------------------------------------------------------------
// Backdoor debug: transparent pass-through to the downstream
// ---------------------------------------------------------------------------

vp::DebugMemIf *IoV2BeatWidthAdapter::resolve_debug_mem()
{
    std::vector<vp::SlavePort *> finals = this->out.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}


int IoV2BeatWidthAdapter::debug_mem_access(uint64_t addr, uint8_t *data,
                                           uint64_t size, bool is_write)
{
    vp::DebugMemIf *target = this->resolve_debug_mem();
    return target ? target->debug_mem_access(addr, data, size, is_write) : -1;
}


void IoV2BeatWidthAdapter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
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


// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void IoV2BeatWidthAdapter::reset(bool active)
{
    if (!active)
    {
        return;
    }

    // Read side: scheduled upstream beats, partial accumulation beats and
    // still-live downstream descriptors are ours (allocator-backed) — return
    // them to their pools. The upstream requests are the initiator's, not ours.
    for (ReadBurst *burst : this->live_reads)
    {
        for (RxSegment &seg : burst->rx)
        {
            seg.beat->free();
        }
        burst->rx.clear();
        if (burst->dn_req != nullptr)
        {
            burst->dn_req->free();
        }
        this->free_read_burst(burst);
    }
    this->live_reads.clear();
    this->read_cursor = -1;
    this->rx_bytes = 0;
    this->dn_read_blocked = false;

    // Write side: chunks queued, held or under construction are ours
    // (payload pool) — free them. Chunks GRANTED downstream belong to the
    // target (which frees them) and are no longer tracked here. The upstream
    // beats were already consumed and freed at submit; only the parked burst
    // records remain, and pending acks are pure snapshots (the ack object is
    // only allocated at emit time).
    auto drop_chunk = [&](WriteChunk *chunk)
    {
        if (chunk->req != nullptr)
        {
            chunk->req->free();
            chunk->req = nullptr;
        }
        this->free_chunk(chunk);
    };
    if (this->cur_chunk != nullptr)
    {
        drop_chunk(this->cur_chunk);
        this->cur_chunk = nullptr;
    }
    for (WriteChunk *chunk : this->chunk_queue) drop_chunk(chunk);
    this->chunk_queue.clear();
    if (this->held_chunk != nullptr)
    {
        drop_chunk(this->held_chunk);
        this->held_chunk = nullptr;
    }
    for (WriteBurst *burst : this->live_bursts)
    {
        this->free_write_burst(burst);
    }
    this->live_bursts.clear();
    this->open_burst = nullptr;
    this->ack_pending.clear();
    this->chunk_issue_cursor = -1;
    this->ack_cursor = -1;
    this->up_write_blocked = false;

    // A held (back-pressured) upstream beat is ours either way: a read beat
    // is an allocator-backed sub-object and a write burst ack is our own
    // size-0-pool object — free it.
    if (this->resp_held && this->held_req != nullptr)
    {
        this->held_req->free();
    }
    this->resp_held = false;
    this->held_req = nullptr;

    if (this->fsm_event.is_enqueued())
    {
        this->fsm_event.cancel();
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatWidthAdapter(config);
}
