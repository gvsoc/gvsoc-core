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
        "bridge mode=%s k_src=%d k_dst=%d depth=%d\n",
        this->parametric ? "parametric" : "sync_only",
        this->k_src_per_dir, this->k_dst_per_dir, this->depth);
}


void IoV2ClockBridge::reset(bool active)
{
    if (!active) return;

    if (!this->parametric)
    {
        if (this->resp_event->is_enqueued())
            this->master_engine->cancel(this->resp_event);
        this->resp_queue.clear();
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
    this->fwd_src_queue.clear();
    this->fwd_dst_queue.clear();
    this->rev_src_queue.clear();
    this->rev_dst_queue.clear();
    this->retry_owed = false;
}


// ---- Helpers (parametric path) --------------------------------------------

void IoV2ClockBridge::reschedule_event(vp::ClockEvent &ev,
                                        const std::deque<Txn> &queue,
                                        vp::ClockEngine *engine)
{
    if (ev.is_enqueued()) engine->cancel(&ev);
    if (queue.empty()) return;
    int64_t now = engine->get_cycles();
    int64_t delta = queue.front().deadline_cycle - now;
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


// ---- v2 IO callbacks (branch on parametric) ------------------------------

vp::IoReqStatus IoV2ClockBridge::in_req_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    if (!self->parametric)
    {
        // Fast path: sync remote engine and forward inline.
        self->slave_engine->sync();
        return self->out.req(req);
    }

    // Parametric path: depth gate + enqueue in fwd_src.
    int in_flight = (int)(self->fwd_src_queue.size()
                          + self->fwd_dst_queue.size()
                          + self->rev_src_queue.size()
                          + self->rev_dst_queue.size());
    if (in_flight >= self->depth)
    {
        self->retry_owed = true;
        return vp::IO_REQ_DENIED;
    }

    int64_t now_master = self->master_engine->get_cycles();
    int64_t deadline = now_master + self->k_src_per_dir;
    if (!self->fwd_src_queue.empty())
    {
        int64_t prev = self->fwd_src_queue.back().deadline_cycle;
        if (deadline < prev + 1) deadline = prev + 1;
    }
    self->fwd_src_queue.push_back({req, deadline});
    self->reschedule_event(*self->fwd_src_event, self->fwd_src_queue,
                           self->master_engine);
    return vp::IO_REQ_GRANTED;
}


void IoV2ClockBridge::out_resp_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(__this);

    if (!self->parametric)
    {
        // The resp lands here on a slave (SoC) clock edge. Re-synchronize it
        // onto the master (cluster) clock by delivering on the master's next
        // edge: enqueue() on the idle master engine aligns the wake-up to the
        // next cluster edge at/after the current time, as a CDC synchronizer
        // would sample the response. Delivering inline instead would leak the
        // SoC edge timing into the cluster.
        self->resp_queue.push_back(req);
        if (!self->resp_event->is_enqueued())
            self->master_engine->enqueue(self->resp_event, 1);
        return;
    }

    int64_t now_slave = self->slave_engine->get_cycles();
    self->enqueue_in(self->rev_src_queue, req,
                     now_slave + self->k_src_per_dir, 1);
    self->reschedule_event(*self->rev_src_event, self->rev_src_queue,
                           self->slave_engine);
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
    // Downstream became ready after DENIED β€” unused for sync-DONE slaves.
}


// ---- sync_only response delivery -----------------------------------------

void IoV2ClockBridge::resp_event_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);

    // Deliver every response that has crossed the bridge so far, now aligned
    // on a master (cluster) clock edge.
    while (!self->resp_queue.empty())
    {
        vp::IoReq *req = self->resp_queue.front();
        self->resp_queue.pop_front();
        self->in.resp(req);
    }
}


// ---- Parametric stage handlers -------------------------------------------

void IoV2ClockBridge::fwd_src_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_master = self->master_engine->get_cycles();
    int64_t now_slave  = self->slave_engine->get_cycles();

    while (!self->fwd_src_queue.empty()
           && self->fwd_src_queue.front().deadline_cycle <= now_master)
    {
        Txn t = self->fwd_src_queue.front();
        self->fwd_src_queue.pop_front();
        self->enqueue_in(self->fwd_dst_queue, t.req,
                         now_slave + self->k_dst_per_dir, 1);
    }

    self->reschedule_event(*self->fwd_src_event, self->fwd_src_queue,
                           self->master_engine);
    self->reschedule_event(*self->fwd_dst_event, self->fwd_dst_queue,
                           self->slave_engine);
}


void IoV2ClockBridge::fwd_dst_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_slave = self->slave_engine->get_cycles();

    while (!self->fwd_dst_queue.empty()
           && self->fwd_dst_queue.front().deadline_cycle <= now_slave)
    {
        Txn t = self->fwd_dst_queue.front();
        self->fwd_dst_queue.pop_front();

        t.req->prepare();
        vp::IoReqStatus st = self->out.req(t.req);
        if (st == vp::IO_REQ_DONE)
        {
            self->enqueue_in(self->rev_src_queue, t.req,
                             now_slave + self->k_src_per_dir, 1);
        }
        // GRANTED: out_resp_handler will handle. DENIED: not modeled.
    }

    self->reschedule_event(*self->fwd_dst_event, self->fwd_dst_queue,
                           self->slave_engine);
    self->reschedule_event(*self->rev_src_event, self->rev_src_queue,
                           self->slave_engine);
}


void IoV2ClockBridge::rev_src_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_slave  = self->slave_engine->get_cycles();
    int64_t now_master = self->master_engine->get_cycles();

    while (!self->rev_src_queue.empty()
           && self->rev_src_queue.front().deadline_cycle <= now_slave)
    {
        Txn t = self->rev_src_queue.front();
        self->rev_src_queue.pop_front();
        self->enqueue_in(self->rev_dst_queue, t.req,
                         now_master + self->k_dst_per_dir, 1);
    }

    self->reschedule_event(*self->rev_src_event, self->rev_src_queue,
                           self->slave_engine);
    self->reschedule_event(*self->rev_dst_event, self->rev_dst_queue,
                           self->master_engine);
}


void IoV2ClockBridge::rev_dst_done_handler(vp::Block *_this, vp::ClockEvent *)
{
    IoV2ClockBridge *self = static_cast<IoV2ClockBridge *>(_this);
    int64_t now_master = self->master_engine->get_cycles();

    while (!self->rev_dst_queue.empty()
           && self->rev_dst_queue.front().deadline_cycle <= now_master)
    {
        Txn t = self->rev_dst_queue.front();
        self->rev_dst_queue.pop_front();
        self->in.resp(t.req);
    }

    if (self->retry_owed)
    {
        int in_flight = (int)(self->fwd_src_queue.size()
                              + self->fwd_dst_queue.size()
                              + self->rev_src_queue.size()
                              + self->rev_dst_queue.size());
        if (in_flight < self->depth)
        {
            self->retry_owed = false;
            self->in.retry();
        }
    }

    self->reschedule_event(*self->rev_dst_event, self->rev_dst_queue,
                           self->master_engine);
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
