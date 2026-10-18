// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_clock_bridge.hpp"


IoV2ClockBridge::IoV2ClockBridge(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);

    js::Config *js = this->get_js_config();
    if (js::Config *c = js->get("k_src_per_dir"); c != nullptr)
        this->k_src_per_dir = c->get_int();
    if (js::Config *c = js->get("k_dst_per_dir"); c != nullptr)
        this->k_dst_per_dir = c->get_int();
    if (js::Config *c = js->get("depth"); c != nullptr)
        this->depth = c->get_int();
    if (js::Config *c = js->get("axi_channels"); c != nullptr)
        this->axi_channels = c->get_bool();

    if (this->k_src_per_dir < 0) this->k_src_per_dir = 0;
    if (this->k_dst_per_dir < 0) this->k_dst_per_dir = 0;
    if (this->depth < 1) this->depth = 1;

    this->parametric = (this->k_src_per_dir > 0 || this->k_dst_per_dir > 0);

    if (this->parametric)
    {
        this->fwd_src_event = new vp::ClockEvent(this, &IoV2ClockBridge::fwd_src_done_handler);
        this->rev_dst_event = new vp::ClockEvent(this, &IoV2ClockBridge::rev_dst_done_handler);
        this->fwd_dst_event = new vp::ClockEvent(this, &IoV2ClockBridge::fwd_dst_done_handler);
        this->rev_src_event = new vp::ClockEvent(this, &IoV2ClockBridge::rev_src_done_handler);
        this->retry_event   = new vp::ClockEvent(this, &IoV2ClockBridge::retry_event_handler);
    }
    else
    {
        this->resp_event = new vp::ClockEvent(this, &IoV2ClockBridge::resp_event_handler);
    }
}


void IoV2ClockBridge::start()
{
    // Use the remote PORT owners, not the remote contexts: with a muxed
    // peer port the remote context is the dispatch-stub port object, not
    // the component.
    auto *master_port = this->in.get_remote_port();
    auto *slave_port  = this->out.get_remote_port();
    if (master_port == nullptr || slave_port == nullptr)
    {
        this->trace.fatal("bridge not fully bound (in.bound=%d, out.bound=%d)\n",
                          master_port != nullptr, slave_port != nullptr);
        return;
    }
    this->master_engine = master_port->get_owner()->clock.get_engine();
    this->slave_engine  = slave_port->get_owner()->clock.get_engine();

    this->trace.msg(vp::Trace::LEVEL_INFO,
        "bridge mode=%s k_src=%d k_dst=%d depth=%d axi_channels=%d\n",
        this->parametric ? "parametric" : "sync_only",
        this->k_src_per_dir, this->k_dst_per_dir, this->depth, this->axi_channels);
}


void IoV2ClockBridge::reset(bool active)
{
    if (!active) return;

    if (!this->parametric)
    {
        if (this->resp_event->is_enqueued())
            this->master_engine->cancel(this->resp_event);
        this->resp_queue.clear();
        this->resp_held = false;
        this->resp_retry_owed = false;
        return;
    }

    if (this->fwd_src_event->is_enqueued())
        this->master_engine->cancel(this->fwd_src_event);
    if (this->rev_dst_event->is_enqueued())
        this->master_engine->cancel(this->rev_dst_event);
    if (this->fwd_dst_event->is_enqueued())
        this->slave_engine->cancel(this->fwd_dst_event);
    if (this->rev_src_event->is_enqueued())
        this->slave_engine->cancel(this->rev_src_event);
    this->master_engine->cancel(this->retry_event);

    // Write beats sitting in the forward queues were GRANTED to the bridge
    // (ownership transferred, upstream considers them consumed) and were not
    // yet forwarded: nobody else will ever free them, so return the
    // pool-backed ones to their pool. Beat-plane write beats are always
    // allocator-backed; a write without an allocator back-pointer is a
    // master-owned object on a non-beat (classic round-trip) flow — the
    // master still holds it, so it is not ours to free. Reads/atomics in the
    // forward queues and everything in the rev queues (responses / burst
    // acks travelling upstream) are dropped un-freed, exactly as before: the
    // whole subtree resets with us and the respective owners rebuild their
    // state.
    for (Channel &c : this->channels)
    {
        for (std::deque<Txn> *queue : { &c.fwd_src, &c.fwd_dst })
        {
            for (Txn &t : *queue)
            {
                if (t.req->get_opcode() == vp::WRITE && t.req->allocator != nullptr)
                {
                    t.req->free();
                }
            }
        }
        c.fwd_src.clear();
        c.fwd_dst.clear();
        c.rev_src.clear();
        c.rev_dst.clear();
        c.retry_owed = false;
        c.fwd_out_denied = false;
        c.rev_in_denied = false;
    }
}


// ---- Helpers (parametric path) --------------------------------------------

// Channel a request or a response travels on: with axi_channels, the write
// beats and their acknowledgements on one, everything else on the other;
// without, everything on the read one.
int IoV2ClockBridge::channel_of(vp::IoReq *req) const
{
    return this->axi_channels && req->get_opcode() == vp::WRITE ? CH_WRITE : CH_READ;
}

// Tell if a retry on `channel` concerns the channel `ch`.
static inline bool covers(vp::IoRetryChannel channel, int ch)
{
    return channel == vp::IO_RETRY_ANY ||
        (channel == vp::IO_RETRY_READ) == (ch == IoV2ClockBridge::CH_READ);
}

static inline vp::IoRetryChannel retry_channel(int ch)
{
    return ch == IoV2ClockBridge::CH_READ ? vp::IO_RETRY_READ : vp::IO_RETRY_WRITE;
}


void IoV2ClockBridge::reschedule_stage(vp::ClockEvent &ev, std::deque<Txn> Channel::*queue,
                                        bool Channel::*denied, vp::ClockEngine *engine)
{
    // The stage event serves both channels: it wakes up for the earliest
    // head. A denied head waits for its retry, not for a cycle. The event is
    // never pushed back: the engine keeps the earliest cycle of an enqueued
    // event, and one waking up early finds nothing due and rearms. Cancelling
    // it here instead would starve it, as this is called at every activity
    // of the channels, possibly each cycle before the event runs.
    int64_t deadline = INT64_MAX;
    for (Channel &c : this->channels)
    {
        if ((c.*queue).empty() || (denied != nullptr && c.*denied)) continue;
        if ((c.*queue).front().deadline_cycle < deadline)
            deadline = (c.*queue).front().deadline_cycle;
    }
    if (deadline == INT64_MAX) return;
    // Called from either domain: bring the engine of the stage to the current
    // time before counting from its cycle.
    engine->sync();
    int64_t delta = deadline - engine->get_cycles();
    if (delta < 1) delta = 1;
    engine->enqueue(&ev, delta);
}


void IoV2ClockBridge::enqueue_in(std::deque<Txn> &queue, vp::IoReq *req,
                                  int64_t now_cycle, int min_spacing_cycles)
{
    int64_t deadline = now_cycle;
    if (!queue.empty())
    {
        int64_t prev = queue.back().deadline_cycle;
        if (deadline < prev + min_spacing_cycles)
            deadline = prev + min_spacing_cycles;
    }
    queue.push_back({req, deadline});
}


void IoV2ClockBridge::reschedule_fwd_src()
{
    this->reschedule_stage(*this->fwd_src_event, &Channel::fwd_src, nullptr, this->master_engine);
}

void IoV2ClockBridge::reschedule_fwd_dst()
{
    this->reschedule_stage(*this->fwd_dst_event, &Channel::fwd_dst, &Channel::fwd_out_denied,
        this->slave_engine);
}

void IoV2ClockBridge::reschedule_rev_src()
{
    this->reschedule_stage(*this->rev_src_event, &Channel::rev_src, nullptr, this->slave_engine);
}

void IoV2ClockBridge::reschedule_rev_dst()
{
    this->reschedule_stage(*this->rev_dst_event, &Channel::rev_dst, &Channel::rev_in_denied,
        this->master_engine);
}


int IoV2ClockBridge::occupancy(int ch) const
{
    // With axi_channels, the requests crossing: the responses have their own
    // FIFO, which does not hold the requests back (they come at the pace the
    // downstream sends them). Without, a single FIFO holding both.
    const Channel &c = this->channels[ch];
    int size = (int)(c.fwd_src.size() + c.fwd_dst.size());
    if (!this->axi_channels)
    {
        size += (int)(c.rev_src.size() + c.rev_dst.size());
    }
    return size;
}


void IoV2ClockBridge::maybe_retry()
{
    // Master-engine context only: in.retry() lands in the upstream master's
    // clock domain and must fire on one of its edges. Recompute occupancy on
    // every call — it is the live queue population, not a counter, so a
    // write beat that was granted downstream (and will never produce a
    // response) has already retired its slot by leaving fwd_dst.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        Channel &c = this->channels[ch];
        if (c.retry_owed && this->occupancy(ch) < this->depth)
        {
            c.retry_owed = false;
            this->in.retry(retry_channel(ch));
        }
    }
}


void IoV2ClockBridge::schedule_retry()
{
    // Occupancy freed on the slave-engine (forward) side: in.retry() must
    // not be called cross-domain, so cross back into the master domain
    // through a master-engine event — the same re-alignment the rev path
    // applies to responses. The handler re-checks the condition: occupancy
    // may have been refilled (e.g. by response beats) in between.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        if (this->channels[ch].retry_owed && this->occupancy(ch) < this->depth)
        {
            // ClockEngine::enqueue keeps the earliest pending cycle if the
            // event is already enqueued.
            this->master_engine->enqueue(this->retry_event, 1);
            return;
        }
    }
}


void IoV2ClockBridge::retry_event_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    self->maybe_retry();
}


// ---- v2 IO callbacks (branch on parametric) ------------------------------

vp::IoReqStatus IoV2ClockBridge::in_req_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    if (!self->parametric)
    {
        // Fast path: sync remote engine and forward inline. This is a pure
        // 1:1 relay, correct as-is under the per-burst write-ack contract:
        // the downstream's status (GRANTED / DONE / DENIED) is returned to
        // the upstream master unmodified, so ownership transfers exactly as
        // if the two were bound directly — a GRANTED write beat now belongs
        // to the downstream (the bridge never touched it), an inline DONE
        // leaves the beat with the master, and the burst ack comes back as
        // an ordinary response through out_resp_handler.
        self->slave_engine->sync();
        return self->out.req(req);
    }

    // Parametric path: depth gate + enqueue in fwd_src. The gate is the live
    // queue population (occupancy()), not a counter: a write beat granted
    // downstream retires its slot by leaving fwd_dst (it never comes back),
    // while round-tripped requests re-occupy a slot in the rev queues until
    // delivered upstream.
    // Reads and writes have their own queues and budget, as the AXI channels
    // of a CDC have their own FIFOs.
    int ch = self->channel_of(req);
    if (self->occupancy(ch) >= self->depth)
    {
        self->channels[ch].retry_owed = true;
        return vp::IO_REQ_DENIED;
    }

    int64_t now_master = self->master_engine->get_cycles();
    self->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Request in (req: %p, addr: 0x%lx, size: 0x%lx, write: %d)\n",
        req, req->get_addr(), req->get_size(), ch == CH_WRITE);
    self->enqueue_in(self->channels[ch].fwd_src, req, now_master + self->k_src_per_dir, 1);
    self->reschedule_fwd_src();
    return vp::IO_REQ_GRANTED;
}


vp::IoRespAck IoV2ClockBridge::out_resp_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    // Any response shape crosses here 1:1 — read beats, atomics round-trips,
    // and the downstream's per-burst write ack alike. The bridge is not the
    // initiator: it relays the object upstream unmodified and never frees it
    // (the initiator frees pool-backed acks / read beats after consuming
    // them).

    if (!self->parametric)
    {
        self->master_engine->sync();

        // The resp lands here on a slave (SoC) clock edge. Re-synchronize it
        // onto the master (cluster) clock by delivering on the master's next
        // edge: enqueue() on the idle master engine aligns the wake-up to the
        // next cluster edge after the current time, as a CDC synchronizer
        // would sample the response. Delivering inline instead would leak the
        // SoC edge timing into the cluster.
        self->resp_queue.push_back(req);
        if (!self->resp_held && !self->resp_event->is_enqueued())
            self->master_engine->enqueue(self->resp_event, 1);
        return vp::IO_RESP_ACCEPTED;
    }

    int64_t now_slave = self->slave_engine->get_cycles();
    self->trace.msg(vp::Trace::LEVEL_DEBUG, "Response in (req: %p, write: %d)\n",
        req, self->channel_of(req) == CH_WRITE);
    self->enqueue_in(self->channels[self->channel_of(req)].rev_src, req,
                     now_slave + self->k_src_per_dir, 1);
    self->reschedule_rev_src();

    return vp::IO_RESP_ACCEPTED;
}


void IoV2ClockBridge::out_retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    if (!self->parametric)
    {
        self->master_engine->sync();
        // Transparent bridge: forward the downstream channel upstream.
        self->in.retry(channel);
        return;
    }

    // The downstream can take the request it denied: send it again now, in
    // the retry call (a zero-buffer arbiter only keeps its accept window open
    // during it), then the ones which became due meanwhile.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        Channel &c = self->channels[ch];
        if (covers(channel, ch) && c.fwd_out_denied)
        {
            c.fwd_out_denied = false;
            self->fwd_dst_send(ch);
        }
    }
    self->reschedule_fwd_dst();
    self->reschedule_rev_src();
    self->schedule_retry();
}


// ---- sync_only response delivery -----------------------------------------

void IoV2ClockBridge::resp_event_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);

    // Deliver every response that has crossed the bridge so far, now aligned
    // on a master (cluster) clock edge.
    if (!self->resp_held)
    {
        self->deliver_resps();
    }
}


void IoV2ClockBridge::deliver_resps()
{
    while (!this->resp_queue.empty())
    {
        vp::IoReq *req = this->resp_queue.front();
        if (this->in.resp(req) == vp::IO_RESP_DENIED)
        {
            // The master keeps nothing of a denied response: it stays at the
            // head of the queue until the master calls resp_retry().
            this->resp_held = true;
            return;
        }
        this->resp_queue.pop_front();
    }
}


void IoV2ClockBridge::in_resp_retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    if (self->parametric)
    {
        // The upstream master can take the response it denied: send it again
        // now, then the ones which became due meanwhile.
        for (int ch = 0; ch < NB_CHANNELS; ch++)
        {
            Channel &c = self->channels[ch];
            if (covers(channel, ch) && c.rev_in_denied)
            {
                c.rev_in_denied = false;
                self->rev_dst_send(ch);
            }
        }
        self->maybe_retry();
        self->reschedule_rev_dst();
        return;
    }

    // The upstream master can take responses again. The held response has to
    // be sent again from here, in the same cycle.
    if (self->resp_held)
    {
        self->resp_held = false;
        self->deliver_resps();
        if (self->resp_held)
        {
            return;
        }
    }

    if (self->resp_retry_owed)
    {
        self->resp_retry_owed = false;
        self->slave_engine->sync();
        self->out.resp_retry(channel);
    }
}


// ---- Parametric stage handlers -------------------------------------------

void IoV2ClockBridge::fwd_src_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_master = self->master_engine->get_cycles();
    // The slave engine may be behind (not run yet in this cycle): bring it to
    // the current time, or the deadline would be computed on a stale cycle.
    self->slave_engine->sync();
    int64_t now_slave  = self->slave_engine->get_cycles();

    for (Channel &c : self->channels)
    {
        while (!c.fwd_src.empty() && c.fwd_src.front().deadline_cycle <= now_master)
        {
            Txn t = c.fwd_src.front();
            c.fwd_src.pop_front();
            self->enqueue_in(c.fwd_dst, t.req, now_slave + self->k_dst_per_dir, 1);
        }
    }

    self->reschedule_fwd_src();
    self->reschedule_fwd_dst();
}


void IoV2ClockBridge::fwd_dst_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);

    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        if (!self->channels[ch].fwd_out_denied)
        {
            self->fwd_dst_send(ch);
        }
    }

    self->reschedule_fwd_dst();
    self->reschedule_rev_src();

    // Forwarding retires occupancy: a granted write beat leaves the bridge
    // for good here (it never produces a response, so no rev-path drain will
    // ever run on its behalf), and an inline-INVALID non-last beat was freed
    // outright. The upstream accept window may thus re-open NOW — service an
    // owed retry. We are on the slave engine, so cross back into the master
    // domain through the dedicated retry event.
    self->schedule_retry();
}


void IoV2ClockBridge::fwd_dst_send(int ch)
{
    IoV2ClockBridge *self = this;
    Channel &c = this->channels[ch];
    int64_t now_slave = self->slave_engine->get_cycles();

    while (!c.fwd_dst.empty() && c.fwd_dst.front().deadline_cycle <= now_slave)
    {
        Txn t = c.fwd_dst.front();

        // Snapshot what the write-ack rules key on BEFORE req(): a GRANTED
        // write beat transfers ownership (buffer included) to the downstream
        // and must never be touched again.
        bool write_beat = t.req->get_opcode() == vp::WRITE;
        bool was_last   = t.req->is_last;

        t.req->prepare();
        self->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Request out (req: %p, addr: 0x%lx, size: 0x%lx, write: %d)\n",
            t.req, t.req->get_addr(), t.req->get_size(), ch == CH_WRITE);
        vp::IoReqStatus st = self->out.req(t.req);
        if (st == vp::IO_REQ_DENIED)
        {
            // The request stays at the head, and nothing behind it goes
            // before it, until the downstream calls retry().
            c.fwd_out_denied = true;
            return;
        }
        c.fwd_dst.pop_front();
        if (st == vp::IO_REQ_DONE)
        {
            if (write_beat && !was_last)
            {
                // A non-last write beat is never answered DONE on the beat
                // plane except as the inline-INVALID escape hatch (malformed
                // beat rejected by the target). The upstream master granted
                // the beat away to us, so the bridge owns it: return it to
                // its pool and drop it. The burst aborts through the
                // eventual ack's INVALID status — and if the master never
                // submits the last beat there is no ack at all; that is the
                // accepted escape-hatch semantics.
                self->traces.assert(
                    t.req->get_resp_status() == vp::IO_RESP_INVALID,
                    "downstream answered DONE(OK) on a non-last write beat "
                    "(req=%p, addr=0x%lx)", t.req, t.req->get_addr());
                t.req->free();
            }
            else if (write_beat)
            {
                // Inline DONE on the LAST write beat: the burst's inline
                // ack. The upstream master granted the beat to the bridge,
                // so the bridge owns a pool-backed beat — release it and
                // build the async data-less burst ack as a dedicated
                // request; a master-owned object (classic round-trip write)
                // travels back itself (io_v2_write_ack handles both). Keep
                // the burst's final status and the downstream timing
                // annotation, and push the ack into the rev path exactly
                // like a round-tripped response, so it keeps the CDC delay
                // shape.
                vp::IoRespStatus status = t.req->get_resp_status();
                int64_t latency  = t.req->get_latency();
                int64_t duration = t.req->get_duration();
                uint64_t addr = t.req->get_addr();
                uint64_t size = t.req->get_size();
                vp::IoReq *ack = vp::io_v2_write_ack(t.req);
                ack->set_addr(addr);
                ack->set_size(size);
                ack->set_resp_status(status);
                ack->set_latency(latency);
                ack->set_duration(duration);
                self->enqueue_in(c.rev_src, ack, now_slave + self->k_src_per_dir, 1);
            }
            else
            {
                // Reads and atomics keep the classic round-trip: the same
                // object travels back through the rev queues.
                self->enqueue_in(c.rev_src, t.req, now_slave + self->k_src_per_dir, 1);
            }
        }
        // GRANTED: ownership moved downstream — drop the pointer, never
        // touch the beat again. For a write beat the target consumes and
        // frees it (non-last beats produce no response at all; the burst's
        // single ack comes back through out_resp_handler like any other
        // response). For reads/atomics the response arrives later through
        // out_resp_handler.
    }
}


void IoV2ClockBridge::rev_src_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_slave  = self->slave_engine->get_cycles();
    // Same for the master engine, seen from the slave domain.
    self->master_engine->sync();
    int64_t now_master = self->master_engine->get_cycles();

    for (Channel &c : self->channels)
    {
        while (!c.rev_src.empty() && c.rev_src.front().deadline_cycle <= now_slave)
        {
            Txn t = c.rev_src.front();
            c.rev_src.pop_front();
            self->enqueue_in(c.rev_dst, t.req, now_master + self->k_dst_per_dir, 1);
        }
    }

    self->reschedule_rev_src();
    self->reschedule_rev_dst();
}


void IoV2ClockBridge::rev_dst_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);

    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        if (!self->channels[ch].rev_in_denied)
        {
            self->rev_dst_send(ch);
        }
    }

    // Delivering upstream freed occupancy; this handler runs on the master
    // engine, so an owed retry can be serviced synchronously.
    self->maybe_retry();

    self->reschedule_rev_dst();
}


void IoV2ClockBridge::rev_dst_send(int ch)
{
    Channel &c = this->channels[ch];
    int64_t now_master = this->master_engine->get_cycles();

    while (!c.rev_dst.empty() && c.rev_dst.front().deadline_cycle <= now_master)
    {
        Txn t = c.rev_dst.front();
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Response out (req: %p, write: %d)\n",
            t.req, ch == CH_WRITE);
        if (this->in.resp(t.req) == vp::IO_RESP_DENIED)
        {
            // The master keeps nothing of a denied response: it stays at the
            // head until the master calls resp_retry().
            c.rev_in_denied = true;
            return;
        }
        c.rev_dst.pop_front();
    }
}


// Backdoor target behind the output, or nullptr.
static vp::DebugMemIf *output_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int IoV2ClockBridge::debug_mem_access(uint64_t addr, uint8_t *data,
    uint64_t size, bool is_write)
{
    vp::DebugMemIf *child = output_debug_mem(this->out);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void IoV2ClockBridge::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *child = output_debug_mem(this->out);
    if (child != nullptr)
    {
        child->debug_mem_regions(regions, local_base, window_size, entry_base,
            depth + 1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2ClockBridge(config);
}
