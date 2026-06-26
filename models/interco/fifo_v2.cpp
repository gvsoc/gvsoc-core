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
 *     ``depth`` requests are buffered, DENIED (+ later retry()) when full. The
 *     master never sees the downstream deny.
 *   - Downstream master side: buffered requests are driven out in order. A
 *     downstream DENIED is parked and re-issued synchronously from retry(), so
 *     the FIFO works in front of a synchronous crossbar (log_ico_v2) that
 *     requires same-cycle re-issue. On completion the response is forwarded
 *     upstream via resp().
 *
 * The request object is forwarded unchanged (no split, no reorder), so read
 * data written by the downstream into the request buffer reaches the master.
 */

#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <interco/fifo_v2/fifo_config.hpp>

class Fifo : public vp::Component
{
public:
    Fifo(vp::ComponentConf &conf);
    void reset(bool active) override;

    FifoConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   output_resp(vp::Block *__this, vp::IoReq *req);
    static void            output_retry(vp::Block *__this);
    static void            pump_handler(vp::Block *__this, vp::ClockEvent *event);

    // Send the head request downstream; classify the returned status.
    void send_head();
    // The head request finished downstream: reply upstream, drop it, and keep
    // the pipe moving.
    void head_done();

    vp::Trace trace;

    vp::IoSlave  input_itf{&Fifo::input_req};
    vp::IoMaster output_itf{&Fifo::output_retry, &Fifo::output_resp};

    // Drives one buffered request downstream per cycle.
    vp::ClockEvent pump_event;

    // Buffered requests, oldest first. Size capped at cfg.depth.
    std::deque<vp::IoReq *> queue;

    // A head request has been issued downstream and is still outstanding
    // (either awaiting an async resp(), or parked on a DENIED waiting for
    // retry()). While set, the pump does not issue another request.
    bool downstream_busy = false;
    // The outstanding head was DENIED and must be replayed from output_retry.
    bool downstream_stalled = false;
    // We denied an upstream request because the FIFO was full; owe a retry().
    bool input_needs_retry = false;
};


Fifo::Fifo(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      pump_event(this, &Fifo::pump_handler)
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
        this->downstream_busy     = false;
        this->downstream_stalled  = false;
        this->input_needs_retry   = false;
    }
}


vp::IoReqStatus Fifo::input_req(vp::Block *__this, vp::IoReq *req)
{
    Fifo *_this = (Fifo *)__this;

    // Full: back-pressure upstream. The master holds the request and resends
    // when we fire retry() (from head_done, once a slot frees).
    if ((int64_t)_this->queue.size() >= _this->cfg.depth)
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Buffering req (offset: 0x%llx, size: 0x%llx, is_write: %d, fill: %d)\n",
        (unsigned long long)req->get_addr(), (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0, (int)_this->queue.size() + 1);

    req->set_resp_status(vp::IO_RESP_OK);
    _this->queue.push_back(req);

    // Kick the pump if it is idle. If a request is already in flight
    // downstream, head_done re-arms the pump for this one when the pipe frees.
    if (!_this->downstream_busy && !_this->pump_event.is_enqueued())
    {
        _this->pump_event.enqueue(_this->cfg.latency >= 1 ? _this->cfg.latency : 1);
    }

    return vp::IO_REQ_GRANTED;
}


void Fifo::send_head()
{
    vp::IoReq *req = this->queue.front();

    vp::IoReqStatus st = this->output_itf.req(req);
    if (st == vp::IO_REQ_DONE)
    {
        this->head_done();
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Parked: the downstream will call retry() (synchronously, for a sync
        // crossbar) and we replay the same request from output_retry.
        this->downstream_busy    = true;
        this->downstream_stalled = true;
    }
    else // IO_REQ_GRANTED: an async slave will drive output_resp later.
    {
        this->downstream_busy = true;
    }
}


void Fifo::pump_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Fifo *_this = (Fifo *)__this;

    if (_this->downstream_busy || _this->queue.empty())
    {
        return;
    }
    _this->send_head();
}


void Fifo::head_done()
{
    vp::IoReq *req = this->queue.front();
    this->queue.pop_front();
    this->downstream_busy    = false;
    this->downstream_stalled = false;

    // Reply upstream for this request.
    this->input_itf.resp(req);

    // A slot just freed: let a previously back-pressured master back in.
    if (this->input_needs_retry && (int64_t)this->queue.size() < this->cfg.depth)
    {
        this->input_needs_retry = false;
        this->input_itf.retry();
    }

    // Keep draining if more requests are buffered.
    if (!this->queue.empty() && !this->pump_event.is_enqueued())
    {
        this->pump_event.enqueue(1);
    }
}


vp::IoRespAck Fifo::output_resp(vp::Block *__this, vp::IoReq * /*req*/)
{
    Fifo *_this = (Fifo *)__this;
    // Async downstream completion of the outstanding head.
    _this->head_done();
    return vp::IO_RESP_ACCEPTED;
}


void Fifo::output_retry(vp::Block *__this)
{
    Fifo *_this = (Fifo *)__this;
    if (!_this->downstream_stalled)
    {
        return;
    }
    // Replay the parked head request. For a synchronous crossbar this returns
    // DONE inline; head_done then re-arms the pump for the next request.
    _this->downstream_stalled = false;
    _this->send_head();
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Fifo(config);
}
