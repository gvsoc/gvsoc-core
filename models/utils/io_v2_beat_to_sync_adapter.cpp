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

    // Forward the whole burst to the sync slave. By the IoV2Sync contract it
    // serves any size inline and must complete with IO_REQ_DONE (never
    // GRANTED/DENIED). Assert it honoured the contract (asserts/debug builds).
    vp::IoReqStatus st = self->out.req(req);
    self->traces.assert(st == vp::IO_REQ_DONE,
        "sync slave must reply IO_REQ_DONE inline (got %d)", (int)st);

    // Queue the response; the FSM streams it back as one resp() beat per cycle.
    self->pending.push_back(req);

    // Start the FSM after the head latency. enqueue() keeps the earliest pending
    // cycle, so while already streaming (enqueued at +1) this is a no-op and the
    // queued response just drains continuously behind the active one.
    self->fsm_event.enqueue(std::max((int64_t)1, req->get_full_latency()));

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


bool IoV2BeatToSyncAdapter::emit_beat(bool is_first, bool is_last, uint64_t beat)
{
    // Deliver on the master's own request for a write (every ack) and for the
    // LAST beat of any read (which covers a single-beat read): once a beat is the
    // last, no sibling beat can alias it, and many masters key completion on
    // getting that exact object back. Earlier read beats each need a distinct
    // object — a downstream that queues responses (e.g. a clock bridge) must not
    // alias a reused one. The terminal master frees every object it receives, so
    // the adapter itself never allocates a separate descriptor to free.
    vp::IoReq *r;
    if (this->cur_req->get_is_write() || is_last)
    {
        r = this->cur_req;
    }
    else
    {
        r = new vp::IoReq();
        r->set_is_write(false);
        r->initiator = this->cur_req->initiator;
    }

    r->set_addr(this->cur_addr + this->cur_offset);
    r->set_data(this->cur_data + this->cur_offset);
    r->set_size(beat);
    r->burst_id = this->cur_burst_id;
    r->is_first = is_first;
    r->is_last = is_last;
    r->set_resp_status(this->cur_status);

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit beat (req=%p, offset=%lu, size=%lu, first=%d, last=%d, write=%d)\n",
        r, this->cur_offset, beat, is_first ? 1 : 0, is_last ? 1 : 0,
        this->cur_req->get_is_write() ? 1 : 0);

    if (this->in.resp(r) == vp::IO_RESP_DENIED)
    {
        // Upstream back-pressure: hold this object and its beat size (read now,
        // before the master can take ownership and reuse it) and re-send on
        // resp_retry. The caller must not advance the cursor.
        this->resp_held = true;
        this->held_req = r;
        this->held_beat = beat;
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
    // Accepted: advance the cursor that fsm_handler left pending, then resume.
    uint64_t beat = self->held_beat;
    self->resp_held = false;
    self->held_req = nullptr;
    self->held_beat = 0;
    self->cur_offset += beat;
    if (self->cur_offset >= self->cur_size)
    {
        self->cur_req = nullptr;   // burst fully streamed
    }
    if (self->cur_req != nullptr || !self->pending.empty())
    {
        self->fsm_event.enqueue(1);
    }
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

    // Activate the next queued response if idle.
    if (self->cur_req == nullptr)
    {
        if (self->pending.empty())
        {
            return;
        }
        vp::IoReq *req = self->pending.front();
        self->pending.pop_front();
        self->cur_req      = req;
        self->cur_data     = req->get_data();
        self->cur_size     = req->get_size();
        self->cur_addr     = req->get_addr();
        self->cur_burst_id = req->burst_id;
        self->cur_status   = req->get_resp_status();
        self->cur_offset   = 0;
    }

    // Carve and send one beat (zero-size response → a single zero-size beat).
    uint64_t beat = std::min<uint64_t>(self->cur_size - self->cur_offset,
                                       (uint64_t)self->beat_width);
    bool is_first = (self->cur_offset == 0);
    bool is_last  = (self->cur_offset + beat >= self->cur_size);

    if (!self->emit_beat(is_first, is_last, beat))
    {
        // Held on upstream back-pressure; resp_retry_in_handler advances the
        // cursor and resumes once the master accepts.
        return;
    }

    self->cur_offset += beat;
    if (self->cur_offset >= self->cur_size)
    {
        self->cur_req = nullptr;   // burst fully streamed
    }

    // More to do (this burst or a queued one)? Tick again next cycle.
    if (self->cur_req != nullptr || !self->pending.empty())
    {
        self->fsm_event.enqueue(1);
    }
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
        this->pending.clear();
        this->cur_req = nullptr;
        this->cur_offset = 0;
        this->resp_held = false;
        this->held_req = nullptr;
        this->held_beat = 0;
        this->fsm_event.cancel();   // safe even if not enqueued
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatToSyncAdapter(config);
}
