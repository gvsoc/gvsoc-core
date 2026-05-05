// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/clocked_signal.hpp>
#include <vp/clock/clock_event.hpp>
#include <interco/limiter/limiter_config.hpp>

class Limiter : public vp::Component {
  public:
    Limiter(vp::ComponentConf &conf);

  private:
    void reset(bool active);
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void resp(vp::Block *__this, vp::IoReq *req);
    vp::IoReq *req_alloc();
    void req_free(vp::IoReq *);
    void handle_req();
    void handle_req_end(vp::IoReq *req, vp::IoReqStatus status);
    void send_resp();
    static void event_handler(vp::Block *_this, vp::ClockEvent *event);
    static void resp_event_handler(vp::Block *_this, vp::ClockEvent *event);

    LimiterConfig cfg;
    vp::Trace trace;

    vp::IoMaster output;
    vp::IoSlave input;

    vp::IoReq *req_queue = NULL;
    // Limiter handles one parent IoReq at a time end-to-end: pending_req
    // is set in req() and cleared in send_resp() once the LAST sub-request
    // has settled AND its computed completion cycle has been reached.
    // While set, any new incoming request is denied. That lets us keep
    // the per-parent split state in plain instance variables.
    vp::IoReq *pending_req;
    uint64_t pending_size;          // bytes left to dispatch to output
    uint64_t pending_addr;
    uint8_t *pending_data;
    int64_t pending_rem_size;       // bytes still owed by in-flight sub-reqs
    int64_t pending_max_completion; // max(now + sub_req->get_full_latency())
    bool input_denied;
    bool denied;
    vp::ClockEvent event;
    vp::ClockEvent resp_event;       // fires resp() at pending_max_completion
    vp::Signal<int64_t> next_req_timestamp;
};

Limiter::Limiter(vp::ComponentConf &config)
: vp::Component(config, this->cfg), event(this, &Limiter::event_handler),
resp_event(this, &Limiter::resp_event_handler),
next_req_timestamp(*this, "next_req_timestamp", 64, vp::SignalCommon::ResetKind::Value, 0) {
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->input.set_req_meth(&Limiter::req);
    this->new_slave_port("input", &this->input);

    this->output.set_grant_meth(&Limiter::grant);
    this->output.set_resp_meth(&Limiter::resp);
    this->new_master_port("output", &this->output);
}

void Limiter::reset(bool active) {
    if (active) {
        this->input_denied = false;
        this->denied = false;
        this->pending_req = NULL;
    }
}

vp::IoReq *Limiter::req_alloc()
{
    if (!this->req_queue)
    {
        this->req_queue = new vp::IoReq();
        this->req_queue->set_next(NULL);
    }

    vp::IoReq *req = this->req_queue;
    this->req_queue = req->get_next();
    return req;
}

void Limiter::req_free(vp::IoReq *req)
{
    req->set_next(this->req_queue);
    this->req_queue = req;
}

void Limiter::event_handler(vp::Block *__this, vp::ClockEvent *event) {
    Limiter *_this = (Limiter *)__this;
    _this->handle_req();
}

vp::IoReqStatus Limiter::req(vp::Block *__this, vp::IoReq *req) {
    Limiter *_this = (Limiter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        req, req->get_addr(), req->get_size(), req->get_is_write());

    if (_this->pending_req)
    {
        _this->input_denied = true;
        return vp::IO_REQ_DENIED;
    }

    req->status = vp::IO_REQ_OK;
    _this->pending_size           = req->get_size();
    _this->pending_addr           = req->get_addr();
    _this->pending_data           = req->get_data();
    _this->pending_rem_size       = (int64_t)req->get_size();
    _this->pending_max_completion = 0;
    _this->pending_req            = req;

    _this->event.enqueue();

    return vp::IO_REQ_PENDING;
}

void Limiter::handle_req()
{
    if (this->next_req_timestamp.get() <= this->clock.get_cycles()) {
        uint64_t size = std::min(this->pending_size, (uint64_t)this->cfg.bandwidth);

        vp::IoReq *req = this->req_alloc();
        req->prepare();

        req->parent_req = this->pending_req;
        req->addr = this->pending_addr;
        req->size = size;
        req->data = this->pending_data;
        req->is_write = this->pending_req->get_opcode();

        this->pending_addr += size;
        this->pending_data += size;
        this->pending_size -= size;
        this->next_req_timestamp.set(this->clock.get_cycles() + 1);

        vp::IoReqStatus status = this->output.req(req);
        if (status == vp::IO_REQ_OK || status == vp::IO_REQ_INVALID) {
            this->handle_req_end(req, status);
        } else if (status == vp::IO_REQ_DENIED) {

        } else {

        }
    }

    // Re-arm the dispatcher only while there's still data to split. The
    // parent's resp is handled by handle_req_end once the last sub-request
    // settles, so nothing else needs to be driven from here after pending_size
    // reaches 0.
    if (this->pending_size > 0 && !this->denied) {
        this->event.enqueue();
    }
}

void Limiter::handle_req_end(vp::IoReq *port_req, vp::IoReqStatus status) {
    vp::IoReq *req = this->pending_req;
    if (status == vp::IO_REQ_INVALID) req->status = vp::IO_REQ_INVALID;

    // Track the latest absolute cycle at which any sub-request finishes.
    // get_full_latency() = latency + duration, both relative to "now"
    // (whether resp arrived sync or async, the limiter's drain runs at the
    // current cycle), so completion = now + full_latency is the absolute
    // settle time for this sub-request.
    int64_t completion = (int64_t)this->clock.get_cycles() +
                         (int64_t)port_req->get_full_latency();
    if (completion > this->pending_max_completion)
        this->pending_max_completion = completion;

    this->pending_rem_size -= (int64_t)port_req->get_size();
    if (this->pending_rem_size == 0) {
        // All sub-requests settled. Defer the resp() so the upstream's
        // resp handler runs exactly at the latest sub-request's completion
        // cycle, instead of immediately with a latency hint set on the
        // parent IoReq. If the deadline has already passed (delay <= 0),
        // fire synchronously.
        int64_t delay = this->pending_max_completion -
                        (int64_t)this->clock.get_cycles();
        if (delay > 0) {
            this->resp_event.enqueue(delay);
        } else {
            this->send_resp();
        }
    }
}

void Limiter::send_resp() {
    // Clear the slot BEFORE calling resp(): the upstream's resp handler
    // may immediately re-issue a new request, and that request must see
    // the limiter as free.
    vp::IoReq *req = this->pending_req;
    this->pending_req = NULL;
    req->get_resp_port()->resp(req);
}

void Limiter::resp_event_handler(vp::Block *__this, vp::ClockEvent *event) {
    Limiter *_this = (Limiter *)__this;
    _this->send_resp();
}

void Limiter::grant(vp::Block *__this, vp::IoReq *req) {
    Limiter *_this = (Limiter *)__this;
}

void Limiter::resp(vp::Block *__this, vp::IoReq *port_req) {
    Limiter *_this = (Limiter *)__this;
    _this->handle_req_end(port_req, port_req->status);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new Limiter(config); }
