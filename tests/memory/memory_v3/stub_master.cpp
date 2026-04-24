// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Testbench master for router_v2 (io_v2 protocol).
 *
 * Reads a schedule from get_js_config()/schedule: a list of entries with
 *   { cycle, addr, size, is_write, name }
 * The master sends each request at its issue cycle. If the send returns DENIED, the
 * master remembers the request and re-sends it as soon as retry() fires. Each event
 * (SEND, DENY, RETRY, GRANT, RESP, DONE) is printed with the current cycle and the
 * entry name so the test can compare against a reference log.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <deque>
#include <string>

class StubMaster : public vp::Component
{
public:
    StubMaster(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    struct ScheduleEntry {
        int64_t cycle;
        uint64_t addr;
        uint64_t size;
        bool is_write;
        std::string name;
        vp::IoReq *req;     // owned
        uint8_t *data;      // owned
        bool sent = false;  // true once SEND has been attempted (and accepted) at least
    };

    static vp::IoReqStatus retry_default(vp::Block *) { return vp::IO_REQ_DONE; } // unused
    static void resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void issue(ScheduleEntry *entry);
    ScheduleEntry *entry_from_req(vp::IoReq *req);

    vp::IoMaster out;
    vp::ClockEvent issue_event;
    vp::ClockEvent quit_event;
    vp::Trace trace;
    std::vector<ScheduleEntry *> schedule;
    size_t next_to_schedule = 0;  // index into schedule, in issue order
    // Requests whose SEND returned DENIED and are waiting for retry(). FIFO.
    std::deque<ScheduleEntry *> denied_queue;
    std::string logname;
    int64_t quit_after_cycles = 100;
};

StubMaster::StubMaster(vp::ComponentConf &config)
    : vp::Component(config),
      out(&StubMaster::retry_handler, &StubMaster::resp_handler),
      issue_event(this, &StubMaster::issue_handler),
      quit_event(this, &StubMaster::quit_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_master_port("output", &this->out);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();

    int qac = this->get_js_config()->get_child_int("quit_after_cycles");
    if (qac > 0) this->quit_after_cycles = qac;

    js::Config *schedule_cfg = this->get_js_config()->get("schedule");
    if (schedule_cfg != NULL)
    {
        for (auto &item : schedule_cfg->get_elems())
        {
            ScheduleEntry *e = new ScheduleEntry();
            e->cycle = item->get_int("cycle");
            e->addr = (uint64_t)item->get_int("addr");
            e->size = (uint64_t)item->get_int("size");
            e->is_write = item->get_child_bool("is_write");
            e->name = item->get_child_str("name");
            if (e->name.empty()) e->name = "req" + std::to_string(this->schedule.size());
            e->data = new uint8_t[e->size];
            for (uint64_t i = 0; i < e->size; i++) e->data[i] = 0;
            // Optional hex pre-fill for the data buffer: the JSON field
            // ``data_hex`` is a contiguous hex string (e.g. "deadbeef") whose
            // bytes are written into ``e->data`` before the request goes
            // out. Useful for writes that need a predictable payload, and
            // for the second operand of atomics.
            std::string data_hex = item->get_child_str("data_hex");
            for (uint64_t i = 0; i < e->size && i * 2 + 1 < data_hex.size(); i++)
            {
                auto hexv = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                e->data[i] = (hexv(data_hex[i*2]) << 4) | hexv(data_hex[i*2+1]);
            }
            e->req = new vp::IoReq(e->addr, e->data, e->size, e->is_write);
            this->schedule.push_back(e);
        }
    }
}

void StubMaster::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_to_schedule == 0)
    {
        // Kick the first issue on reset de-assertion. issue_handler chains.
        int64_t first = this->schedule[0]->cycle;
        if (first <= 0) first = 1;
        this->issue_event.enqueue(first);
    }
}

StubMaster::ScheduleEntry *StubMaster::entry_from_req(vp::IoReq *req)
{
    for (ScheduleEntry *e : this->schedule)
    {
        if (e->req == req) return e;
    }
    return nullptr;
}

void StubMaster::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    if (_this->next_to_schedule >= _this->schedule.size()) return;

    ScheduleEntry *e = _this->schedule[_this->next_to_schedule++];
    _this->issue(e);

    // Schedule next issue if any, else arm the quit event.
    if (_this->next_to_schedule < _this->schedule.size())
    {
        int64_t now = _this->clock.get_cycles();
        int64_t next_cycle = _this->schedule[_this->next_to_schedule]->cycle;
        int64_t delta = next_cycle - now;
        if (delta <= 0) delta = 1;
        _this->issue_event.enqueue(delta);
    }
    else
    {
        _this->quit_event.enqueue(_this->quit_after_cycles);
    }
}

void StubMaster::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    _this->time.get_engine()->quit(0);
}

void StubMaster::issue(ScheduleEntry *entry)
{
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s SEND name=%s addr=0x%lx size=%lu write=%d\n",
        now, this->logname.c_str(), entry->name.c_str(),
        entry->addr, entry->size, entry->is_write ? 1 : 0);

    // Reset the IoReq addr in case it was mutated by the router's address translation on
    // a previous attempt. Keep the data pointer. Also reset latency.
    entry->req->set_addr(entry->addr);
    entry->req->set_size(entry->size);
    entry->req->set_is_write(entry->is_write);
    entry->req->prepare();

    vp::IoReqStatus st = this->out.req(entry->req);
    switch (st)
    {
        case vp::IO_REQ_DONE:
        {
            char hex[17] = { 0 };
            int n = entry->size < 8 ? (int)entry->size : 8;
            for (int i = 0; i < n; i++)
                snprintf(&hex[i*2], 3, "%02x", entry->data[i]);
            printf("[%ld] %s DONE name=%s status=%d latency=%ld data=%s\n",
                now, this->logname.c_str(), entry->name.c_str(),
                (int)entry->req->get_resp_status(), entry->req->get_latency(), hex);
            break;
        }
        case vp::IO_REQ_GRANTED:
            printf("[%ld] %s GRANTED name=%s\n",
                now, this->logname.c_str(), entry->name.c_str());
            break;
        case vp::IO_REQ_DENIED:
            printf("[%ld] %s DENIED name=%s\n",
                now, this->logname.c_str(), entry->name.c_str());
            this->denied_queue.push_back(entry);
            break;
    }
}

void StubMaster::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMaster *_this = (StubMaster *)__this;
    ScheduleEntry *e = _this->entry_from_req(req);
    int64_t now = _this->clock.get_cycles();
    const char *name = e ? e->name.c_str() : "?";
    char hex[17] = { 0 };
    if (e != nullptr)
    {
        int n = e->size < 8 ? (int)e->size : 8;
        for (int i = 0; i < n; i++)
            snprintf(&hex[i*2], 3, "%02x", e->data[i]);
    }
    printf("[%ld] %s RESP name=%s status=%d latency=%ld data=%s\n",
        now, _this->logname.c_str(), name,
        (int)req->get_resp_status(), req->get_latency(), hex);
}

void StubMaster::retry_handler(vp::Block *__this)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s RETRY queue=%zu\n",
        now, _this->logname.c_str(), _this->denied_queue.size());

    while (!_this->denied_queue.empty())
    {
        ScheduleEntry *e = _this->denied_queue.front();
        _this->denied_queue.pop_front();
        size_t before_size = _this->denied_queue.size();
        _this->issue(e);
        // issue() re-pushes on DENIED -> queue grew by one. Stop; the next retry will
        // continue draining.
        if (_this->denied_queue.size() > before_size)
        {
            break;
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubMaster(config);
}
