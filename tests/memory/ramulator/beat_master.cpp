// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Beat master for the memory.ramulator testbench (IoV2Beat protocol).
//
// For each schedule entry it issues ONE whole-burst request (full size,
// is_first = is_last = true, a burst_id) and expects the wrapper to stream the
// response back as ceil(size / beat_width) per-cycle beats (read data slices /
// write acks), with is_first on the first beat and is_last on the last. It
// polices the beat stream: exactly N beats, is_first/is_last placement,
// burst_id echo, status, and (for reads) the returned data.
//
// Schedule entry keys: cycle, addr, size, is_write, name, [data_hex].

#include <vp/vp.hpp>
#include <beat_master/ramulator_beat_master_config.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

class BeatMaster : public vp::Component
{
public:
    BeatMaster(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    RamulatorBeatMasterConfig cfg;
    struct Entry {
        int64_t cycle;
        uint64_t addr;
        uint64_t size;
        bool is_write;
        int64_t burst_id;
        std::string name;
        uint8_t *data_hex_buf;   // optional write payload (owned), or nullptr
        size_t data_hex_len;
    };

    // Live state for one outstanding burst, reached from each beat via
    // req->initiator.
    struct BurstState {
        Entry *entry;
        uint8_t *buffer;     // master-owned data buffer (whole burst)
        vp::IoReq *req;      // the single whole-burst descriptor
        int expected_beats;
        int beats_seen;
        int64_t last_resp_cycle;
    };

    static vp::IoRespAck resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this, vp::IoRetryChannel);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void send_burst(Entry *entry);
    vp::IoReqStatus issue(BurstState *bs);
    void handle_status(BurstState *bs, vp::IoReqStatus st);

    vp::IoMaster out;
    vp::ClockEvent issue_event;
    vp::ClockEvent quit_event;
    vp::Trace trace;
    std::vector<Entry *> schedule;
    size_t next_to_schedule = 0;
    int beat_width = 64;
    int outstanding = 0;
    // Bursts whose request was DENIED, awaiting retry() to re-send. A backend
    // that back-pressures (e.g. DRAMSys when its buffers fill) denies and later
    // retries; Ramulator always grants, so this stays empty there.
    std::deque<BurstState *> denied;
    std::string logname;
    int64_t quit_after_cycles = 200;

    // Speed-benchmark knobs. quiet=1 suppresses per-request/per-beat logging and
    // asserts (so wall time measures simulation, not printf). chain=1 + repeat=N
    // re-issues the (single-entry) schedule N times, one burst in flight at a
    // time (next issued when the previous completes), for a long dense run.
    bool quiet = false;
    bool chain = false;
    long repeat = 1;
    long bursts_issued = 0;
    long bursts_done = 0;
    long total_bursts = 1;
    long long beats_total = 0;
};

BeatMaster::BeatMaster(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      out(&BeatMaster::retry_handler, &BeatMaster::resp_handler),
      issue_event(this, &BeatMaster::issue_handler),
      quit_event(this, &BeatMaster::quit_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_master_port("output", &this->out);

    this->logname = this->cfg.logname;
    if (this->logname.empty()) this->logname = this->get_name();
    this->beat_width = this->cfg.beat_width;
    int qac = this->cfg.quit_after_cycles;
    if (qac > 0) this->quit_after_cycles = qac;
    this->quiet = this->cfg.quiet;
    this->chain = this->cfg.chain;
    int rep = this->cfg.repeat;
    this->repeat = rep > 0 ? rep : 1;

    int64_t burst_id = 0;
    {
        for (int si = 0; si < this->cfg.schedule_count; si++)
        {
            const RamulatorBeatMasterEntryConfig *item = &this->cfg.schedule[si];
            Entry *e = new Entry();
            e->cycle = item->cycle;
            e->addr = (uint64_t)item->addr;
            e->size = (uint64_t)item->size;
            e->is_write = item->is_write;
            e->burst_id = burst_id++;
            e->name = item->req_name;
            if (e->name.empty()) e->name = "b" + std::to_string(this->schedule.size());

            // Decode optional hex write payload.
            std::string data_hex = item->data_hex;
            e->data_hex_len = 0;
            e->data_hex_buf = nullptr;
            if (!data_hex.empty())
            {
                e->data_hex_len = data_hex.size() / 2;
                e->data_hex_buf = new uint8_t[e->data_hex_len];
                auto hexv = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                for (size_t i = 0; i < e->data_hex_len; i++)
                    e->data_hex_buf[i] = (hexv(data_hex[i * 2]) << 4) | hexv(data_hex[i * 2 + 1]);
            }
            this->schedule.push_back(e);
        }
    }
}

void BeatMaster::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_to_schedule == 0)
    {
        this->total_bursts = (long)this->schedule.size() * this->repeat;
        int64_t first = this->schedule[0]->cycle;
        if (first <= 0) first = 1;
        this->issue_event.enqueue(first);
    }
}

static vp::IoReq *_pool_alloc(BeatMaster *m, void *unused)
{
    (void)m; (void)unused;
    return vp::IoReqAllocator::get(0)->alloc();
}

void BeatMaster::send_burst(Entry *entry)
{
    int64_t now = this->clock.get_cycles();

    BurstState *bs = new BurstState();
    bs->entry = entry;
    bs->expected_beats = entry->size == 0 ? 1
        : (int)((entry->size + this->beat_width - 1) / this->beat_width);
    bs->beats_seen = 0;
    bs->last_resp_cycle = -1;

    // Writes: preload payload (data_hex, else addr-derived). Reads are
    // data-less (beat protocol): the data comes back inside the
    // allocator-backed response beats.
    if (entry->is_write)
    {
        uint64_t buf_size = entry->size == 0 ? 1 : entry->size;
        bs->buffer = new uint8_t[buf_size];
        for (uint64_t i = 0; i < entry->size; i++)
        {
            bs->buffer[i] = (entry->data_hex_buf && i < entry->data_hex_len)
                ? entry->data_hex_buf[i] : (uint8_t)((entry->addr + i) & 0xff);
        }
    }
    else
    {
        bs->buffer = nullptr;
    }

    // Writes: a size-0-pool one-beat burst the target consumes and frees
    // (per-burst write-ack contract); expect a single data-less ack.
    // Reads: our own data-less descriptor (initiator-owned, freed by us).
    if (entry->is_write)
    {
        bs->req = _pool_alloc(this, bs);
        bs->expected_beats = 1;
    }
    else
    {
        bs->req = new vp::IoReq(entry->addr, bs->buffer, entry->size, false);
    }

    if (!this->quiet)
        printf("[%ld] %s SEND name=%s addr=0x%lx size=%lu write=%d burst_id=%ld expect_beats=%d\n",
            now, this->logname.c_str(), entry->name.c_str(), entry->addr,
            (unsigned long)entry->size, entry->is_write ? 1 : 0,
            (long)entry->burst_id, bs->expected_beats);

    this->outstanding++;
    this->handle_status(bs, this->issue(bs));
}

// (Re)issue a burst's whole-descriptor request. Reset the fields each time so a
// re-send after DENIED is well-formed (prepare() does not reset the beat flags).
vp::IoReqStatus BeatMaster::issue(BurstState *bs)
{
    Entry *e = bs->entry;
    bs->req->set_addr(e->addr);
    bs->req->set_size(e->size);
    bs->req->set_is_write(e->is_write);
    bs->req->prepare();
    if (e->is_write)
    {
        // Size-0 pool object: data is caller-managed, set on every issue.
        bs->req->set_data(bs->buffer);
    }
    bs->req->is_first = true;
    bs->req->is_last = true;
    bs->req->burst_id = e->burst_id;
    bs->req->initiator = bs;
    vp::IoReqStatus st = this->out.req(bs->req);
    if (e->is_write && st == vp::IO_REQ_GRANTED)
    {
        // Ownership transferred: the target frees the beat; the burst ack
        // (distinct, data-less) correlates back via initiator.
        bs->req = nullptr;
    }
    return st;
}

void BeatMaster::handle_status(BurstState *bs, vp::IoReqStatus st)
{
    if (st == vp::IO_REQ_GRANTED)
    {
        // Accepted: the response streams back beat-by-beat via resp_handler.
        return;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        // Back-pressured: re-send when retry() fires.
        this->denied.push_back(bs);
        return;
    }
    // IO_REQ_DONE: answered inline (a last write beat's inline burst ack, or
    // an error). The master keeps the beat — recycle it to its pool.
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s DONE name=%s status=%d\n", now, this->logname.c_str(),
        bs->entry->name.c_str(), (int)bs->req->get_resp_status());
    if (bs->entry->is_write)
    {
        bs->req->free();
    }
    else
    {
        delete bs->req;
    }
    delete[] bs->buffer;
    delete bs;
    this->outstanding--;
    this->bursts_done++;
}

void BeatMaster::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    BeatMaster *_this = (BeatMaster *)__this;

    if (_this->chain)
    {
        // Speed mode: one burst in flight; the next is issued from resp_handler
        // when this one completes. Loop the schedule total_bursts times.
        if (_this->bursts_issued >= _this->total_bursts) return;
        Entry *entry = _this->schedule[_this->bursts_issued % _this->schedule.size()];
        _this->bursts_issued++;
        _this->send_burst(entry);
        return;
    }

    if (_this->next_to_schedule >= _this->schedule.size()) return;

    Entry *entry = _this->schedule[_this->next_to_schedule++];
    _this->send_burst(entry);

    if (_this->next_to_schedule < _this->schedule.size())
    {
        int64_t now = _this->clock.get_cycles();
        int64_t delta = _this->schedule[_this->next_to_schedule]->cycle - now;
        if (delta <= 0) delta = 1;
        _this->issue_event.enqueue(delta);
    }
    else
    {
        _this->quit_event.enqueue(_this->quit_after_cycles);
    }
}

void BeatMaster::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    BeatMaster *_this = (BeatMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    _this->traces.assert(_this->outstanding == 0,
        "%d burst(s) never completed", _this->outstanding);
    long long bytes = _this->beats_total * (long long)_this->beat_width;
    printf("[%ld] %s QUIT bursts=%ld beats=%lld bytes=%lld sim_cycles=%ld\n",
        now, _this->logname.c_str(), _this->bursts_done, _this->beats_total,
        bytes, (long)now);
    _this->time.get_engine()->quit(0);
}

vp::IoRespAck BeatMaster::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    BeatMaster *_this = (BeatMaster *)__this;
    int64_t now = _this->clock.get_cycles();
    BurstState *bs = (BurstState *)req->initiator;
    Entry *e = bs->entry;

    _this->beats_total++;

    if (!_this->quiet)
    {
        char hex[17] = { 0 };
        uint8_t *d = req->get_data();
        int n = (d != nullptr && req->get_size() < 8) ? (int)req->get_size()
                : (d != nullptr ? 8 : 0);
        for (int i = 0; i < n; i++) snprintf(&hex[i * 2], 3, "%02x", d[i]);

        printf("[%ld] %s RESP name=%s beat=%d/%d addr=0x%lx size=%lu first=%d last=%d status=%d data=%s\n",
            now, _this->logname.c_str(), e->name.c_str(),
            bs->beats_seen, bs->expected_beats, (unsigned long)req->get_addr(),
            (unsigned long)req->get_size(), req->is_first ? 1 : 0,
            req->is_last ? 1 : 0, (int)req->get_resp_status(), hex);

        // ---- Beat-stream protocol assertions (skip in quiet speed runs) ----
        _this->traces.assert(req->burst_id == e->burst_id,
            "beat burst_id %ld != expected %ld", (long)req->burst_id, (long)e->burst_id);
        if (e->is_write)
        {
            // Per-burst ack contract: exactly one data-less ack.
            _this->traces.assert(req->is_last && req->get_data() == nullptr,
                "malformed write burst ack (last=%d, data=%p)",
                req->is_last ? 1 : 0, req->get_data());
        }
        else if (bs->beats_seen == 0)
            _this->traces.assert(req->is_first, "first beat must have is_first=1");
        else
            _this->traces.assert(!req->is_first, "non-first beat must have is_first=0");
        _this->traces.assert(now > bs->last_resp_cycle,
            "more than one beat in cycle %ld (beat channel is 1/cycle)", now);
    }
    bs->last_resp_cycle = now;

    bs->beats_seen++;
    bool last = req->is_last;

    // Read beats and the write burst ack are distinct allocator-backed
    // objects — free each back to its pool (our own read descriptor is
    // freed separately below; the write descriptor was consumed by the
    // target at grant time).
    if (req != bs->req)
    {
        req->free();
    }

    if (last)
    {
        if (!_this->quiet)
            _this->traces.assert(bs->beats_seen == bs->expected_beats,
                "got %d beats, expected %d", bs->beats_seen, bs->expected_beats);
        if (bs->req != nullptr)
        {
            delete bs->req;
        }
        delete[] bs->buffer;
        delete bs;
        _this->outstanding--;
        _this->bursts_done++;

        if (_this->chain)
        {
            if (_this->bursts_done >= _this->total_bursts)
                _this->quit_event.enqueue(1);          // whole run done
            else if (_this->bursts_issued < _this->total_bursts)
                _this->issue_event.enqueue(1);         // issue the next burst
        }
    }

    return vp::IO_RESP_ACCEPTED;
}

void BeatMaster::retry_handler(vp::Block *__this, vp::IoRetryChannel)
{
    BeatMaster *_this = (BeatMaster *)__this;
    // Re-send denied bursts. If one is denied again, put it back and wait for
    // the next retry() rather than spinning.
    while (!_this->denied.empty())
    {
        BurstState *bs = _this->denied.front();
        _this->denied.pop_front();
        vp::IoReqStatus st = _this->issue(bs);
        if (st == vp::IO_REQ_DENIED)
        {
            _this->denied.push_front(bs);
            break;
        }
        _this->handle_status(bs, st);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new BeatMaster(config);
}
