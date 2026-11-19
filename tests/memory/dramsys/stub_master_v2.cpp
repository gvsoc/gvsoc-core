/*
 * Testbench master for the memory.dramsys_v2 wrapper (io v2 protocol,
 * beat stream).
 *
 * Each schedule entry is one issue (req() call). Fields:
 *   { cycle, addr, size, is_write, name, data_hex,
 *     is_first, is_last, burst_id, chain_to_prev }
 *
 *   - For READS: one entry with is_first=is_last=true, size = total burst
 *     size. The wrapper streams beat resps back; the master XOR-accumulates
 *     each beat's bytes and emits ONE "RESP" stdout line on the final beat
 *     with the cumulative checksum (so the testset.cfg checkers can match
 *     against an expected XOR like in the v1 tests).
 *   - For WRITES (beat-form): multiple entries with shared burst_id, each
 *     size <= beat_width; first carries is_first=true, last carries
 *     is_last=true. Each req returns IO_REQ_DONE inline (the wrapper
 *     accepts the beat in zero time).
 *
 * Statuses handled: IO_REQ_DONE (log DONE inline), IO_REQ_GRANTED (register
 * an accumulator for the upcoming beat resps), IO_REQ_DENIED (push to local
 * denied queue, re-issue from retry_handler).
 *
 * chain_to_prev: when true, the entry is NOT fired by the cycle timer;
 * instead it's enqueued on chain_event (+1 cycle) from mark_resolved() of
 * its predecessor — same deferral pattern as stub_master_v1.cpp.
 */

#include <vp/vp.hpp>
#include <stub_master_v2/dramsys_stub_master_v2_config.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io_v2.hpp>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>


class StubMasterV2 : public vp::Component
{
public:
    StubMasterV2(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    DramsysStubMasterV2Config cfg;
    struct ScheduleEntry {
        int64_t  cycle;
        uint64_t addr;
        uint64_t size;
        bool     is_write;
        // Write-burst bookkeeping (per-burst ack contract): the burst's
        // is_first entry is the shared initiator of all its beats and
        // records which entry carries is_last, so the single ack can
        // resolve it.
        ScheduleEntry *burst_last_entry = nullptr;
        vp::IoRespStatus wr_status = vp::IO_RESP_OK;
        uint64_t wr_latency = 0;
        std::string name;
        std::string data_hex;
        bool     is_first = true;
        bool     is_last  = true;
        int64_t  burst_id = -1;
        bool     chain_to_prev = false;
        size_t   idx = 0;
        // Owned IoReq + data buffer.
        vp::IoReq *req = nullptr;
        uint8_t   *data = nullptr;
        // Read-side accumulator state (only used when this is a read).
        uint64_t bytes_received = 0;
        uint8_t  preview[8] = {0};   // first 8 bytes from the FIRST beat
        uint8_t  checksum = 0;
        bool     resolved = false;
    };

    static vp::IoReqStatus retry_unused(vp::Block *) { return vp::IO_REQ_DONE; }  // unused
    static vp::IoRespAck resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this, vp::IoRetryChannel);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void chain_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void issue(ScheduleEntry *entry);
    void mark_resolved(ScheduleEntry *entry);
    void release_if_idle();
    void log_done(const char *tag, ScheduleEntry *e);
    ScheduleEntry *entry_from_req(vp::IoReq *req);

    vp::Trace trace;
    vp::IoMaster out;
    // Size-0 pool for write beats (consumed and freed downstream).
    vp::IoReqAllocator *wr_allocator;
    // First entry of the write burst currently being issued (shared
    // initiator for all its beats).
    ScheduleEntry *open_write_first = nullptr;
    vp::ClockEvent issue_event;
    vp::ClockEvent chain_event;
    vp::ClockEvent quit_event;

    // VCD-visible request trace, mirrors stub_master_v1.
    vp::Signal<uint64_t> sig_req_addr;
    vp::Signal<uint64_t> sig_req_size;
    vp::Signal<bool>     sig_req_is_write;
    vp::Signal<uint64_t> sig_pending;

    std::vector<ScheduleEntry *> schedule;
    size_t   next_to_schedule = 0;
    size_t   nb_resolved = 0;
    int      nb_inflight = 0;
    std::deque<ScheduleEntry *> denied_queue;
    ScheduleEntry *pending_chain = nullptr;
    std::string logname;
    int64_t  quit_after_cycles = 10000;
};


StubMasterV2::StubMasterV2(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      out(&StubMasterV2::retry_handler, &StubMasterV2::resp_handler),
      issue_event(this, &StubMasterV2::issue_handler),
      chain_event(this, &StubMasterV2::chain_handler),
      quit_event(this, &StubMasterV2::quit_handler),
      sig_req_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
      sig_req_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
      sig_req_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      sig_pending(*this, "pending", 64)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_master_port("output", &this->out);

    this->wr_allocator = vp::IoReqAllocator::get(0);
    this->logname = this->cfg.logname;
    if (this->logname.empty()) this->logname = this->get_name();

    int qac = this->cfg.quit_after_cycles;
    if (qac > 0) this->quit_after_cycles = qac;

    {
        for (int si = 0; si < this->cfg.schedule_count; si++)
        {
            const DramsysStubMasterV2EntryConfig *item = &this->cfg.schedule[si];
            ScheduleEntry *e = new ScheduleEntry();
            e->cycle    = item->cycle;
            e->addr     = (uint64_t)item->addr;
            e->size     = (uint64_t)item->size;
            e->is_write = item->is_write;
            e->name     = item->req_name;
            if (e->name.empty()) e->name = "req" + std::to_string(this->schedule.size());

            // Per-beat metadata. Defaults: a single-beat single-req burst.
            // get_child_bool returns false by default, so the explicit
            // is_first/is_last fields below override only when present.
            e->is_first = item->is_first;
            e->is_last  = item->is_last;
            e->burst_id = item->burst_id;
            e->chain_to_prev = item->chain_to_prev;
            e->idx = this->schedule.size();

            e->data = new uint8_t[e->size > 0 ? e->size : 1];
            for (uint64_t i = 0; i < e->size; i++) e->data[i] = 0;

            // Optional contiguous hex pre-fill.
            std::string data_hex = item->data_hex;
            e->data_hex = data_hex;
            auto hexv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            for (uint64_t i = 0; i < e->size && i * 2 + 1 < data_hex.size(); i++)
            {
                e->data[i] = (hexv(data_hex[i*2]) << 4) | hexv(data_hex[i*2+1]);
            }

            // Writes draw a pool beat per issue (the target frees it);
            // reads keep a reusable own request (initiator-owned).
            e->req = e->is_write ? nullptr
                                 : new vp::IoReq(e->addr, e->data, e->size, false);
            this->schedule.push_back(e);
        }
    }
}


void StubMasterV2::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_to_schedule == 0)
    {
        int64_t first = this->schedule[0]->cycle;
        if (first <= 0) first = 1;
        this->issue_event.enqueue(first);
    }
}


StubMasterV2::ScheduleEntry *StubMasterV2::entry_from_req(vp::IoReq *req)
{
    // Correlate by req->initiator (set at issue), not object identity: a read
    // response arrives as DISTINCT per-beat objects (initiator-owned convention),
    // each carrying our entry as initiator; a write ack round-trips our own req
    // (whose initiator is also the entry). So req->initiator is the entry either way.
    return (ScheduleEntry *)req->initiator;
}


void StubMasterV2::log_done(const char *tag, ScheduleEntry *e)
{
    int64_t now = this->clock.get_cycles();
    char hex[17] = { 0 };
    int n = e->size < 8 ? (int)e->size : 8;
    if (e->is_write)
    {
        // Writes: log the data we sent (data buffer holds the source bytes).
        // Status/latency come from the entry's stash — the request object is
        // gone once granted (target-freed under the per-burst ack contract).
        for (int i = 0; i < n; i++)
            snprintf(&hex[i*2], 3, "%02x", e->data[i]);
        uint8_t cksum = 0;
        for (uint64_t i = 0; i < e->size; i++) cksum ^= e->data[i];
        printf("[%ld] %s %s name=%s status=%d latency=%lu data=%s checksum=%02x\n",
            now, this->logname.c_str(), tag, e->name.c_str(),
            (int)e->wr_status, (unsigned long)e->wr_latency,
            hex, cksum);
    }
    else
    {
        // Reads: log the accumulated checksum + first-beat preview.
        for (int i = 0; i < n; i++)
            snprintf(&hex[i*2], 3, "%02x", e->preview[i]);
        printf("[%ld] %s %s name=%s status=%d latency=%lu data=%s checksum=%02x\n",
            now, this->logname.c_str(), tag, e->name.c_str(),
            (int)e->req->get_resp_status(), (unsigned long)e->req->get_latency(),
            hex, e->checksum);
    }
}


void StubMasterV2::mark_resolved(ScheduleEntry *e)
{
    if (e->resolved) return;
    e->resolved = true;
    this->nb_resolved++;

    // Chain: if next entry is chained, fire chain_event +1 cycle.
    size_t next_idx = e->idx + 1;
    if (next_idx < this->schedule.size())
    {
        ScheduleEntry *next = this->schedule[next_idx];
        if (next->chain_to_prev && this->next_to_schedule == next_idx)
        {
            this->next_to_schedule++;
            this->pending_chain = next;
            this->chain_event.enqueue(1);
        }
    }

    if (this->nb_resolved == this->schedule.size() && !this->quit_event.is_enqueued())
    {
        this->quit_event.enqueue(this->quit_after_cycles);
    }
}


void StubMasterV2::release_if_idle()
{
    this->nb_inflight--;
    if (this->nb_inflight == 0)
    {
        this->sig_req_addr.release();
        this->sig_req_size.release();
        this->sig_req_is_write.release();
        this->sig_pending.release();
    }
    else
    {
        this->sig_pending = this->nb_inflight;
    }
}


void StubMasterV2::issue(ScheduleEntry *e)
{
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s SEND name=%s addr=0x%lx size=%lu write=%d "
           "is_first=%d is_last=%d burst_id=%ld\n",
        now, this->logname.c_str(), e->name.c_str(),
        e->addr, (unsigned long)e->size, e->is_write ? 1 : 0,
        e->is_first ? 1 : 0, e->is_last ? 1 : 0, e->burst_id);

    if (e->is_write)
    {
        // Per-burst ack contract: all beats of one burst carry the same
        // initiator — the burst's is_first entry — which records the
        // is_last entry so the ack can resolve it. Run the burst
        // bookkeeping only on the first issue of a beat: a DENIED beat is
        // re-issued with its pool object (and initiator) intact, and must
        // not reopen or re-close the burst.
        if (e->req == nullptr)
        {
            if (e->is_first || this->open_write_first == nullptr)
            {
                this->open_write_first = e;
            }
            if (e->is_last)
            {
                this->open_write_first->burst_last_entry = e;
            }
            e->req = this->wr_allocator->alloc();
            e->req->initiator = this->open_write_first;
            if (e->is_last)
            {
                this->open_write_first = nullptr;
            }
        }
        // prepare() preserves initiator; refresh the caller-managed fields.
        e->req->prepare();
        e->req->set_data(e->data);
    }
    else
    {
        e->req->prepare();
        e->req->data = e->data;
        e->req->initiator = e;
    }
    e->req->set_addr(e->addr);
    e->req->set_size(e->size);
    e->req->set_is_write(e->is_write);
    e->req->is_first = e->is_first;
    e->req->is_last  = e->is_last;
    e->req->burst_id = e->burst_id;
    // Reset the read-side accumulator for this issue.
    e->bytes_received = 0;
    e->checksum = 0;

    this->sig_req_addr     = e->addr;
    this->sig_req_size     = e->size;
    this->sig_req_is_write = e->is_write;
    this->nb_inflight++;
    this->sig_pending      = this->nb_inflight;

    bool wr = e->is_write;
    bool wr_last = e->is_last;
    vp::IoReqStatus st = this->out.req(e->req);
    switch (st)
    {
        case vp::IO_REQ_DONE:
            if (wr)
            {
                // Inline completion: burst ack on a last beat (or the
                // DONE+INVALID escape hatch). The master keeps the beat —
                // recycle it to the pool.
                e->wr_status = e->req->get_resp_status();
                e->wr_latency = e->req->get_latency();
                e->req->free();
                e->req = nullptr;
            }
            this->log_done("DONE", e);
            this->mark_resolved(e);
            this->release_if_idle();
            break;
        case vp::IO_REQ_GRANTED:
            printf("[%ld] %s GRANTED name=%s\n", now, this->logname.c_str(), e->name.c_str());
            if (wr)
            {
                // Ownership transferred: the target frees the beat. Non-last
                // beats get no response at all — they are complete from the
                // master's point of view once accepted; the burst's last
                // entry stays pending until the single ack.
                e->req = nullptr;
                if (!wr_last)
                {
                    this->mark_resolved(e);
                    this->release_if_idle();
                }
            }
            break;
        case vp::IO_REQ_DENIED:
            printf("[%ld] %s DENIED name=%s\n", now, this->logname.c_str(), e->name.c_str());
            this->denied_queue.push_back(e);
            break;
    }
}


void StubMasterV2::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV2 *_this = (StubMasterV2 *)__this;
    if (_this->next_to_schedule >= _this->schedule.size()) return;

    ScheduleEntry *e = _this->schedule[_this->next_to_schedule];
    if (e->chain_to_prev) return;       // mark_resolved will trigger this one

    _this->next_to_schedule++;
    _this->issue(e);

    if (_this->next_to_schedule < _this->schedule.size() &&
        !_this->schedule[_this->next_to_schedule]->chain_to_prev)
    {
        int64_t now = _this->clock.get_cycles();
        int64_t next_cycle = _this->schedule[_this->next_to_schedule]->cycle;
        int64_t delta = next_cycle - now;
        if (delta <= 0) delta = 1;
        _this->issue_event.enqueue(delta);
    }
}


void StubMasterV2::chain_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV2 *_this = (StubMasterV2 *)__this;
    if (_this->pending_chain)
    {
        ScheduleEntry *e = _this->pending_chain;
        _this->pending_chain = nullptr;
        _this->issue(e);
    }
}


vp::IoRespAck StubMasterV2::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMasterV2 *_this = (StubMasterV2 *)__this;
    ScheduleEntry *e = _this->entry_from_req(req);
    if (e == nullptr) return vp::IO_RESP_ACCEPTED;

    int64_t now = _this->clock.get_cycles();

    // Write burst ack: a single distinct data-less object whose initiator is
    // the burst's first entry; resolve the entry carrying is_last.
    if (req->get_opcode() == vp::WRITE)
    {
        ScheduleEntry *last = e->burst_last_entry ? e->burst_last_entry : e;
        if (!req->is_last || req->get_data() != nullptr)
        {
            printf("[%ld] %s ERROR malformed write burst ack (last=%d data=%p)\n",
                now, _this->logname.c_str(), req->is_last ? 1 : 0, req->get_data());
            abort();
        }
        last->wr_status = req->get_resp_status();
        last->wr_latency = req->get_latency();
        req->free();
        _this->log_done("RESP", last);
        _this->mark_resolved(last);
        _this->release_if_idle();
        return vp::IO_RESP_ACCEPTED;
    }

    // Accumulate this beat's data into the read-side checksum/preview.
    uint64_t beat_size = req->size;
    uint8_t *beat_data = req->data;
    if (!e->is_write)
    {
        if (req->is_first)
        {
            int n = beat_size < 8 ? (int)beat_size : 8;
            for (int i = 0; i < n; i++) e->preview[i] = beat_data[i];
        }
        for (uint64_t i = 0; i < beat_size; i++) e->checksum ^= beat_data[i];
        e->bytes_received += beat_size;
    }

    // Per-beat trace line so tests can verify per-beat cycle ordering.
    // addr is the per-beat address as set by the slave (dramsys_v2 sets
    // it to burst_addr + cumulative emitted bytes; other slaves may
    // leave it at the burst's start — both are valid per the v2
    // contract).
    printf("[%ld] %s BEAT name=%s addr=0x%lx size=%lu is_first=%d is_last=%d\n",
           now, _this->logname.c_str(), e->name.c_str(),
           req->addr, (unsigned long)beat_size,
           req->is_first ? 1 : 0, req->is_last ? 1 : 0);

    if (req->is_last)
    {
        _this->log_done("RESP", e);
        _this->mark_resolved(e);
        _this->release_if_idle();
    }
    // Free the distinct read-beat objects we consume (allocator-backed —
    // return them to their pool); never our own request object.
    if (req != e->req)
    {
        req->free();
    }
    return vp::IO_RESP_ACCEPTED;
}


void StubMasterV2::retry_handler(vp::Block *__this, vp::IoRetryChannel)
{
    StubMasterV2 *_this = (StubMasterV2 *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s RETRY queue=%zu\n", now, _this->logname.c_str(), _this->denied_queue.size());
    if (_this->denied_queue.empty()) return;
    ScheduleEntry *e = _this->denied_queue.front();
    _this->denied_queue.pop_front();
    // The previous issue() already incremented nb_inflight; undo so reissue
    // sees a balanced count.
    _this->nb_inflight--;
    if (_this->nb_inflight == 0) _this->sig_pending.release();
    else                          _this->sig_pending = _this->nb_inflight;
    _this->issue(e);
}


void StubMasterV2::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV2 *_this = (StubMasterV2 *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    _this->time.get_engine()->quit(0);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubMasterV2(config);
}
