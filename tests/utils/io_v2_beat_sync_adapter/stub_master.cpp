// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Beat-master testbench for IoV2BeatSyncAdapter. It declares signature
 * IoV2Beat on its output, so binding it to an IoV2Sync stub target makes the
 * framework auto-insert the adapter between them.
 *
 * Unlike a per-beat router, this master issues ONE whole-burst descriptor per
 * schedule entry (size = the full burst) and expects the adapter to stream back
 * ceil(size / beat_width) per-beat resp() calls. It POLICES the response stream
 * with traces.assert (active only in asserts/debug builds): exactly N beats,
 * is_first/is_last placement, burst_id, ≤1 beat/cycle, status, and read data.
 *
 * Schedule entry keys (cycle/addr/size required):
 *   cycle        : issue cycle of this burst
 *   addr         : base address
 *   size         : whole-burst size in bytes (0 => one zero-size beat)
 *   is_write     : false for reads
 *   burst_id     : tag echoed on every beat (default -1)
 *   expect_status: 0 => IO_RESP_OK, 1 => IO_RESP_INVALID (default 0)
 *   name         : log label
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
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
        bool is_write;
        int64_t burst_id;
        int expect_status;
        std::string name;
    };

    // Live state for one outstanding burst, reachable from each resp() beat via
    // req->initiator.
    struct BurstState {
        BurstEntry *entry;
        uint8_t *buffer;        // master-owned data buffer
        vp::IoReq *req;         // the original whole-burst descriptor
        int expected_beats;
        int beats_seen;
        int64_t last_resp_cycle;
    };

    static vp::IoRespAck resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this, vp::IoRetryChannel);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void send_burst(BurstEntry *entry);

    vp::IoMaster out;
    vp::ClockEvent issue_event;
    vp::ClockEvent quit_event;
    vp::Trace trace;
    std::vector<BurstEntry *> schedule;
    size_t next_to_schedule = 0;
    int beat_width = 0;
    int outstanding = 0;
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

    this->beat_width = this->get_js_config()->get_child_int("beat_width");

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
            b->is_write = item->get_child_bool("is_write");
            b->burst_id = (int64_t)item->get_int("burst_id");
            if (item->get("burst_id") == nullptr) b->burst_id = -1;
            b->expect_status = item->get_child_int("expect_status");
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

void StubMaster::send_burst(BurstEntry *entry)
{
    int64_t now = this->clock.get_cycles();

    BurstState *bs = new BurstState();
    bs->entry = entry;
    bs->expected_beats = entry->size == 0 ? 1
        : (int)((entry->size + this->beat_width - 1) / this->beat_width);
    bs->beats_seen = 0;
    bs->last_resp_cycle = -1;

    uint64_t buf_size = entry->size == 0 ? 1 : entry->size;
    bs->buffer = new uint8_t[buf_size];
    // For a write, preload the buffer with the addr-derived pattern (the sync
    // target may check it). For a read, zero it; the target fills it.
    for (uint64_t i = 0; i < entry->size; i++)
    {
        bs->buffer[i] = entry->is_write ? (uint8_t)((entry->base_addr + i) & 0xff) : 0;
    }

    bs->req = new vp::IoReq(entry->base_addr, bs->buffer, entry->size, entry->is_write);
    bs->req->is_first = true;
    bs->req->is_last = true;
    bs->req->burst_id = entry->burst_id;
    bs->req->initiator = bs;

    printf("[%ld] %s SEND name=%s addr=0x%lx size=%lu write=%d burst_id=%ld expect_beats=%d\n",
        now, this->logname.c_str(), entry->name.c_str(), entry->base_addr,
        entry->size, entry->is_write ? 1 : 0, (long)entry->burst_id,
        bs->expected_beats);

    this->outstanding++;
    vp::IoReqStatus st = this->out.req(bs->req);

    // The adapter always accepts and streams the response back via resp().
    this->traces.assert(st == vp::IO_REQ_GRANTED,
        "adapter must return IO_REQ_GRANTED for a beat master (got %d)", (int)st);
}

void StubMaster::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    if (_this->next_to_schedule >= _this->schedule.size()) return;

    BurstEntry *entry = _this->schedule[_this->next_to_schedule];
    _this->send_burst(entry);
    _this->next_to_schedule++;

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

void StubMaster::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    // Every burst must have fully drained by quit time.
    _this->traces.assert(_this->outstanding == 0,
        "%d burst(s) never completed", _this->outstanding);
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    _this->time.get_engine()->quit(0);
}

vp::IoRespAck StubMaster::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMaster *_this = (StubMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    BurstState *bs = (BurstState *)req->initiator;
    BurstEntry *e = bs->entry;

    printf("[%ld] %s RESP name=%s beat=%d/%d addr=0x%lx size=%lu first=%d last=%d status=%d\n",
        now, _this->logname.c_str(), e->name.c_str(),
        bs->beats_seen, bs->expected_beats, req->get_addr(),
        (unsigned long)req->get_size(), req->is_first ? 1 : 0,
        req->is_last ? 1 : 0, (int)req->get_resp_status());

    // ---- Protocol assertions (the adapter / target behave correctly) ----
    _this->traces.assert(req->burst_id == e->burst_id,
        "beat burst_id %ld != expected %ld", (long)req->burst_id, (long)e->burst_id);
    if (bs->beats_seen == 0)
        _this->traces.assert(req->is_first, "first beat must have is_first=1");
    else
        _this->traces.assert(!req->is_first, "non-first beat must have is_first=0");
    _this->traces.assert(now > bs->last_resp_cycle,
        "more than one beat in cycle %ld (beat channel is 1/cycle)", now);
    bs->last_resp_cycle = now;
    _this->traces.assert(
        (int)req->get_resp_status() == (e->expect_status ? (int)vp::IO_RESP_INVALID
                                                         : (int)vp::IO_RESP_OK),
        "beat status %d != expected", (int)req->get_resp_status());
    // Read data must carry the target's addr-derived pattern.
    if (!e->is_write && req->get_resp_status() == vp::IO_RESP_OK)
    {
        uint8_t *d = req->get_data();
        for (uint64_t i = 0; i < req->get_size(); i++)
        {
            _this->traces.assert(d[i] == (uint8_t)((req->get_addr() + i) & 0xff),
                "read data mismatch at beat addr 0x%lx byte %lu", req->get_addr(), i);
        }
    }

    bs->beats_seen++;
    bool last = req->is_last;

    // ---- Ownership: free every object we receive ----
    // The adapter returns our own descriptor (bs->req) as the last beat of any
    // read/write (and on every write ack); only non-last read beats are distinct
    // adapter-allocated objects. So free any beat that isn't our descriptor now,
    // and free the descriptor (plus our buffer/state) once on the last beat.
    if (req != bs->req)
    {
        delete req;     // a distinct non-last read-beat object
    }

    if (last)
    {
        _this->traces.assert(bs->beats_seen == bs->expected_beats,
            "got %d beats, expected %d", bs->beats_seen, bs->expected_beats);
        delete bs->req;       // our descriptor (returned as last read beat / write ack)
        delete[] bs->buffer;
        delete bs;
        _this->outstanding--;
    }

    return vp::IO_RESP_ACCEPTED;
}

void StubMaster::retry_handler(vp::Block *__this, vp::IoRetryChannel)
{
    StubMaster *_this = (StubMaster *)__this;
    // The adapter never denies a beat master (it always accepts and streams),
    // so it must never retry.
    _this->traces.assert(false, "unexpected retry() from the beat-sync adapter");
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubMaster(config);
}
