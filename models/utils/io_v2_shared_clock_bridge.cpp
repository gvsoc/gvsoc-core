// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_shared_clock_bridge.hpp"


IoV2SharedClockBridge::IoV2SharedClockBridge(vp::ComponentConf &config)
    : vp::Component(config),
      req_event(this, &IoV2SharedClockBridge::req_event_handler),
      resp_event(this, &IoV2SharedClockBridge::resp_event_handler),
      req_retry_event(this, &IoV2SharedClockBridge::req_retry_event_handler),
      resp_retry_event(this, &IoV2SharedClockBridge::resp_retry_event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}


void IoV2SharedClockBridge::start()
{
    // The remote PORT owners, not the remote contexts: with a muxed peer port
    // the remote context is the dispatch stub, not the component.
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
    this->init_lists();
    this->trace.msg(vp::Trace::LEVEL_INFO, "bridge mode=shared_clock\n");
}


void IoV2SharedClockBridge::init_lists()
{
    // Requests go from the input (master) domain to the output (slave) one,
    // responses the other way.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        this->req_list[ch] = List();
        this->req_list[ch].engine = this->slave_engine;
        this->req_list[ch].event = &this->req_event;
        this->req_list[ch].src_engine = this->master_engine;
        this->req_list[ch].retry_event = &this->req_retry_event;

        this->resp_list[ch] = List();
        this->resp_list[ch].engine = this->master_engine;
        this->resp_list[ch].event = &this->resp_event;
        this->resp_list[ch].src_engine = this->slave_engine;
        this->resp_list[ch].retry_event = &this->resp_retry_event;
    }
}


void IoV2SharedClockBridge::reset(bool active)
{
    if (!active)
    {
        return;
    }
    // The events are enqueued on the engines of their domains, not on the
    // bridge clock.
    this->slave_engine->cancel(&this->req_event);
    this->master_engine->cancel(&this->resp_event);
    this->master_engine->cancel(&this->req_retry_event);
    this->slave_engine->cancel(&this->resp_retry_event);
    // A write beat still in the request list was granted to the bridge and
    // nobody else will free it. A write without allocator is a master-owned
    // object (round-trip write), not ours to free. The rest is dropped: the
    // owners reset with us.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        for (vp::IoReq *req = this->req_list[ch].head; req != nullptr; )
        {
            vp::IoReq *next = req->get_next();
            if (req->get_opcode() == vp::WRITE && req->allocator != nullptr)
            {
                req->free();
            }
            req = next;
        }
    }
    this->init_lists();
}


// Channel a request or a response travels on: the write beats and their
// acknowledgements on one, everything else on the other.
static inline int channel_of(vp::IoReq *req)
{
    return req->get_opcode() == vp::WRITE ? IoV2SharedClockBridge::CH_WRITE
                                          : IoV2SharedClockBridge::CH_READ;
}

// Tell if a retry on `channel` concerns the list of channel `ch`.
static inline bool covers(vp::IoRetryChannel channel, int ch)
{
    return channel == vp::IO_RETRY_ANY ||
        (channel == vp::IO_RETRY_READ) == (ch == IoV2SharedClockBridge::CH_READ);
}


bool IoV2SharedClockBridge::can_take(List &list)
{
    // Called from the domain feeding the list: bring the engine of the list up
    // to date to know its cycle.
    list.engine->sync();

    if (list.head == nullptr)
    {
        return true;
    }
    // The slot frees in this cycle if the one in it is sent in this cycle:
    // alone, it has waited its cycle and was not denied.
    return list.head == list.tail && !list.held &&
        list.head_cycle <= list.engine->get_cycles();
}


void IoV2SharedClockBridge::push(List &list, vp::IoReq *req)
{
    list.engine->sync();

    req->set_next(nullptr);
    if (list.head == nullptr)
    {
        list.head = req;
        list.head_cycle = list.engine->get_cycles() + 1;
    }
    else
    {
        list.tail->set_next(req);
    }
    list.tail = req;

    list.engine->enqueue(list.event, 1);
}


void IoV2SharedClockBridge::pop(List &list, vp::IoReq *next)
{
    list.head = next;
    if (list.head == nullptr)
    {
        list.tail = nullptr;
    }
    // One per cycle: the next head, taken at the latest in this cycle, goes on
    // the next one.
    list.head_cycle = list.engine->get_cycles() + 1;

    // The slot is free again: the side we denied can send again from the next
    // cycle, told by an event of its own domain.
    if (list.retry_owed)
    {
        list.src_engine->enqueue(list.retry_event, 1);
    }
}


void IoV2SharedClockBridge::check(List &list)
{
    if (list.head != nullptr && !list.held)
    {
        list.engine->enqueue(list.event, 1);
    }
}


// ---- Request path ------------------------------------------------------------

vp::IoReqStatus IoV2SharedClockBridge::in_req_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);

    List &list = self->req_list[channel_of(req)];

    if (!self->can_take(list))
    {
        self->trace.msg(vp::Trace::LEVEL_TRACE, "Denied request (req: %p)\n", req);
        list.retry_owed = true;
        return vp::IO_REQ_DENIED;
    }

    self->trace.msg(vp::Trace::LEVEL_TRACE, "Buffered request (req: %p, addr: 0x%lx, size: 0x%lx, "
        "is_write: %d)\n", req, req->get_addr(), req->get_size(), req->get_is_write());

    self->push(list, req);
    return vp::IO_REQ_GRANTED;
}


void IoV2SharedClockBridge::req_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        self->req_send(ch);
        self->check(self->req_list[ch]);
    }
}


void IoV2SharedClockBridge::req_send(int ch)
{
    List &list = this->req_list[ch];
    if (list.head == nullptr || list.held || list.head_cycle > list.engine->get_cycles())
    {
        return;
    }

    vp::IoReq *req = list.head;
    // Read before sending: once taken, the request may be freed and its next
    // reused by its pool.
    vp::IoReq *next = req->get_next();

    // What the write acknowledgement rules key on, snapshotted before req(): a
    // granted write beat belongs to the output side and is not ours anymore.
    bool is_write = req->get_opcode() == vp::WRITE;
    bool is_last = req->is_last;

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Forwarding request (req: %p)\n", req);

    vp::IoReqStatus status = this->out.req(req);
    if (status == vp::IO_REQ_DENIED)
    {
        // Stays at the head, keeping the slot, until the output side asks for
        // a retry.
        list.held = true;
        return;
    }

    this->pop(list, next);

    if (status == vp::IO_REQ_DONE)
    {
        // Answered inline: the response crosses back through the response
        // list, as any other response. It cannot be refused anymore, so it is
        // queued even if the response slot is taken.
        if (is_write && !is_last)
        {
            // Only the inline-INVALID escape hatch answers DONE on a non-last
            // write beat. The beat was granted to the bridge: free it. The
            // burst aborts through the status of its ack.
            this->traces.assert(req->get_resp_status() == vp::IO_RESP_INVALID,
                "output answered DONE(OK) on a non-last write beat (req=%p, addr=0x%lx)",
                req, req->get_addr());
            req->free();
        }
        else if (is_write)
        {
            // Inline burst ack on the last write beat: the upstream master
            // granted the beat to the bridge and waits for a separate ack.
            vp::IoRespStatus resp_status = req->get_resp_status();
            int64_t latency  = req->get_latency();
            int64_t duration = req->get_duration();
            uint64_t addr = req->get_addr();
            uint64_t size = req->get_size();
            vp::IoReq *ack = vp::io_v2_write_ack(req);
            ack->set_addr(addr);
            ack->set_size(size);
            ack->set_resp_status(resp_status);
            ack->set_latency(latency);
            ack->set_duration(duration);
            this->push(this->resp_list[CH_WRITE], ack);
        }
        else
        {
            this->push(this->resp_list[CH_READ], req);
        }
    }
    // GRANTED: the output side owns it now. A read or an atomic comes back
    // through out_resp_handler, a write beat is consumed there and its burst
    // ack comes back the same way.
}


void IoV2SharedClockBridge::out_retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    // The held requests must be sent again from here, in the same cycle.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        if (covers(channel, ch) && self->req_list[ch].held)
        {
            self->req_list[ch].held = false;
            self->req_send(ch);
            self->check(self->req_list[ch]);
        }
    }
}


void IoV2SharedClockBridge::req_retry_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    // In the input domain, from the cycle after the slot freed. If it got
    // taken again meanwhile (an inline response for the response list), the
    // retry waits for the next free slot.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        List &list = self->req_list[ch];
        if (list.retry_owed && self->can_take(list))
        {
            list.retry_owed = false;
            self->in.retry(ch == CH_READ ? vp::IO_RETRY_READ : vp::IO_RETRY_WRITE);
        }
    }
}


// ---- Response path -----------------------------------------------------------

vp::IoRespAck IoV2SharedClockBridge::out_resp_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);

    List &list = self->resp_list[channel_of(req)];

    if (!self->can_take(list))
    {
        self->trace.msg(vp::Trace::LEVEL_TRACE, "Denied response (req: %p)\n", req);
        list.retry_owed = true;
        return vp::IO_RESP_DENIED;
    }

    self->trace.msg(vp::Trace::LEVEL_TRACE, "Buffered response (req: %p)\n", req);

    self->push(list, req);
    return vp::IO_RESP_ACCEPTED;
}


void IoV2SharedClockBridge::resp_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        self->resp_send(ch);
        self->check(self->resp_list[ch]);
    }
}


void IoV2SharedClockBridge::resp_send(int ch)
{
    List &list = this->resp_list[ch];
    if (list.head == nullptr || list.held || list.head_cycle > list.engine->get_cycles())
    {
        return;
    }

    vp::IoReq *req = list.head;
    // Read before sending: once taken, the response may be freed and its next
    // reused by its pool.
    vp::IoReq *next = req->get_next();

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Forwarding response (req: %p)\n", req);

    if (this->in.resp(req) == vp::IO_RESP_DENIED)
    {
        // Stays at the head, keeping the slot, until the input side asks for
        // a retry.
        list.held = true;
        return;
    }

    this->pop(list, next);
}


void IoV2SharedClockBridge::in_resp_retry_handler(vp::Block *__this, vp::IoRetryChannel channel)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    // The held responses must be sent again from here, in the same cycle.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        if (covers(channel, ch) && self->resp_list[ch].held)
        {
            self->resp_list[ch].held = false;
            self->resp_send(ch);
            self->check(self->resp_list[ch]);
        }
    }
}


void IoV2SharedClockBridge::resp_retry_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IoV2SharedClockBridge *self = static_cast<IoV2SharedClockBridge *>(__this);
    // In the output domain, from the cycle after the slot freed.
    for (int ch = 0; ch < NB_CHANNELS; ch++)
    {
        List &list = self->resp_list[ch];
        if (list.retry_owed && self->can_take(list))
        {
            list.retry_owed = false;
            if (self->out.is_resp_retry_bound())
            {
                self->out.resp_retry(ch == CH_READ ? vp::IO_RETRY_READ : vp::IO_RETRY_WRITE);
            }
        }
    }
}


// ---- Backdoor debug path (DebugMemIf): pass-through into the output --------

static vp::DebugMemIf *output_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int IoV2SharedClockBridge::debug_mem_access(uint64_t addr, uint8_t *data,
    uint64_t size, bool is_write)
{
    vp::DebugMemIf *child = output_debug_mem(this->out);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void IoV2SharedClockBridge::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
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
    return new IoV2SharedClockBridge(config);
}
