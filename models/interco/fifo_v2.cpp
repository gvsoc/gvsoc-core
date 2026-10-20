// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Generic io_v2 request/response FIFO buffer.
 *
 * Decouples an upstream master from the cycle-by-cycle back-pressure of a
 * downstream slave, modelling a small request FIFO such as the ``lint_FIFO``
 * placed in front of a TCDM logarithmic interconnect on silicon.
 *
 *   - Upstream slave side: an incoming request is GRANTED whenever fewer than
 *     ``depth`` requests hold a slot, DENIED (+ later retry()) when full. The
 *     master never sees the downstream deny. A request holds its slot from the
 *     moment it is accepted until its response has left upstream.
 *   - Downstream master side: buffered requests are driven out in order, one
 *     per cycle, ``latency`` cycles after they came in, with at most
 *     ``max_outstanding`` of them waiting for their downstream response. A
 *     downstream DENIED is parked and re-issued synchronously from retry(), so
 *     the FIFO works in front of a synchronous crossbar (log_ico_v2) that
 *     requires same-cycle re-issue.
 *   - Response: forwarded upstream via resp(), ``resp_latency`` cycles after
 *     the downstream gave it (the response registers of the buffer). With
 *     several requests outstanding the responses leave in the order the
 *     downstream gives them, which may not be the request order.
 *
 * The request object is forwarded unchanged (no split, no reorder), so read
 * data written by the downstream into the request buffer reaches the master.
 */

#include <algorithm>
#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>
#include <interco/fifo_v2/fifo_config.hpp>

class Fifo : public vp::Component, public vp::DebugMemIf
{
public:
    Fifo(vp::ComponentConf &conf);
    void reset(bool active) override;

    // Backdoor debug path: the FIFO is invisible, forward to the downstream.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

    FifoConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   output_resp(vp::Block *__this, vp::IoReq *req);
    static void            output_retry(vp::Block *__this, vp::IoRetryChannel channel);
    static void            pump_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            resp_handler(vp::Block *__this, vp::ClockEvent *event);

    // Send the head request downstream; classify the returned status.
    void send_head();
    // A request finished downstream: send its response upstream, now or after
    // the response latency.
    void req_done(vp::IoReq *req, int64_t latency);
    // Send a response upstream and free its slot.
    void reply(vp::IoReq *req);
    // Arm the pump if the head request can go downstream.
    void check_pump(int64_t cycles);
    // Number of slots in use.
    int64_t fill() {
        return (int64_t)(this->queue.size() + this->outstanding.size() + this->resp_queue.size());
    }
    int64_t max_outstanding() { return std::max((int64_t)1, (int64_t)this->cfg.max_outstanding); }

    vp::Trace trace;

    vp::IoSlave  input_itf{&Fifo::input_req};
    vp::IoMaster output_itf{&Fifo::output_retry, &Fifo::output_resp};

    // Drives one buffered request downstream per cycle.
    vp::ClockEvent pump_event;

    // Sends the delayed responses upstream.
    vp::ClockEvent resp_event;

    // Requests not yet accepted by the downstream, oldest first.
    std::deque<vp::IoReq *> queue;

    // Requests the downstream has granted and not answered yet.
    std::deque<vp::IoReq *> outstanding;

    // Responses the downstream has given and which are waiting for
    // cfg.resp_latency cycles before leaving upstream, oldest first.
    struct DelayedResp { vp::IoReq *req; int64_t cycle; };
    std::deque<DelayedResp> resp_queue;

    // The head request was DENIED and must be replayed from output_retry.
    // While set, the pump does not issue another request.
    bool downstream_stalled = false;
    // We denied an upstream request because the FIFO was full; owe a retry().
    bool input_needs_retry = false;
};


Fifo::Fifo(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      pump_event(this, &Fifo::pump_handler),
      resp_event(this, &Fifo::resp_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input",   &this->input_itf);
    this->new_master_port("output", &this->output_itf);
}

void Fifo::reset(bool active)
{
    if (active)
    {
        this->queue.clear();
        this->outstanding.clear();
        this->resp_queue.clear();
        this->downstream_stalled  = false;
        this->input_needs_retry   = false;
    }
}


vp::IoReqStatus Fifo::input_req(vp::Block *__this, vp::IoReq *req)
{
    Fifo *_this = (Fifo *)__this;

    // Full: back-pressure upstream. The master holds the request and resends
    // when we fire retry() (from reply, once a slot frees).
    if (_this->fill() >= _this->cfg.depth)
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Buffering req (offset: 0x%llx, size: 0x%llx, is_write: %d, fill: %d)\n",
        (unsigned long long)req->get_addr(), (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0, (int)_this->fill() + 1);

    req->set_resp_status(vp::IO_RESP_OK);
    _this->queue.push_back(req);

    // Kick the pump if it is idle. If the downstream side is busy, req_done
    // re-arms the pump for this one when it frees.
    _this->check_pump(_this->cfg.latency >= 1 ? _this->cfg.latency : 1);

    return vp::IO_REQ_GRANTED;
}


void Fifo::check_pump(int64_t cycles)
{
    if (!this->queue.empty() && !this->downstream_stalled &&
        (int64_t)this->outstanding.size() < this->max_outstanding() &&
        !this->pump_event.is_enqueued())
    {
        this->pump_event.enqueue(cycles);
    }
}


void Fifo::send_head()
{
    vp::IoReq *req = this->queue.front();

    vp::IoReqStatus st = this->output_itf.req(req);
    if (st == vp::IO_REQ_DENIED)
    {
        // Parked: the downstream will call retry() (synchronously, for a sync
        // crossbar) and we replay the same request from output_retry.
        this->downstream_stalled = true;
        return;
    }

    this->queue.pop_front();

    if (st == vp::IO_REQ_DONE)
    {
        int64_t latency = this->cfg.resp_latency + this->cfg.done_latency;
        if (this->cfg.done_annotated_latency)
        {
            latency += req->get_latency();
        }
        this->req_done(req, latency);
    }
    else // IO_REQ_GRANTED: an async slave will drive output_resp later.
    {
        this->outstanding.push_back(req);
        // The next request can follow on the next cycle.
        this->check_pump(1);
    }
}


void Fifo::pump_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Fifo *_this = (Fifo *)__this;

    if (_this->downstream_stalled || _this->queue.empty() ||
        (int64_t)_this->outstanding.size() >= _this->max_outstanding())
    {
        return;
    }
    _this->send_head();
}


void Fifo::req_done(vp::IoReq *req, int64_t latency)
{
    if (latency <= 0)
    {
        this->reply(req);
    }
    else
    {
        // The response goes through the response registers: it keeps its slot
        // until it leaves upstream. Responses leave in the order they are due.
        DelayedResp resp = {req, this->clock.get_cycles() + latency};
        auto it = this->resp_queue.begin();
        while (it != this->resp_queue.end() && it->cycle <= resp.cycle) ++it;
        this->resp_queue.insert(it, resp);
        this->resp_event.enqueue(this->resp_queue.front().cycle - this->clock.get_cycles());
    }

    // Keep draining if more requests are buffered.
    this->check_pump(1);
}


void Fifo::reply(vp::IoReq *req)
{
    // Reply upstream for this request.
    this->input_itf.resp(req);

    // A slot just freed: let a previously back-pressured master back in.
    if (this->input_needs_retry && this->fill() < this->cfg.depth)
    {
        this->input_needs_retry = false;
        this->input_itf.retry();
    }
}


void Fifo::resp_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Fifo *_this = (Fifo *)__this;
    int64_t now = _this->clock.get_cycles();

    while (!_this->resp_queue.empty() && _this->resp_queue.front().cycle <= now)
    {
        vp::IoReq *req = _this->resp_queue.front().req;
        _this->resp_queue.pop_front();
        _this->reply(req);
    }

    if (!_this->resp_queue.empty())
    {
        _this->resp_event.enqueue(_this->resp_queue.front().cycle - now);
    }
}


vp::IoRespAck Fifo::output_resp(vp::Block *__this, vp::IoReq *req)
{
    Fifo *_this = (Fifo *)__this;
    // Async downstream completion of one of the outstanding requests.
    auto it = std::find(_this->outstanding.begin(), _this->outstanding.end(), req);
    if (it != _this->outstanding.end())
    {
        _this->outstanding.erase(it);
    }
    _this->req_done(req, _this->cfg.resp_latency);
    return vp::IO_RESP_ACCEPTED;
}


void Fifo::output_retry(vp::Block *__this, vp::IoRetryChannel /*channel*/)
{
    Fifo *_this = (Fifo *)__this;
    if (!_this->downstream_stalled)
    {
        return;
    }
    // Replay the parked head request. For a synchronous crossbar this returns
    // DONE inline; req_done then re-arms the pump for the next request.
    _this->downstream_stalled = false;
    _this->send_head();
}


// ---- backdoor debug: transparent pass-through to the downstream ------------

static vp::DebugMemIf *downstream_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int Fifo::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size, bool is_write)
{
    vp::DebugMemIf *child = downstream_debug_mem(this->output_itf);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void Fifo::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *child = downstream_debug_mem(this->output_itf);
    if (child != nullptr)
    {
        child->debug_mem_regions(regions, local_base, window_size, entry_base, depth + 1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Fifo(config);
}
