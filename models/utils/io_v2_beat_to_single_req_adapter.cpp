// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_beat_to_single_req_adapter.hpp"

#include <algorithm>

// ===========================================================================
// Method roster / call graph
// ===========================================================================
// The adapter has two io_v2 ports — the slave "input" (`in`, faces the upstream
// beat master) and the master "output" (`out`, faces the downstream single-req
// slave) — plus one ClockEvent (`fsm_event`). Every method is reached from one
// of those, or is an internal helper called by them:
//
//   in.req           -> req_handler          (upstream submits a read/write)
//   in.resp_retry    -> resp_retry_in_handler(upstream can take responses again)
//   out.resp         -> resp_handler         (downstream async response arrives)
//   out.retry        -> retry_handler        (downstream ready again after a DENY)
//   fsm_event        -> fsm_handler          (per-cycle pump for scheduled beats)
//
// Internal helpers (not entry points):
//   issue_one_sub_read / issue_pending_sub_reads  — generate & send sub-reads
//   complete_read_beat / emit_read_beat           — read response -> upstream beat
//   on_read_burst_complete                        — free a burst slot
//   schedule_chunk (queue burst) / emit_write_beat (cursor) — WRITE path
//   reschedule_fsm                                — (re)arm fsm_event
//
// Typical READ flow:  req_handler (queue burst) -> issue_pending_sub_reads ->
//   issue_one_sub_read (1/cycle) -> [downstream] -> resp_handler / inline DONE ->
//   complete_read_beat (schedule) -> fsm_handler -> emit_read_beat (-> upstream).
// Typical WRITE flow: req_handler (forward whole) -> schedule_chunk (spread acks)
//   -> fsm_handler -> emit_write_beat (cursor, -> upstream).
// ===========================================================================


// Constructor. Instantiated by the framework via gv_new() during platform build.
// Pulls beat_width / max_read_bursts from the generated config tree (copied into
// `cfg` by the Component(config, cfg) base ctor) and declares the two ports:
//   "input"  (slave)  — bound to the upstream IoV2Beat master,
//   "output" (master) — bound to the downstream IoV2SingleReq slave.
IoV2BeatToSingleReqAdapter::IoV2BeatToSingleReqAdapter(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      in(&IoV2BeatToSingleReqAdapter::req_handler, &IoV2BeatToSingleReqAdapter::resp_retry_in_handler),
      out(&IoV2BeatToSingleReqAdapter::retry_handler, &IoV2BeatToSingleReqAdapter::resp_handler),
      fsm_event(this, &IoV2BeatToSingleReqAdapter::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->beat_width = (int)this->cfg.beat_width;
    if (this->beat_width <= 0)
    {
        this->trace.fatal("IoV2BeatToSingleReqAdapter requires beat_width > 0 (got %d)\n",
                          this->beat_width);
    }

    if (this->cfg.max_read_bursts > 0)
    {
        this->max_read_bursts = (int)this->cfg.max_read_bursts;
    }

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}


// Slave "input" req callback: the upstream beat master submits one access.
// Returns IO_REQ_GRANTED (accepted; responses stream back via resp()) or
// IO_REQ_DENIED (back-pressure; the master re-sends on retry) — never DONE.
//   READ : queue the whole burst (bounded by max_read_bursts) and kick issuing;
//          the per-beat sub-reads are generated later, one per cycle.
//   WRITE: forward the whole request as-is to the downstream slave and turn its
//          single response into the per-beat ack stream (schedule_chunk).
vp::IoReqStatus IoV2BeatToSingleReqAdapter::req_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatToSingleReqAdapter *>(__this);
    uint64_t size = req->get_size();

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d, burst_id=%ld)\n",
        req, req->get_addr(), size, req->get_is_write() ? 1 : 0,
        (long)req->burst_id);

    // READ: the upstream beat master sends a single read descriptor carrying the
    // full burst size. Push it whole onto the burst queue; sub-reads are then
    // generated on the fly (one per cycle) and each becomes one upstream beat.
    if (!req->get_is_write())
    {
        // Bound the number of read bursts in flight (HW r_id-FIFO analogue):
        // back-pressure the request channel when the limit is hit. The master
        // re-sends on retry, which we raise from on_read_burst_complete().
        if (self->outstanding_read_bursts >= self->max_read_bursts)
        {
            self->read_blocked = true;
            self->trace.msg(vp::Trace::LEVEL_TRACE,
                "Read burst denied — %d bursts already in flight (limit %d)\n",
                self->outstanding_read_bursts, self->max_read_bursts);
            return vp::IO_REQ_DENIED;
        }

        self->outstanding_read_bursts++;
        int nb_beats = size == 0 ? 1
            : (int)((size + self->beat_width - 1) / self->beat_width);
        self->issue_bursts.push_back(ReadBurst{
            req, req->get_addr(), req->get_data(), size, req->burst_id,
            nb_beats, 0, 0});
        self->issue_pending_sub_reads();
        self->reschedule_fsm();
        return vp::IO_REQ_GRANTED;
    }

    // WRITE: forwarded as-is. Register the in-flight slot before forwarding so
    // the slave can respond inline or same-stack.
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
        // per-beat resp() stream now. get_full_latency() = head latency +
        // bandwidth occupancy (duration), the latter max-combined across hops.
        self->schedule_chunk(req, req->get_data(), size, req->get_full_latency());
        return vp::IO_REQ_GRANTED;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        self->in_flight.erase(req);
    }
    return st;
}


// Master "output" resp callback: the downstream slave delivers an ASYNC response
// (it had returned IO_REQ_GRANTED to our out.req()). Routes it to the read or the
// write path by identity, then returns IO_RESP_ACCEPTED — the adapter buffers and
// paces the upstream stream itself, so it never back-pressures the downstream.
// (Inline IO_REQ_DONE responses do NOT come here; they are handled where out.req
// is called — see issue_one_sub_read and req_handler's write branch.)
vp::IoRespAck IoV2BeatToSingleReqAdapter::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatToSingleReqAdapter *>(__this);

    // The adapter always accepts the downstream response (it buffers it and paces
    // the upstream stream itself), so it never back-pressures downstream.

    // Read sub-read async completion. A single-req slave is in-order, so the
    // response must be the OLDEST outstanding sub-read — no search, just the FIFO
    // front. (The HW axi2mem relies on the same in-order property.) The beat
    // framing is derived from the front burst's cursor inside complete_read_beat.
    if (!self->issued.empty() && self->issued.front() == req)
    {
        self->issued.pop_front();
        self->complete_read_beat(req, req->get_resp_status(),
                                 req->get_full_latency());
        self->issue_pending_sub_reads();
        self->reschedule_fsm();
        return vp::IO_RESP_ACCEPTED;
    }

    // Not the oldest read → it must be a WRITE async response (tracked in
    // in_flight). A read response that is not the FIFO front means the slave
    // reordered, which the single-req contract forbids — catch it loudly.
    self->traces.assert(self->in_flight.find(req) != self->in_flight.end(),
        "SingleReq slave responded out of order or for an unknown request "
        "(req=%p) — read responses must arrive in issue order", req);

    self->schedule_chunk(req, req->get_data(), req->get_size(),
                         req->get_full_latency());
    return vp::IO_RESP_ACCEPTED;
}


// Master "output" retry callback: the downstream slave is ready again after it
// DENIED one of our out.req() calls. Re-sends the held sub-read synchronously (a
// hard io_v2 requirement) and also forwards the retry to the upstream master, in
// case the master is holding a DENIED write of its own to re-send.
void IoV2BeatToSingleReqAdapter::retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    auto *self = static_cast<IoV2BeatToSingleReqAdapter *>(__this);

    // A denied downstream sub-read can now be re-issued. The io_v2 contract
    // requires the re-send to happen synchronously inside retry(). issued_beats
    // was not advanced on the DENY, so issue_one_sub_read() regenerates the exact
    // same beat from the burst cursor — no held-job state needed.
    if (self->sub_read_denied)
    {
        self->sub_read_denied = false;
        self->issue_one_sub_read();
        self->issue_pending_sub_reads();
        self->reschedule_fsm();
    }

    // Forward upstream too: the beat master may have its own DENIED writes to
    // re-send.
    self->in.retry(channel);
}


// Generate and send exactly ONE beat-sized read sub-read downstream, taken from
// the head of the issue chain. Returns true if a sub-read was issued, false if
// there was nothing to issue or the downstream DENIED it.
//   Called by issue_pending_sub_reads() (the paced, ≤1/cycle path) and directly
//   by retry_handler() (the synchronous re-send after a downstream DENY).
//   On GRANTED the req is parked in `issued` to await its async resp(); on inline
//   DONE it is completed immediately; on DENY it is dropped and re-tried later
//   (issued_beats is left unadvanced so the same beat regenerates).
bool IoV2BeatToSingleReqAdapter::issue_one_sub_read()
{
    // The burst being issued is always the head of the issue chain (a burst is
    // moved to drain_bursts the moment its last sub-read is issued, so the head
    // is never fully issued) — no scanning.
    if (this->issue_bursts.empty())
    {
        return false;   // nothing to issue
    }
    ReadBurst &b = this->issue_bursts.front();

    // Generate the next sub-read on the fly from the beat index (HW
    // address-generator model — nothing is pre-expanded into a job object).
    int i = b.issued_beats;
    uint64_t offset = (uint64_t)i * (uint64_t)this->beat_width;
    uint64_t beat = b.total_size > offset
        ? std::min<uint64_t>(b.total_size - offset, (uint64_t)this->beat_width)
        : 0;

    // One heap object per sub-read, sent downstream as a single-beat request.
    // Once its response lands, the SAME object is forwarded upstream as the beat
    // (no second allocation); the terminal master frees it.
    vp::IoReq *r = new vp::IoReq();
    r->prepare();
    r->set_addr(b.base_addr + offset);
    r->set_data(b.base_data + offset);
    r->set_size(beat);
    r->set_is_write(false);
    r->is_first = true;       // single-beat to the slave
    r->is_last  = true;
    r->burst_id = -1;
    r->initiator = b.up_req->initiator;

    vp::IoReqStatus st = this->out.req(r);

    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream full: drop this object and re-issue on retry. issued_beats is
        // NOT advanced, so the retry regenerates the exact same beat.
        this->sub_read_denied = true;
        delete r;
        return false;
    }

    b.issued_beats++;
    this->read_issue_last_cycle = this->clock.get_cycles();

    // Once fully issued, the burst leaves the issue chain for the drain chain.
    // Do this BEFORE completing an inline DONE so completion finds it there.
    if (b.issued_beats == b.nb_beats)
    {
        this->drain_bursts.push_back(b);
        this->issue_bursts.pop_front();
    }

    if (st == vp::IO_REQ_DONE)
    {
        // Inline completion. A single-req slave is consistent within a burst, so
        // no async sub-read may be outstanding ahead of this one.
        this->traces.assert(this->issued.empty(),
            "SingleReq slave mixed inline and async responses within a burst");
        this->complete_read_beat(r, r->get_resp_status(), r->get_full_latency());
    }
    else
    {
        // GRANTED: track the req in issue order, awaiting the async resp().
        this->issued.push_back(r);
    }
    return true;
}


// Paced entry point for issuing: send at most ONE sub-read this cycle, subject to
// the 1/cycle pacing, the outstanding-window (max_sub_outstanding) and not being
// blocked on a downstream DENY. Called from every place that may have created new
// work or freed a slot: req_handler (new read burst), resp_handler (a sub-read
// completed), retry_handler, and fsm_handler (per-cycle refill).
void IoV2BeatToSingleReqAdapter::issue_pending_sub_reads()
{
    // Pace issuance at one sub-read per cycle (read_issue_last_cycle): the fsm
    // re-ticks every cycle while bursts remain, so the rest drip out on later
    // cycles. This stops a series of bandwidth routers from compounding their
    // per-request waits while still keeping round-trip-many sub-reads outstanding.
    int64_t now = this->clock.get_cycles();
    if (now <= this->read_issue_last_cycle)
    {
        return;
    }
    if (this->sub_read_denied
        || (int)this->issued.size() >= this->max_sub_outstanding)
    {
        return;
    }
    this->issue_one_sub_read();
}


// Turn a completed read sub-read response into a scheduled upstream beat. Called
// once per sub-read response, from issue_one_sub_read() (inline DONE) and from
// resp_handler() (async resp). `req` is the internal sub-read object. It picks the
// object delivered upstream (see below), stamps the burst framing (is_first/
// is_last/burst_id/status) from the completing burst's completed_beats cursor,
// queues a ReadyBeat at its modeled ready cycle (read_last_sched_cycle keeps beats
// ≥1 cycle apart), and drops the burst from the drain chain once its last response
// has been received.
void IoV2BeatToSingleReqAdapter::complete_read_beat(vp::IoReq *req,
                                        vp::IoRespStatus status,
                                        int64_t latency_cycles)
{
    // Responses arrive in burst order, so the completing burst is the oldest in
    // flight: the front of the drain chain, or — while a single burst is still
    // being issued and its early responses already arrive — the issue head. Its
    // completed_beats cursor gives this beat's burst position.
    ReadBurst &b = this->drain_bursts.empty() ? this->issue_bursts.front()
                                              : this->drain_bursts.front();
    int i = b.completed_beats;

    int64_t now = this->clock.get_cycles();
    if (this->read_last_sched_cycle < now)
        this->read_last_sched_cycle = now;

    int64_t ready = now + std::max((int64_t)1, latency_cycles);
    if (ready <= this->read_last_sched_cycle)
        ready = this->read_last_sched_cycle + 1;
    this->read_last_sched_cycle = ready;

    // The sub-read object IS the upstream response beat (distinct per beat, even
    // for a single-beat read — no round-trip of the master's request). The
    // initiator owns its request and frees it on the last response; it correlates
    // each response to its request by req->initiator (set at issue time), and
    // frees the response objects. So the adapter never frees up_req nor the beat.
    // A master that needs its own object back (a CPU LSU) sits behind a collapse
    // adapter, so its raw request never reaches here.
    req->is_first = (i == 0);
    req->is_last  = (i == b.nb_beats - 1);
    req->burst_id = b.burst_id;
    req->set_resp_status(status == vp::IO_RESP_INVALID ? vp::IO_RESP_INVALID
                                                       : vp::IO_RESP_OK);

    this->read_pending.push_back(ReadyBeat{req, ready});

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Read beat ready (beat=%p, beat=%d/%d, ready=%ld, first=%d, last=%d)\n",
        req, i, b.nb_beats, (long)ready, req->is_first ? 1 : 0, req->is_last ? 1 : 0);

    b.completed_beats++;
    if (b.completed_beats >= b.nb_beats)
    {
        // All responses received; the burst is fully issued (so it lives in the
        // drain chain) — drop it. up_req is the initiator's to free, not ours.
        this->drain_bursts.pop_front();
    }

    this->reschedule_fsm();
}


// Forward one scheduled read beat upstream via in.resp(). Called by fsm_handler()
// when the beat's ready cycle has arrived. If the upstream master back-pressures
// (IO_RESP_DENIED), the beat is held and re-sent later from resp_retry_in_handler.
// The beat object is owned by the consumer (the initiator) once accepted — the
// adapter frees nothing here. On the burst's LAST beat it only releases the
// in-flight slot (bookkeeping for max_read_bursts); the initiator frees its own
// request and the response beats.
void IoV2BeatToSingleReqAdapter::emit_read_beat(const ReadyBeat &rb)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit read beat (beat=%p, size=%lu, first=%d, last=%d)\n",
        rb.beat, rb.beat->get_size(),
        rb.beat->is_first ? 1 : 0, rb.beat->is_last ? 1 : 0);

    bool last = rb.beat->is_last;

    if (this->in.resp(rb.beat) == vp::IO_RESP_DENIED)
    {
        // Upstream busy: hold this beat and re-send it on resp_retry; on the last
        // beat, release the slot when it finally lands.
        this->resp_held = true;
        this->held_req = rb.beat;
        this->held_read_last = last;
        return;
    }

    if (last)
    {
        this->on_read_burst_complete();   // release the in-flight slot
    }
}


// Release one read-burst in-flight slot once its last beat has reached the master.
// Called from emit_read_beat() (direct accept) and resp_retry_in_handler() (when a
// held last beat is finally accepted). If the read request channel was previously
// back-pressured at the burst limit, this wakes the master with in.retry().
void IoV2BeatToSingleReqAdapter::on_read_burst_complete()
{
    // A read burst's last beat reached the master: free its in-flight slot and,
    // if the read request channel was back-pressured at the burst limit, tell the
    // master it can re-send (synchronously, per the io_v2 contract).
    this->outstanding_read_bursts--;
    if (this->read_blocked)
    {
        this->read_blocked = false;
        this->in.retry(vp::IO_RETRY_READ);
    }
}


// WRITE path. Spread a single whole-write response into the per-beat ack stream
// the upstream beat master expects. Called from req_handler() (inline DONE write)
// and resp_handler() (async write response). Each ack beat round-trips the
// master's own request object (see emit_write_beat); beats are emitted 1/cycle so the last
// lands at now+full_latency. Also enforces the SingleReq contract (one response
// per request, covering its whole size) in asserts builds.
void IoV2BeatToSingleReqAdapter::schedule_chunk(vp::IoReq *req, uint8_t *data,
                                     uint64_t size, int64_t latency_cycles)
{
    auto it = this->in_flight.find(req);
    if (it == this->in_flight.end())
    {
        // SingleReq contract: every response must correspond to a request we
        // forwarded and are still tracking. An untracked response means the
        // slave streamed an extra beat — forbidden for a single-req slave.
        this->traces.assert(false,
            "SingleReq response for an untracked request — a single-req slave "
            "must answer each forwarded request with exactly one response "
            "(req=%p, size=%lu)", req, size);
        this->trace.force_warning(
            "Response for unknown req (req=%p, size=%lu) — dropping\n",
            req, size);
        return;
    }
    InFlight &inf = it->second;

    // SingleReq contract: the whole request is answered by exactly one
    // single-beat response (this call) covering its full size. A multi-beat slave
    // would instead deliver the response in several chunks, accumulating
    // bytes_routed across calls. Checked only in asserts/debug builds.
    this->traces.assert(inf.bytes_routed == 0 && size == inf.total_size,
        "SingleReq slave must answer the whole request in one response "
        "(req=%p, total=%lu, this_resp=%lu, already_routed=%lu)",
        req, inf.total_size, size, inf.bytes_routed);

    int64_t now = this->clock.get_cycles();
    int n = (int)((size + this->beat_width - 1) / this->beat_width);
    if (n <= 0) n = 1;

    // Bandwidth model: the slave's latency annotation is the time-to-completion of
    // the whole chunk. step is the per-beat slave time so the LAST beat lands at
    // now+latency; first_ready is when beat 0 is due. Nothing is pre-expanded — the
    // burst is queued and fsm_handler emits one beat per cycle from its cursor.
    int64_t step = (n > 1) ? std::max((int64_t)1, latency_cycles / n) : (int64_t)1;
    int64_t first_ready = now + latency_cycles - (int64_t)(n - 1) * step;
    if (first_ready < now + 1) first_ready = now + 1;

    this->write_bursts.push_back(WriteBurst{
        req,
        data,
        inf.burst_addr,
        size,
        req->burst_id,
        req->get_resp_status(),
        n,
        0,
        first_ready,
        step,
    });

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Queue write ack burst (req=%p, size=%lu, beats=%d, first_ready=%ld, step=%ld)\n",
        req, size, n, (long)first_ready, (long)step);

    this->in_flight.erase(it);

    this->reschedule_fsm();
}


// WRITE path. Generate and forward the front write burst's NEXT ack beat from its
// cursor (b.emitted_beats) — extracted on the fly, nothing pre-expanded. Called by
// fsm_handler() once the beat's ready cycle has arrived. The ack reuses the
// master's own request object (writes are one-resp-per-req, no aliasing). On
// upstream back-pressure the beat is held (held_write) and re-sent from
// resp_retry_in_handler, which advances the cursor when it finally lands; on accept
// the cursor advances here.
void IoV2BeatToSingleReqAdapter::emit_write_beat(WriteBurst &b)
{
    int i = b.emitted_beats;
    uint64_t offset = (uint64_t)i * (uint64_t)this->beat_width;
    uint64_t beat = b.total_size > offset
        ? std::min<uint64_t>(b.total_size - offset, (uint64_t)this->beat_width)
        : 0;

    vp::IoReq *req = b.up_req;
    req->set_addr(b.base_addr + offset);
    req->set_data(b.base_data + offset);
    req->set_size(beat);
    req->burst_id = b.burst_id;
    req->is_first = (i == 0);
    req->is_last  = (i == b.nb_beats - 1);
    req->set_resp_status(b.status);

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Emit write ack (req=%p, beat=%d/%d, offset=%lu, size=%lu, first=%d, last=%d)\n",
        req, i, b.nb_beats, offset, beat, req->is_first ? 1 : 0, req->is_last ? 1 : 0);

    if (this->in.resp(req) == vp::IO_RESP_DENIED)
    {
        // Upstream busy: hold the master's own request and re-send it on
        // resp_retry. Do NOT advance the cursor — resp_retry advances it once the
        // held beat is accepted. A write ack frees nothing.
        this->resp_held = true;
        this->held_req = req;
        this->held_write = true;
        this->held_read_last = false;
        return;
    }

    b.emitted_beats++;
}


// Slave "input" resp_retry callback: the upstream master can accept responses
// again, after it returned IO_RESP_DENIED to one of our in.resp() beats. Re-sends
// the single held beat synchronously (io_v2 requirement); if accepted, releases the
// in-flight slot when it was a read burst's last beat and resumes draining.
// Applies to both read beats and write acks (whichever was held). Frees nothing —
// the initiator owns its request and the response objects. Does nothing if no beat
// is currently held.
void IoV2BeatToSingleReqAdapter::resp_retry_in_handler(vp::Block *__this,
                                            vp::IoRetryChannel /*channel*/)
{
    auto *self = static_cast<IoV2BeatToSingleReqAdapter *>(__this);
    if (!self->resp_held)
    {
        return;
    }
    // The io_v2 contract requires the re-send to happen synchronously inside the
    // retry callback.
    if (self->in.resp(self->held_req) == vp::IO_RESP_DENIED)
    {
        return;   // still not ready — keep holding
    }
    // The held beat just landed. Advance the matching cursor:
    if (self->held_write)
    {
        // Front write burst's current ack beat: advance it, drop the burst once
        // its last beat has been accepted.
        WriteBurst &b = self->write_bursts.front();
        b.emitted_beats++;
        if (b.emitted_beats >= b.nb_beats)
        {
            self->write_bursts.pop_front();
        }
    }
    else if (self->held_read_last)
    {
        // Read burst's last beat: release the in-flight slot.
        self->on_read_burst_complete();
    }
    self->resp_held = false;
    self->held_req = nullptr;
    self->held_read_last = false;
    self->held_write = false;
    // Resume draining the rest of the beats (next cycle — one beat per cycle).
    self->reschedule_fsm();
}


// Per-cycle pump, run by fsm_event at the cycle reschedule_fsm() armed. It (1)
// emits every read beat and write-ack beat whose ready cycle has arrived — read
// and write are independent channels, each ≤1 beat/cycle by construction — and
// (2) refills the downstream issue pipeline. Stops emitting the instant a beat is
// back-pressured (resp_held), since the held beat must go out first (from
// resp_retry_in_handler) before any later one. Re-arms itself via reschedule_fsm.
void IoV2BeatToSingleReqAdapter::fsm_handler(vp::Block *__this, vp::ClockEvent *)
{
    auto *self = static_cast<IoV2BeatToSingleReqAdapter *>(__this);
    int64_t now = self->clock.get_cycles();

    // Emit due read beats. Stop the instant one is back-pressured (resp_held):
    // the held beat must be re-sent first, from resp_retry_in_handler.
    while (!self->resp_held && !self->read_pending.empty()
           && self->read_pending.front().ready_cycle <= now)
    {
        ReadyBeat rb = self->read_pending.front();
        self->read_pending.pop_front();
        self->emit_read_beat(rb);
    }

    // Emit the front write burst's next ack beat (independent channel), at most one
    // per cycle, generated on the fly from its cursor. The burst is dropped once
    // its last beat has been sent; a backlog drains one beat per cycle (reschedule
    // re-arms for now+1 while a due beat remains).
    if (!self->resp_held && !self->write_bursts.empty())
    {
        WriteBurst &b = self->write_bursts.front();
        int64_t ready = b.first_ready + (int64_t)b.emitted_beats * b.step;
        if (ready <= now)
        {
            self->emit_write_beat(b);
            if (!self->resp_held && b.emitted_beats >= b.nb_beats)
            {
                self->write_bursts.pop_front();
            }
        }
    }

    // Refill the downstream read pipeline (a slot may have freed, or
    // back-pressure may have cleared).
    self->issue_pending_sub_reads();

    self->reschedule_fsm();
}


// (Re)arm fsm_event for the earliest future work: the soonest due read/write beat
// and/or the next cycle if a sub-read can still be issued. Called at the end of
// every state-changing method. A no-op while back-pressured (resp_held) — the
// resp_retry path will re-arm — or if the event is already enqueued.
void IoV2BeatToSingleReqAdapter::reschedule_fsm()
{
    // Blocked on upstream back-pressure: nothing can drain until resp_retry
    // releases the held beat, which reschedules us itself.
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
    if (!this->read_pending.empty())
    {
        next = std::min(next, this->read_pending.front().ready_cycle);
    }
    if (!this->write_bursts.empty())
    {
        const WriteBurst &b = this->write_bursts.front();
        next = std::min(next, b.first_ready + (int64_t)b.emitted_beats * b.step);
    }
    // A burst still to issue (head of the issue chain; room in the window, not
    // held) wants the next cycle.
    if (!this->issue_bursts.empty() && !this->sub_read_denied
        && (int)this->issued.size() < this->max_sub_outstanding)
    {
        next = std::min(next, now + 1);
    }
    if (next == INT64_MAX)
    {
        return;
    }
    this->fsm_event.enqueue(std::max(next - now, (int64_t)1));
}


// --- Backdoor debug path (DebugMemIf) ---------------------------------------
// These make the adapter transparent to out-of-band accesses (gvsoc_control
// mem_read/mem_write, GDB): they bypass the timed beat machinery entirely and
// forward straight to the downstream component. Called by the debug subsystem,
// never from the simulated request path.

// Resolve the downstream component's DebugMemIf (the thing bound to "output").
vp::DebugMemIf *IoV2BeatToSingleReqAdapter::resolve_debug_mem()
{
    std::vector<vp::SlavePort *> finals = this->out.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}


int IoV2BeatToSingleReqAdapter::debug_mem_access(uint64_t addr, uint8_t *data,
                                      uint64_t size, bool is_write)
{
    vp::DebugMemIf *target = this->resolve_debug_mem();
    return target ? target->debug_mem_access(addr, data, size, is_write) : -1;
}


void IoV2BeatToSingleReqAdapter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
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


// Component reset hook, called by the framework (active=true on entering reset).
// Drops all queued work and frees the objects the adapter still owns (in-flight
// sub-reads and not-yet-emitted read beats); the master-owned burst requests are
// not freed here. Re-initialises the pacing cursors and cancels fsm_event.
void IoV2BeatToSingleReqAdapter::reset(bool active)
{
    if (active)
    {
        this->write_bursts.clear();
        this->in_flight.clear();
        this->issue_bursts.clear();
        this->drain_bursts.clear();
        this->outstanding_read_bursts = 0;
        this->read_blocked = false;
        // Sub-reads still owned by the adapter (issued downstream or completed
        // and awaiting upstream emit) are freed; the master-owned burst requests
        // are not (freed at teardown elsewhere).
        for (auto *r : this->issued)
        {
            delete r;
        }
        this->issued.clear();
        // Scheduled-but-not-yet-emitted read beats are sub-reads the adapter still
        // owns (not yet handed to the consumer), so free them here.
        for (auto &rb : this->read_pending)
        {
            delete rb.beat;
        }
        this->read_pending.clear();
        this->sub_read_denied = false;
        this->resp_held = false;
        this->held_req = nullptr;
        this->held_read_last = false;
        this->held_write = false;
        this->read_last_sched_cycle = -1;
        this->read_issue_last_cycle = -1;
        if (this->fsm_event.is_enqueued())
        {
            this->fsm_event.cancel();
        }
    }
}


// Component factory. The framework calls this (resolved from the model's .so) to
// instantiate the adapter when it auto-inserts it on an IoV2Beat->IoV2SingleReq
// binding.
extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatToSingleReqAdapter(config);
}
