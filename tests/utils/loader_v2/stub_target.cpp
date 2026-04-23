// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Testbench memory target for loader_v2.
//
// Behaves per a list of rules (same shape as the interco stub_target —
// addr_min, addr_max, behavior, resp_delay, retry_delay), accumulates
// the incoming writes into an internal byte store, and logs each
// REQ/RESP/RETRY so the checker can verify address+data coverage.

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class StubTarget : public vp::Component
{
public:
    StubTarget(vp::ComponentConf &conf);

private:
    enum class Behavior { DONE, DONE_INVALID, GRANTED, DENIED };

    struct Rule {
        uint64_t  addr_min;
        uint64_t  addr_max;
        Behavior  behavior;
        int64_t   resp_delay;
        int64_t   retry_delay;
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void            deferred_resp_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            deferred_retry_handler(vp::Block *__this, vp::ClockEvent *event);

    Rule *rule_for(uint64_t addr);
    void  store_bytes(uint64_t addr, uint8_t *data, uint64_t size);

    vp::IoSlave     in;
    vp::ClockEvent  resp_event;
    vp::ClockEvent  retry_event;
    vp::Trace       trace;
    std::vector<Rule> rules;
    std::string     logname;

    struct Pending { vp::IoReq *req; int64_t due_cycle; };
    std::deque<Pending> pending_resps;
    std::deque<int64_t> pending_retries;

    // Byte-addressable store. Only grows.
    std::unordered_map<uint64_t, uint8_t> memory;
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
            r.resp_delay  = item->get_child_int("resp_delay");
            r.retry_delay = item->get_child_int("retry_delay");
            this->rules.push_back(r);
        }
    }
}


StubTarget::Rule *StubTarget::rule_for(uint64_t addr)
{
    for (Rule &r : this->rules)
    {
        if (addr >= r.addr_min && addr <= r.addr_max) return &r;
    }
    return nullptr;
}


void StubTarget::store_bytes(uint64_t addr, uint8_t *data, uint64_t size)
{
    if (data == nullptr) return;
    for (uint64_t i = 0; i < size; i++)
    {
        this->memory[addr + i] = data[i];
    }
}


vp::IoReqStatus StubTarget::req_handler(vp::Block *__this, vp::IoReq *req)
{
    StubTarget *_this = (StubTarget *)__this;
    int64_t now = _this->clock.get_cycles();

    // Build a short hex fingerprint of the first up-to-8 data bytes so
    // the checker can eyeball (and grep) what each chunk carried.
    char hex[4 + 8 * 2 + 1] = { 0 };
    {
        char *p = hex;
        uint64_t n = req->get_size() < 8 ? req->get_size() : 8;
        for (uint64_t i = 0; i < n; i++)
        {
            uint8_t b = req->get_data() != nullptr
                            ? req->get_data()[i]
                            : (uint8_t)0;
            snprintf(p, 3, "%02x", b);
            p += 2;
        }
        *p = '\0';
    }

    printf("[%ld] %s REQ addr=0x%lx size=%lu write=%d data=%s\n",
        now, _this->logname.c_str(), req->get_addr(), req->get_size(),
        req->get_is_write() ? 1 : 0, hex);

    Rule *r = _this->rule_for(req->get_addr());
    Behavior b = r ? r->behavior : Behavior::DONE;

    switch (b)
    {
        case Behavior::DONE:
            if (req->get_is_write())
            {
                _this->store_bytes(req->get_addr(), req->get_data(),
                                    req->get_size());
            }
            req->set_resp_status(vp::IO_RESP_OK);
            return vp::IO_REQ_DONE;

        case Behavior::DONE_INVALID:
            req->set_resp_status(vp::IO_RESP_INVALID);
            return vp::IO_REQ_DONE;

        case Behavior::GRANTED:
        {
            if (req->get_is_write())
            {
                _this->store_bytes(req->get_addr(), req->get_data(),
                                    req->get_size());
            }
            req->set_resp_status(vp::IO_RESP_OK);
            int64_t due = now + r->resp_delay;
            Pending p{req, due};
            auto it = _this->pending_resps.begin();
            while (it != _this->pending_resps.end() && it->due_cycle <= due) ++it;
            _this->pending_resps.insert(it, p);
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
    while (!_this->pending_retries.empty()
            && _this->pending_retries.front() <= now)
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
