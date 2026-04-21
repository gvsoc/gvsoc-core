// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Testbench master for router_async_v2 (io_v2 protocol, beat mode).
 *
 * Schedule entry keys (all optional except cycle/addr/size):
 *   cycle      : issue cycle of the first beat of this burst
 *   addr       : base address (beat i uses addr + i*stride)
 *   size       : per-beat size (<= router width)
 *   stride     : address stride between beats; defaults to size
 *   nb_beats   : number of beats in this burst (default 1)
 *   burst_id   : shared across beats of this burst (default -1)
 *   is_write   : false for reads
 *   name       : label for logs
 *
 * The master emits one beat per cycle until the burst is done, then idles until the
 * next schedule entry. Denied beats go back to a retry queue and are re-issued on
 * retry().
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

class StubMaster : public vp::Component
{
public:
    StubMaster(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    struct BurstEntry {
        int64_t start_cycle;
        uint64_t base_addr;
        uint64_t size;
        uint64_t stride;
        int nb_beats;
        int64_t burst_id;
        bool is_write;
        std::string name;
    };

    struct Beat {
        uint64_t addr;
        bool is_first;
        bool is_last;
        int idx;                 // 0..nb_beats-1 (for logging)
        BurstEntry *burst;
        vp::IoReq *req;
        uint8_t *data;
    };

    static void resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void emit_next_beat(BurstEntry *burst);
    void try_send(Beat *beat);

    vp::IoMaster out;
    vp::ClockEvent issue_event;
    vp::ClockEvent quit_event;
    vp::Trace trace;
    std::vector<BurstEntry *> schedule;
    size_t next_to_schedule = 0;
    int next_beat_idx = 0;                // index of the next beat to emit in the current burst
    std::deque<Beat *> denied_queue;      // beats waiting for retry
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

    js::Config *sched = this->get_js_config()->get("schedule");
    if (sched != NULL)
    {
        for (auto &item : sched->get_elems())
        {
            BurstEntry *b = new BurstEntry();
            b->start_cycle = item->get_int("cycle");
            b->base_addr = (uint64_t)item->get_int("addr");
            b->size = (uint64_t)item->get_int("size");
            b->nb_beats = item->get_child_int("nb_beats");
            if (b->nb_beats <= 0) b->nb_beats = 1;
            b->stride = (uint64_t)item->get_int("stride");
            if (b->stride == 0) b->stride = b->size;
            b->burst_id = (int64_t)item->get_int("burst_id");
            if (item->get("burst_id") == nullptr) b->burst_id = -1;
            b->is_write = item->get_child_bool("is_write");
            b->name = item->get_child_str("name");
            if (b->name.empty()) b->name = "b" + std::to_string(this->schedule.size());
            this->schedule.push_back(b);
        }
    }
}

void StubMaster::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_to_schedule == 0)
    {
        int64_t first = this->schedule[0]->start_cycle;
        if (first <= 0) first = 1;
        this->issue_event.enqueue(first);
    }
}

void StubMaster::try_send(Beat *beat)
{
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s SEND name=%s#%d addr=0x%lx size=%lu write=%d burst_id=%ld first=%d last=%d\n",
        now, this->logname.c_str(), beat->burst->name.c_str(), beat->idx,
        beat->addr, beat->burst->size, beat->burst->is_write ? 1 : 0,
        (long)beat->burst->burst_id, beat->is_first ? 1 : 0, beat->is_last ? 1 : 0);

    // Configure the IoReq from our Beat snapshot (the router may mutate addr).
    beat->req->set_addr(beat->addr);
    beat->req->set_size(beat->burst->size);
    beat->req->set_is_write(beat->burst->is_write);
    beat->req->is_first = beat->is_first;
    beat->req->is_last = beat->is_last;
    beat->req->burst_id = beat->burst->burst_id;
    // Stash ourselves in initiator BEFORE req(). The router saves/restores it across
    // the round-trip so resp_handler will see `beat` back.
    beat->req->initiator = beat;

    vp::IoReqStatus st = this->out.req(beat->req);
    switch (st)
    {
        case vp::IO_REQ_DONE:
            printf("[%ld] %s DONE name=%s#%d status=%d\n",
                now, this->logname.c_str(), beat->burst->name.c_str(), beat->idx,
                (int)beat->req->get_resp_status());
            delete[] beat->data;
            delete beat->req;
            delete beat;
            break;
        case vp::IO_REQ_GRANTED:
            printf("[%ld] %s GRANTED name=%s#%d\n",
                now, this->logname.c_str(), beat->burst->name.c_str(), beat->idx);
            // Beat is in-flight; resp_handler will free the Beat when it arrives.
            break;
        case vp::IO_REQ_DENIED:
            printf("[%ld] %s DENIED name=%s#%d\n",
                now, this->logname.c_str(), beat->burst->name.c_str(), beat->idx);
            this->denied_queue.push_back(beat);
            break;
    }
}

void StubMaster::emit_next_beat(BurstEntry *burst)
{
    Beat *beat = new Beat();
    beat->idx = this->next_beat_idx;
    beat->is_first = (beat->idx == 0);
    beat->is_last = (beat->idx == burst->nb_beats - 1);
    beat->addr = burst->base_addr + (uint64_t)beat->idx * burst->stride;
    beat->burst = burst;
    beat->data = new uint8_t[burst->size];
    for (uint64_t i = 0; i < burst->size; i++) beat->data[i] = 0;
    beat->req = new vp::IoReq(beat->addr, beat->data, burst->size, burst->is_write);
    // set first/last/burst_id in try_send, which runs right now.
    this->try_send(beat);
}

void StubMaster::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    if (_this->next_to_schedule >= _this->schedule.size()) return;

    BurstEntry *burst = _this->schedule[_this->next_to_schedule];

    _this->emit_next_beat(burst);
    _this->next_beat_idx++;

    if (_this->next_beat_idx >= burst->nb_beats)
    {
        // Burst complete (all beats emitted). Move to next burst.
        _this->next_to_schedule++;
        _this->next_beat_idx = 0;
        if (_this->next_to_schedule < _this->schedule.size())
        {
            int64_t now = _this->clock.get_cycles();
            int64_t next_cycle = _this->schedule[_this->next_to_schedule]->start_cycle;
            int64_t delta = next_cycle - now;
            if (delta <= 0) delta = 1;
            _this->issue_event.enqueue(delta);
        }
        else
        {
            _this->quit_event.enqueue(_this->quit_after_cycles);
        }
    }
    else
    {
        // Next beat in same burst, 1 cycle later.
        _this->issue_event.enqueue(1);
    }
}

void StubMaster::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    _this->time.get_engine()->quit(0);
}

void StubMaster::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    Beat *beat = (Beat *)req->initiator;
    const char *name = beat ? beat->burst->name.c_str() : "?";
    int idx = beat ? beat->idx : -1;
    printf("[%ld] %s RESP name=%s#%d status=%d\n",
        now, _this->logname.c_str(), name, idx, (int)req->get_resp_status());
    if (beat)
    {
        delete[] beat->data;
        delete beat->req;
        delete beat;
    }
}

void StubMaster::retry_handler(vp::Block *__this)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s RETRY queue=%zu\n",
        now, _this->logname.c_str(), _this->denied_queue.size());

    while (!_this->denied_queue.empty())
    {
        Beat *beat = _this->denied_queue.front();
        _this->denied_queue.pop_front();
        size_t before = _this->denied_queue.size();
        _this->try_send(beat);
        if (_this->denied_queue.size() > before)
        {
            // DENIED again; stop draining, wait for next retry.
            break;
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubMaster(config);
}
