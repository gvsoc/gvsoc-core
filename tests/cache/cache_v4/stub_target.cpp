// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Testbench target for router_v2 (io_v2 protocol).
 *
 * Reads a list of rules from get_js_config()/rules, each entry:
 *   { addr_min, addr_max, behavior, resp_delay, retry_delay }
 * behavior in {"done", "done_invalid", "granted", "denied"}.
 *
 * On req(), the target picks the first matching rule and:
 *   done         -> IO_REQ_DONE + IO_RESP_OK
 *   done_invalid -> IO_REQ_DONE + IO_RESP_INVALID
 *   granted      -> IO_REQ_GRANTED, schedule resp() after resp_delay cycles
 *   denied       -> IO_REQ_DENIED, schedule retry() after retry_delay cycles
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

class StubTarget : public vp::Component
{
public:
    StubTarget(vp::ComponentConf &conf);

private:
    enum class Behavior { DONE, DONE_INVALID, GRANTED, DENIED };

    struct Rule {
        uint64_t addr_min;
        uint64_t addr_max;
        Behavior behavior;
        int64_t resp_delay;
        int64_t retry_delay;
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void deferred_resp_handler(vp::Block *__this, vp::ClockEvent *event);
    static void deferred_retry_handler(vp::Block *__this, vp::ClockEvent *event);

    Rule *rule_for(uint64_t addr);

    vp::IoSlave in;
    vp::ClockEvent resp_event;
    vp::ClockEvent retry_event;
    vp::Trace trace;
    std::vector<Rule> rules;
    std::string logname;

    // Pending resp queue, ordered by due_cycle. Head drives resp_event.
    struct Pending {
        vp::IoReq *req;
        int64_t due_cycle;
    };
    std::deque<Pending> pending_resps;

    // Pending retry events (due cycles). Head drives retry_event.
    std::deque<int64_t> pending_retries;
};

StubTarget::StubTarget(vp::ComponentConf &config)
    : vp::Component(config),
      in(&StubTarget::req_handler),
      resp_event(this, &StubTarget::deferred_resp_handler),
      retry_event(this, &StubTarget::deferred_retry_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in, this);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();

    js::Config *rules_cfg = this->get_js_config()->get("rules");
    if (rules_cfg != NULL)
    {
        for (auto &item : rules_cfg->get_elems())
        {
            Rule r;
            r.addr_min = (uint64_t)item->get_int("addr_min");
            r.addr_max = (uint64_t)item->get_int("addr_max");
            std::string b = item->get_child_str("behavior");
            if (b == "done_invalid")      r.behavior = Behavior::DONE_INVALID;
            else if (b == "granted")      r.behavior = Behavior::GRANTED;
            else if (b == "denied")       r.behavior = Behavior::DENIED;
            else                           r.behavior = Behavior::DONE;
            r.resp_delay = item->get_child_int("resp_delay");
            r.retry_delay = item->get_child_int("retry_delay");
            this->rules.push_back(r);
        }
    }
}

StubTarget::Rule *StubTarget::rule_for(uint64_t addr)
{
    for (Rule &r : this->rules)
    {
        if (addr >= r.addr_min && addr <= r.addr_max)
        {
            return &r;
        }
    }
    return nullptr;
}

vp::IoReqStatus StubTarget::req_handler(vp::Block *__this, vp::IoReq *req)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();

    printf("[%ld] %s REQ addr=0x%lx size=%lu write=%d\n",
        now, _this->logname.c_str(), req->get_addr(), req->get_size(),
        req->get_is_write() ? 1 : 0);

    Rule *r = _this->rule_for(req->get_addr());
    Behavior b = r ? r->behavior : Behavior::DONE_INVALID;

    switch (b)
    {
        case Behavior::DONE:
            if (!req->get_is_write() && req->get_data())
            {
                std::memset(req->get_data(), 0xAA, req->get_size());
            }
            req->set_resp_status(vp::IO_RESP_OK);
            return vp::IO_REQ_DONE;

        case Behavior::DONE_INVALID:
            req->set_resp_status(vp::IO_RESP_INVALID);
            return vp::IO_REQ_DONE;

        case Behavior::GRANTED:
        {
            if (!req->get_is_write() && req->get_data())
            {
                std::memset(req->get_data(), 0xAA, req->get_size());
            }
            req->set_resp_status(vp::IO_RESP_OK);
            int64_t due = now + r->resp_delay;
            Pending p{req, due};
            // Insert sorted by due.
            auto it = _this->pending_resps.begin();
            while (it != _this->pending_resps.end() && it->due_cycle <= due) ++it;
            _this->pending_resps.insert(it, p);
            // Arm or re-arm resp_event to the head.
            int64_t head_due = _this->pending_resps.front().due_cycle;
            int64_t delta = head_due - now;
            if (delta <= 0) delta = 1;
            if (_this->resp_event.is_enqueued()) _this->resp_event.cancel();
            _this->resp_event.enqueue(delta);
            return vp::IO_REQ_GRANTED;
        }

        case Behavior::DENIED:
        {
            int64_t due = now + (r->retry_delay > 0 ? r->retry_delay : 1);
            // Schedule a retry at `due`. Multiple denies stack up — one retry per event.
            auto it = _this->pending_retries.begin();
            while (it != _this->pending_retries.end() && *it <= due) ++it;
            _this->pending_retries.insert(it, due);
            int64_t head_due = _this->pending_retries.front();
            int64_t delta = head_due - now;
            if (delta <= 0) delta = 1;
            if (_this->retry_event.is_enqueued()) _this->retry_event.cancel();
            _this->retry_event.enqueue(delta);
            return vp::IO_REQ_DENIED;
        }
    }
    return vp::IO_REQ_DONE;
}

void StubTarget::deferred_resp_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();

    // Fire all resps that are due at this cycle.
    while (!_this->pending_resps.empty()
            && _this->pending_resps.front().due_cycle <= now)
    {
        Pending p = _this->pending_resps.front();
        _this->pending_resps.pop_front();
        printf("[%ld] %s RESP addr=0x%lx\n",
            now, _this->logname.c_str(), p.req->get_addr());
        _this->in.resp(p.req);
    }

    if (!_this->pending_resps.empty())
    {
        int64_t delta = _this->pending_resps.front().due_cycle - now;
        if (delta <= 0) delta = 1;
        _this->resp_event.enqueue(delta);
    }
}

void StubTarget::deferred_retry_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();

    // Fire all retries that are due at this cycle. Each call to retry() is a single
    // edge; repeated calls are harmless per the v2 protocol.
    while (!_this->pending_retries.empty() && _this->pending_retries.front() <= now)
    {
        _this->pending_retries.pop_front();
        printf("[%ld] %s RETRY\n", now, _this->logname.c_str());
        _this->in.retry();
    }

    if (!_this->pending_retries.empty())
    {
        int64_t delta = _this->pending_retries.front() - now;
        if (delta <= 0) delta = 1;
        _this->retry_event.enqueue(delta);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubTarget(config);
}
