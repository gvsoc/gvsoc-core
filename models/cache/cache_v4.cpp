// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Set-associative cache on the io_v2 protocol.
 *
 * Direct port of cache_v3 to the io_v2 IO interface. Functional scope and
 * replacement policy (pseudo-random LFSR) are unchanged; only the IO-side
 * plumbing differs:
 *
 *   - Single-master slave port; replies via `input_itf.resp(req)` on our own
 *     slave port (no v1 `resp_port` indirection).
 *   - Status codes: IO_REQ_DONE / IO_REQ_GRANTED / IO_REQ_DENIED, with error
 *     reporting via `req->set_resp_status(IO_RESP_INVALID) + IO_REQ_DONE`.
 *   - No arg stack: pending CPU requests are tracked by member fields and a
 *     simple queue, not by save()/restore() on the request.
 *   - Latency on synchronous replies is annotated via `req->inc_latency(n)`.
 *     For the async path the wall-clock of `resp()` is the timing signal —
 *     no extra annotation needed.
 *
 * Model-level behaviour otherwise matches cache_v3:
 *   - one refill in flight at a time (set_associative, line-granular)
 *   - CPU requests that miss during a pending refill are queued and replied to
 *     once their miss resolves; they are acknowledged upstream as GRANTED
 *   - disable (via the `enable` wire) bypasses the cache: the CPU request is
 *     forwarded verbatim through the refill port (address transformed by
 *     refill_shift / refill_offset first)
 *   - flush, flush-line, flush-ack wires are unchanged
 *
 * Several refills in flight (cfg.max_refills > 1), for a cache shared by
 * several masters whose misses overlap (a refill table, like the PULP shared
 * instruction cache):
 *   - a miss takes a free entry of the table, sends its refill and leaves the
 *     cache free for the other requests: hits are served while refills are in
 *     flight, and other misses start their own refill
 *   - a miss on a line which is already being refilled waits for that refill
 *     instead of sending another one
 *   - with cfg.refill_banks > 1 the table is split between banks of lines
 *     (line address modulo the number of banks), max_refills in each, as in
 *     a cache made of several banks with a refill table of their own
 *   - a miss finding the table full (or every way of its set being refilled,
 *     or the refill port denying) is queued; the queued requests are served
 *     one per cycle, oldest first, as refills complete
 *   - the way a refill goes to is invalidated when the refill starts, since
 *     requests are served while its data comes in
 * With max_refills = 1 the cache behaves as described above: everything waits
 * behind the refill in flight.
 */

#include <bit>
#include <vp/vp.hpp>
#include <vp/queue.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/signal.hpp>
#include <vector>
#include <deque>
#include <cache/cache_v4/cache_config.hpp>

static int ceil_log2(unsigned int n)
{
    if (n <= 1) return 0;
    return 32 - __builtin_clz(n - 1);
}

typedef struct
{
    uint32_t tag;
    bool dirty;
    uint8_t *data;
    vp::Trace tag_event;
    int64_t timestamp;
    // Line brought in by the sequential prefetcher and not yet demanded.
    // The first demand hit on it triggers the prefetch of the next line
    // (tagged prefetching, like the RTL snitch_icache prefetcher).
    bool prefetched;
} cache_line_t;

// One refill in flight (max_refills > 1): the request sent downstream, where
// its data goes, and the requests waiting for that line.
struct RefillSlot
{
    struct Waiter
    {
        vp::IoReq *req;
        unsigned int line_offset;
    };

    bool busy = false;
    vp::IoReq req;
    cache_line_t *line = nullptr;
    uint32_t tag = 0;
    std::vector<Waiter> waiters;
};

class Cache : public vp::Component
{
public:
    Cache(vp::ComponentConf &conf);

    void reset(bool active) override;

    CacheConfig cfg;

private:
    // Clock events
    static void refill_event_clear_handler(vp::Block *__this, vp::ClockEvent *event);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // Wire callbacks
    static void enable_sync(vp::Block *_this, bool active);
    static void flush_sync(vp::Block *_this, bool active);
    static void flush_line_sync(vp::Block *_this, bool active);
    static void flush_line_addr_sync(vp::Block *_this, uint32_t addr);

    // io_v2 callbacks
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck refill_resp(vp::Block *__this, vp::IoReq *req);
    static void refill_retry(vp::Block *__this, vp::IoRetryChannel);

    vp::IoReqStatus handle_req(vp::IoReq *req);
    void check_state();

    // Several refills in flight (cfg.max_refills > 1)
    vp::IoReqStatus handle_req_multi(vp::IoReq *req, bool queued, bool *blocked);
    vp::IoRespAck refill_resp_multi(RefillSlot *slot, vp::IoReq *req);
    void fsm_multi();
    void check_state_multi();
    RefillSlot *slot_of(vp::IoReq *req);
    bool line_under_refill(cache_line_t *line);
    int get_refill_way_multi(unsigned int line_index);
    bool bank_has_waiting(uint32_t tag);
    void update_pending_refill();

    cache_line_t *refill(int line_index, unsigned int addr, unsigned int tag,
                          vp::IoReq *req, bool *pending);
    void arm_prefetch(unsigned int addr);
    void try_prefetch();
    cache_line_t *get_line(vp::IoReq *req, unsigned int *line_index,
                            unsigned int *tag, unsigned int *line_offset);

    unsigned int step_lru();
    unsigned int get_refill_way(unsigned int line_index);
    void enable(bool e);
    void flush();
    void flush_line_op(unsigned int addr);

    // Derived geometry (computed in the ctor from cfg)
    unsigned int line_size_bits = 0;
    unsigned int nb_sets_bits = 0;
    unsigned int nb_sets = 0;

    bool enabled = false;

    vp::Trace trace;
    vp::Trace io_event;

    // io_v2 ports — method pointers are passed at construction.
    vp::IoSlave  input_itf{&Cache::input_req};
    vp::IoMaster refill_itf{&Cache::refill_retry, &Cache::refill_resp};

    // Side-band wire ports
    vp::WireSlave<bool>     enable_itf;
    vp::WireSlave<bool>     flush_itf;
    vp::WireMaster<bool>    flush_ack_itf;
    vp::WireSlave<bool>     flush_line_itf;
    vp::WireSlave<uint32_t> flush_line_addr_itf;

    // Internal refill vehicle (cache owns exactly one — only one refill in flight).
    vp::IoReq refill_req;
    // Sequential prefetcher state (active when cfg.prefetch). A demand miss
    // or the first hit on a prefetched line arms the next line; the prefetch
    // is issued through the refill port when it is otherwise idle, using its
    // own request vehicle so responses can be told apart.
    vp::IoReq prefetch_req;
    bool prefetch_wanted = false;
    unsigned int prefetch_addr = 0;
    // The in-flight refill is a prefetch (pending_refill is reused as the
    // busy flag so the demand path naturally queues behind it).
    bool pending_is_prefetch = false;

    // FIFO of CPU requests that were acknowledged upstream (GRANTED) but not yet
    // served. Two sources feed it:
    //  - reqs arriving while a refill is already in flight (they re-enter via
    //    fsm_handler once the refill resolves);
    //  - the CPU req whose miss triggered the current refill (placed at the head
    //    so the refill_resp path can pop it and reply to the master).
    vp::Queue refill_pending_reqs;

    // GUI / VCD signals (match cache_v3)
    vp::Signal<bool>     pending_refill;
    vp::Signal<uint64_t> refill_event;
    vp::Signal<uint64_t> req_event;

    vp::ClockEvent *fsm_event = nullptr;
    vp::ClockEvent  refill_event_clear_event;

    // Earliest cycle at which another synchronous refill can complete. Used to
    // fold a previous refill's in-flight window into subsequent synchronous
    // hits/misses so the CPU sees the serialised latency.
    int64_t refill_timestamp = -1;

    // Pseudo-random LFSR state for replacement policy
    uint8_t lru_out = 0;

    // Flush-line wire staging (address arrives via a separate wire)
    uint32_t flush_line_addr = 0;

    // Lines storage (nb_sets * nb_ways * cache_line_t).
    cache_line_t *lines = nullptr;

    // Refill state (only meaningful when pending_refill is set).
    cache_line_t *refill_line = nullptr;
    uint32_t      refill_tag = 0;
    unsigned int  pending_line_offset = 0;

    // Set if a refill was denied by the downstream and must be retried on the
    // next retry() signal. Used only while a queued request is being drained —
    // if we are denied on the inline path we propagate DENIED to the master.
    bool refill_retry_pending = false;
    // Set if we returned IO_REQ_DENIED to the upstream master; on the next retry
    // from our refill port we owe input_itf.retry() to un-stick it.
    bool input_needs_retry = false;

    // Refill table (cfg.max_refills > 1). `multi` selects this mode, in which
    // `waiting` holds, in order of arrival, the requests which found no room
    // for their refill; `drain_blocked` is set while none of them has any, so
    // that they are only looked at again once a refill completes.
    bool multi = false;
    int nb_slots = 0;
    RefillSlot *slots = nullptr;
    std::deque<vp::IoReq *> waiting;
    bool drain_blocked = false;
};


Cache::Cache(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      refill_pending_reqs(this, "refill_queue"),
      pending_refill(*this, "refill", 0),
      refill_event(*this, "refill_addr", 32),
      req_event(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
      refill_event_clear_event(this, &Cache::refill_event_clear_handler)
{
    // Derived geometry — CacheConfig carries bytes, we unpack into log2s.
    this->line_size_bits = ceil_log2(this->cfg.line_size);
    this->nb_sets = this->cfg.size / this->cfg.ways / this->cfg.line_size;
    this->nb_sets_bits = ceil_log2(this->nb_sets);

    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("port", &this->io_event, 32);

    // io_v2 slave/master ports (methods bound in-class above).
    this->new_slave_port("input", &this->input_itf);
    this->new_master_port("refill", &this->refill_itf);

    // Side-band wires
    this->enable_itf.set_sync_meth(&Cache::enable_sync);
    this->new_slave_port("enable", &this->enable_itf);

    this->flush_itf.set_sync_meth(&Cache::flush_sync);
    this->new_slave_port("flush", &this->flush_itf);

    this->flush_line_itf.set_sync_meth(&Cache::flush_line_sync);
    this->new_slave_port("flush_line", &this->flush_line_itf);

    this->flush_line_addr_itf.set_sync_meth(&Cache::flush_line_addr_sync);
    this->new_slave_port("flush_line_addr", &this->flush_line_addr_itf);

    this->new_master_port("flush_ack", &this->flush_ack_itf);

    this->lines = new cache_line_t[this->nb_sets * this->cfg.ways];
    for (unsigned int i = 0; i < this->nb_sets; i++)
    {
        for (unsigned int j = 0; j < this->cfg.ways; j++)
        {
            cache_line_t *line = &this->lines[i * this->cfg.ways + j];
            line->timestamp = -1;
            line->tag = -1;
            line->prefetched = false;
            line->data = new uint8_t[this->cfg.line_size];
            this->traces.new_trace_event(
                "set_" + std::to_string(j) + "/line_" + std::to_string(i),
                &line->tag_event, 32);
        }
    }

    this->fsm_event = this->event_new(&Cache::fsm_handler);

    this->multi = this->cfg.max_refills > 1;
    if (this->multi)
    {
        if (this->cfg.prefetch)
        {
            this->trace.fatal("The prefetcher is not supported with max_refills > 1\n");
        }
        this->nb_slots = this->cfg.max_refills * this->cfg.refill_banks;
        this->slots = new RefillSlot[this->nb_slots];
    }

    this->trace.msg(vp::Trace::LEVEL_INFO,
        "Instantiating cache (sets: %d, ways: %d, line_size: %d)\n",
        this->nb_sets, this->cfg.ways, this->cfg.line_size);
}


void Cache::reset(bool active)
{
    if (active)
    {
        this->flush();
        this->enabled = this->cfg.enabled;
        this->refill_event.release();
        this->refill_retry_pending = false;
        this->input_needs_retry = false;
        this->refill_timestamp = -1;
        this->prefetch_wanted = false;
        this->pending_is_prefetch = false;
        this->drain_blocked = false;
        this->waiting.clear();
        for (int i = 0; i < this->nb_slots; i++)
        {
            this->slots[i].busy = false;
            this->slots[i].waiters.clear();
        }
    }
}


// ---------------------------------------------------------------------------
// Refill path (master-side response / retry)
// ---------------------------------------------------------------------------

vp::IoRespAck Cache::refill_resp(vp::Block *__this, vp::IoReq *req)
{
    Cache *_this = (Cache *)__this;

    if (_this->multi)
    {
        RefillSlot *slot = _this->slot_of(req);
        if (slot == nullptr)
        {
            // Bypass path, see below.
            _this->input_itf.resp(req);
            return vp::IO_RESP_ACCEPTED;
        }
        return _this->refill_resp_multi(slot, req);
    }

    // Asynchronous completion of a prefetch: tag the line and free the refill
    // port; queued demand requests (if any) drain through check_state. No CPU
    // request is attached to a prefetch.
    if (req == &_this->prefetch_req)
    {
        if (!req->is_last)
        {
            return vp::IO_RESP_ACCEPTED;
        }
        cache_line_t *line = _this->refill_line;
        line->tag = _this->refill_tag;
        line->prefetched = true;
        _this->pending_refill.set(0);
        _this->pending_is_prefetch = false;
        _this->refill_event.release();
        _this->check_state();
        return vp::IO_RESP_ACCEPTED;
    }

    // Bypass path: the cache is disabled and we simply pass upstream requests
    // through. The request we receive here is the CPU's own request (not our
    // refill_req), so we forward the response to the CPU on our own slave port.
    if (req != &_this->refill_req)
    {
        _this->input_itf.resp(req);
        return vp::IO_RESP_ACCEPTED;
    }

    // Cached refill path. A beat-streaming downstream (KIND_BEAT router) may
    // emit one resp() per beat for a single refill request. The cache-line
    // buffer is filled in place via slice pointers, so the data is already
    // complete by the time the final beat fires; we only run the completion
    // logic on the burst's last beat. Sync DONE and async big-packet responses
    // produce a single resp() with is_last=true, so this is a no-op for them.
    if (!req->is_last)
    {
        return vp::IO_RESP_ACCEPTED;
    }

    // Cached-refill path. The CPU request whose miss triggered this refill is at
    // the head of refill_pending_reqs (placed there by Cache::refill()).
    vp_assert(!_this->refill_pending_reqs.empty(), &_this->trace,
        "Received refill response with no pending CPU request\n");

    vp::IoReq *cpu_req = (vp::IoReq *)_this->refill_pending_reqs.pop();
    uint8_t *data = cpu_req->get_data();
    uint64_t size = cpu_req->get_size();
    bool is_write = cpu_req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Received refill response (cpu_req: %p, is_write: %d, data: %p, size: 0x%lx)\n",
        cpu_req, is_write, data, size);

    _this->pending_refill.set(0);
    _this->refill_event.release();

    if (data)
    {
        cache_line_t *line = _this->refill_line;
        line->tag = _this->refill_tag;

        if (!is_write)
        {
            memcpy(data, &line->data[_this->pending_line_offset], size);
        }
        else
        {
            memcpy(&line->data[_this->pending_line_offset], data, size);
        }
    }

    _this->input_itf.resp(cpu_req);
    _this->check_state();

    return vp::IO_RESP_ACCEPTED;
}


void Cache::refill_retry(vp::Block *__this, vp::IoRetryChannel)
{
    Cache *_this = (Cache *)__this;

    // The downstream is now ready. Two independent things may be waiting on this:
    //
    //  (a) We previously returned DENIED to the upstream because a new CPU request
    //      missed and the refill target refused it. The master is waiting for a
    //      retry() on the input slave port.
    //
    //  (b) A queued CPU request was being drained, hit a miss, and the refill was
    //      denied inside fsm_handler. The CPU request stayed in the queue and we
    //      now need to nudge fsm_handler to try again.
    //
    // Both flags are one-shot and cleared here.
    _this->refill_retry_pending = false;

    if (_this->input_needs_retry)
    {
        _this->input_needs_retry = false;
        _this->input_itf.retry();
    }

    _this->check_state();
}


// ---------------------------------------------------------------------------
// Internal queue drainer
// ---------------------------------------------------------------------------

// Kicked by check_state() whenever there is at least one queued CPU request and
// no refill currently in flight. Pulls one request from the queue, runs it
// through handle_req, and replies to the CPU if it resolves synchronously.
void Cache::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Cache *_this = (Cache *)__this;

    if (_this->multi)
    {
        _this->fsm_multi();
        return;
    }

    if (!_this->pending_refill.get() && !_this->refill_retry_pending
        && !_this->refill_pending_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)_this->refill_pending_reqs.pop();

        _this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Resuming req (req: %p, is_write: %d, offset: 0x%lx, size: 0x%lx)\n",
            req, req->get_is_write(), req->get_addr(), req->get_size());

        vp::IoReqStatus st = _this->handle_req(req);
        if (st == vp::IO_REQ_DONE)
        {
            _this->input_itf.resp(req);
        }
        else if (st == vp::IO_REQ_DENIED)
        {
            // Refill was refused. Put the req back at the head so we retry it
            // once refill_retry() clears refill_retry_pending.
            _this->refill_pending_reqs.push_front(req);
        }
        // If GRANTED, Cache::refill() has already pushed cpu_req back at the
        // head of refill_pending_reqs — refill_resp will pop and reply.
    }

    _this->check_state();
}


void Cache::check_state()
{
    if (this->multi)
    {
        this->check_state_multi();
        return;
    }

    // Use has_reqs() (presence), NOT !empty() (readiness): a CPU request queued
    // via push_back this same cycle carries a now+1 timestamp, so empty() reports
    // it as "not yet available". If a refill completes the same cycle the request
    // is queued, an !empty() test would skip scheduling the drain fsm, and nothing
    // re-checks next cycle -> the queued request is lost and the master hangs. The
    // fsm runs at +1, by which point the element is ready, so scheduling on
    // presence is correct.
    if (!this->pending_refill.get() && !this->refill_retry_pending
        && this->refill_pending_reqs.has_reqs())
    {
        if (!this->fsm_event->is_enqueued())
        {
            this->event_enqueue(this->fsm_event, 1);
        }
    }

    // The refill port is idle and no demand request is waiting: give the
    // armed prefetch a chance.
    if (!this->pending_refill.get() && !this->refill_retry_pending
        && !this->refill_pending_reqs.has_reqs())
    {
        this->try_prefetch();
    }
}


void Cache::arm_prefetch(unsigned int addr)
{
    if (!this->cfg.prefetch)
    {
        return;
    }
    this->prefetch_wanted = true;
    this->prefetch_addr = (addr & ~((1U << this->line_size_bits) - 1))
        + (1U << this->line_size_bits);
}


void Cache::try_prefetch()
{
    if (!this->prefetch_wanted || !this->enabled)
    {
        return;
    }

    unsigned int addr = this->prefetch_addr;
    unsigned int tag = addr >> this->line_size_bits;
    unsigned int line_index = tag & (this->nb_sets - 1);

    this->prefetch_wanted = false;

    // Already cached: nothing to do.
    for (unsigned int i = 0; i < this->cfg.ways; i++)
    {
        if (this->lines[line_index * this->cfg.ways + i].tag == tag)
        {
            return;
        }
    }

    unsigned int way = this->get_refill_way(line_index);
    cache_line_t *line = &this->lines[line_index * this->cfg.ways + way];

    uint32_t full_addr = ((addr & ~((1U << this->line_size_bits) - 1))
                          << this->cfg.refill_shift) + this->cfg.refill_offset;

    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Prefetching line (addr: 0x%x, index: %d, way: %d)\n",
        full_addr, line_index, way);

    line->tag_event.event((uint8_t *)&full_addr);

    vp::IoReq *r = &this->prefetch_req;
    r->prepare();
    r->is_first = true;
    r->is_last = true;
    r->burst_id = -1;
    r->set_addr(full_addr);
    r->set_is_write(false);
    r->set_size(1U << this->line_size_bits);
    r->set_data(line->data);

    vp::IoReqStatus st = this->refill_itf.req(r);

    if (st == vp::IO_REQ_GRANTED)
    {
        this->refill_line = line;
        this->refill_tag = tag;
        this->pending_refill.set(1);
        this->pending_is_prefetch = true;
        return;
    }

    if (st == vp::IO_REQ_DENIED)
    {
        // Best effort: drop the prefetch. The downstream will emit a
        // spurious retry() which refill_retry tolerates.
        return;
    }

    // Synchronous completion: the line lands after the accumulated refill
    // window; demand hits arriving before that pay the remaining time
    // (this is exactly how the prefetch hides the refill latency), and a
    // demand miss serialises behind the port occupancy.
    int64_t now = this->clock.get_cycles();
    int64_t latency = 0;
    if (now < this->refill_timestamp)
    {
        latency += this->refill_timestamp - now;
    }
    latency += r->get_full_latency() + this->cfg.refill_latency;

    this->refill_timestamp = now + latency;
    line->tag = tag;
    line->prefetched = true;
    line->timestamp = now + latency;
}


// ---------------------------------------------------------------------------
// Core cache logic (mirrors cache_v3, minus debug/atomics)
// ---------------------------------------------------------------------------

cache_line_t *Cache::refill(int line_index, unsigned int addr, unsigned int tag,
                              vp::IoReq *cpu_req, bool *pending)
{
    // Cache supports only one refill at a time. Queue the CPU req and back off.
    if (this->pending_refill.get())
    {
        this->refill_pending_reqs.push_back(cpu_req);
        *pending = true;
        return nullptr;
    }

    unsigned int refill_way = this->get_refill_way(line_index);
    cache_line_t *line = &this->lines[line_index * this->cfg.ways + refill_way];

    uint32_t full_addr = ((addr & ~((1U << this->line_size_bits) - 1))
                          << this->cfg.refill_shift) + this->cfg.refill_offset;

    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Refilling line (addr: 0x%x, index: %d, way: %d)\n",
        full_addr, line_index, refill_way);

    line->tag_event.event((uint8_t *)&full_addr);

    vp::IoReq *r = &this->refill_req;
    r->prepare();
    // A refill is a single whole-line burst. Reset the burst flags explicitly:
    // prepare() does not touch them, and a beat-streaming downstream (KIND_BEAT
    // router / IoV2BeatAdapter) leaves is_first=0/is_last=1 on the shared
    // refill_req after the previous response's last beat. Reusing it without a
    // reset would send the next refill as a stray continuation beat.
    r->is_first = true;
    r->is_last = true;
    r->burst_id = -1;
    r->set_addr(full_addr);
    r->set_is_write(false);
    r->set_size(1U << this->line_size_bits);
    r->set_data(line->data);

    this->refill_event_clear_event.cancel();

    vp::IoReqStatus st = this->refill_itf.req(r);

    if (st == vp::IO_REQ_GRANTED)
    {
        // The refill will be completed asynchronously. Park the CPU request at the
        // head of the queue so refill_resp can pop it and reply to the master.
        this->refill_pending_reqs.push_front(cpu_req);
        this->refill_line = line;
        this->refill_tag = tag;
        this->pending_refill.set(1);
        *pending = true;
        return nullptr;
    }

    if (st == vp::IO_REQ_DENIED)
    {
        // The refill was refused. Caller decides whether to propagate DENIED
        // upstream (new inline req) or to keep the CPU req queued (drain path).
        this->refill_retry_pending = true;
        *pending = false;
        return nullptr;
    }

    // Synchronous success. Tag the line, account for serialisation with any
    // previously-started synchronous refill, and annotate the CPU request's
    // latency so the master paces itself correctly.
    line->tag = tag;

    int64_t now = this->clock.get_cycles();
    int64_t latency = 0;
    if (now < this->refill_timestamp)
    {
        latency += this->refill_timestamp - now;
    }
    // get_full_latency() so a bandwidth router on the refill path contributes
    // its (max-combined) transfer time, not just the head latency.
    latency += r->get_full_latency() + this->cfg.refill_latency;

    this->refill_timestamp = now + latency;
    this->refill_event_clear_event.enqueue(latency);

    cpu_req->inc_latency(latency);

    line->timestamp = now + latency;

    return line;
}


void Cache::flush_line_op(unsigned int addr)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Flushing cache line (addr: 0x%x)\n", addr);
    unsigned int tag = addr >> this->line_size_bits;
    unsigned int line_index = tag & (this->nb_sets - 1);
    for (unsigned int i = 0; i < this->cfg.ways; i++)
    {
        cache_line_t *line = &this->lines[line_index * this->cfg.ways + i];
        if (line->tag == tag)
            line->tag = -1;
    }
}


void Cache::flush()
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Flushing whole cache\n");
    for (unsigned int i = 0; i < this->nb_sets; i++)
    {
        for (unsigned int j = 0; j < this->cfg.ways; j++)
        {
            this->lines[i * this->cfg.ways + j].tag = -1;
        }
    }

    if (this->flush_ack_itf.is_bound())
    {
        this->flush_ack_itf.sync(true);
    }
}


void Cache::enable(bool e)
{
    this->enabled = e;
    this->trace.msg(vp::Trace::LEVEL_INFO, "%s cache\n",
        e ? "Enabling" : "Disabling");
}


cache_line_t *Cache::get_line(vp::IoReq *req, unsigned int *line_index,
                                unsigned int *tag, unsigned int *line_offset)
{
    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    unsigned int line_size = 1U << this->line_size_bits;

    *tag = offset >> this->line_size_bits;
    *line_index = *tag & (this->nb_sets - 1);
    *line_offset = offset & (line_size - 1);

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Cache access (is_write: %d, addr: 0x%lx, size: 0x%lx, tag: 0x%x, "
        "index: %d, line_offset: 0x%x)\n",
        is_write, offset, size, *tag, *line_index, *line_offset);

    cache_line_t *line = &this->lines[*line_index * this->cfg.ways];
    for (unsigned int i = 0; i < this->cfg.ways; i++)
    {
        if (line->tag == *tag)
        {
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Cache hit (way: %d)\n", i);
            return line;
        }
        line++;
    }
    return nullptr;
}


vp::IoReqStatus Cache::handle_req(vp::IoReq *req)
{
    unsigned int line_index;
    unsigned int tag;
    unsigned int line_offset;
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool is_write = req->get_is_write();

    cache_line_t *hit_line = this->get_line(req, &line_index, &tag, &line_offset);

    if (hit_line == nullptr)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Cache miss\n");
        uint64_t offset = req->get_addr();
        this->refill_event.set(offset);
        this->arm_prefetch(offset);
        bool pending = false;
        hit_line = this->refill(line_index, offset, tag, req, &pending);
        if (hit_line == nullptr)
        {
            if (pending)
            {
                this->pending_line_offset = line_offset;
                return vp::IO_REQ_GRANTED;
            }
            // Refill denied OR true error. The caller (input_req / fsm_handler)
            // decides how to map this to an upstream status.
            return vp::IO_REQ_DENIED;
        }
    }
    else
    {
        // Cache hit. If the line is still being refilled (synchronous case from an
        // earlier miss in this cycle), defer the timing of this access until the
        // refill would have landed.
        int64_t now = this->clock.get_cycles();
        if (now < hit_line->timestamp)
        {
            req->inc_latency(hit_line->timestamp - now);
        }
        // First demand hit on a prefetched line: keep the sequential stream
        // going by arming the next line (tagged prefetching).
        if (hit_line->prefetched)
        {
            hit_line->prefetched = false;
            this->arm_prefetch(req->get_addr());
        }
    }

    if (data)
    {
        if (!is_write)
        {
            memcpy(data, &hit_line->data[line_offset], size);
        }
        else
        {
            memcpy(&hit_line->data[line_offset], data, size);
        }
    }

    return vp::IO_REQ_DONE;
}


vp::IoReqStatus Cache::input_req(vp::Block *__this, vp::IoReq *req)
{
    Cache *_this = (Cache *)__this;

    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Received req (req: %p, is_write: %d, addr: 0x%lx, size: 0x%lx)\n",
        req, is_write, offset, size);

    _this->req_event.set_and_release(offset);

    // Bypass: forward the CPU request verbatim through the refill port after
    // address transformation. The response comes back on our refill_resp
    // callback, which recognises a non-&refill_req req as a bypass forward
    // and replies to the master on our own slave port.
    if (!_this->enabled)
    {
        req->set_addr((offset << _this->cfg.refill_shift) + _this->cfg.refill_offset);
        vp::IoReqStatus st = _this->refill_itf.req(req);
        if (st == vp::IO_REQ_DENIED)
        {
            // Undo the address rewrite so the master can retry cleanly.
            req->set_addr(offset);
            _this->input_needs_retry = true;
        }
        return st;
    }

    _this->io_event.event((uint8_t *)&offset);

    if (_this->multi)
    {
        // Hits are served and refills are started whatever is in flight. A
        // request which finds no room for its refill is queued and granted:
        // the queued requests are served as refills complete.
        bool blocked = false;
        vp::IoReqStatus st = _this->handle_req_multi(req, false, &blocked);
        if (blocked || st == vp::IO_REQ_DENIED)
        {
            _this->waiting.push_back(req);
            _this->check_state();
            return vp::IO_REQ_GRANTED;
        }
        return st;
    }

    // Cached path. If a refill is pending we must not start another one: queue
    // this request and ack upstream with GRANTED. When the current refill
    // resolves, fsm_handler will re-enter handle_req for this request.
    if (_this->pending_refill.get() || _this->refill_retry_pending)
    {
        _this->refill_pending_reqs.push_back(req);
        _this->check_state();
        return vp::IO_REQ_GRANTED;
    }

    vp::IoReqStatus st = _this->handle_req(req);
    if (st == vp::IO_REQ_DENIED)
    {
        // Refill was refused by the downstream and this request was new (not yet
        // acked to the master). Propagate DENIED and remember that we owe an
        // input.retry() once the refill port wakes up.
        _this->input_needs_retry = true;
    }
    else if (!_this->pending_refill.get() && !_this->refill_retry_pending)
    {
        _this->try_prefetch();
    }
    return st;
}


// ---------------------------------------------------------------------------
// Several refills in flight (cfg.max_refills > 1)
// ---------------------------------------------------------------------------

RefillSlot *Cache::slot_of(vp::IoReq *req)
{
    for (int i = 0; i < this->nb_slots; i++)
    {
        if (req == &this->slots[i].req)
        {
            return &this->slots[i];
        }
    }
    return nullptr;
}


bool Cache::line_under_refill(cache_line_t *line)
{
    for (int i = 0; i < this->nb_slots; i++)
    {
        if (this->slots[i].busy && this->slots[i].line == line)
        {
            return true;
        }
    }
    return false;
}


void Cache::update_pending_refill()
{
    bool busy = false;
    for (int i = 0; i < this->nb_slots; i++)
    {
        busy |= this->slots[i].busy;
    }
    this->pending_refill.set(busy);
}


// Way to refill in a set, never one which is being refilled. Returns -1 if
// every way of the set is.
int Cache::get_refill_way_multi(unsigned int line_index)
{
    cache_line_t *set = &this->lines[line_index * this->cfg.ways];

    if (this->cfg.refill_free_way_first)
    {
        for (unsigned int i = 0; i < this->cfg.ways; i++)
        {
            if (set[i].tag == (uint32_t)-1 && !this->line_under_refill(&set[i]))
            {
                return i;
            }
        }
    }

    bool any = false;
    for (unsigned int i = 0; i < this->cfg.ways; i++)
    {
        any |= !this->line_under_refill(&set[i]);
    }
    if (!any)
    {
        return -1;
    }

    unsigned int way = this->step_lru() % this->cfg.ways;
    while (this->line_under_refill(&set[way]))
    {
        way = (way + 1) % this->cfg.ways;
    }
    return way;
}


// A request is waiting for a refill entry of the bank this line belongs to.
bool Cache::bank_has_waiting(uint32_t tag)
{
    uint32_t bank_mask = this->cfg.refill_banks - 1;
    for (vp::IoReq *req : this->waiting)
    {
        uint32_t req_tag = req->get_addr() >> this->line_size_bits;
        if ((req_tag & bank_mask) == (tag & bank_mask))
        {
            return true;
        }
    }
    return false;
}


// Serve a request: a hit inline, a miss by joining the refill of its line or
// by starting one. `queued` tells the request comes from the queue (it then
// keeps its turn). `*blocked` is set if nothing could be done for it now: no
// free refill entry in its bank, no way to refill, or requests already
// waiting for an entry of that bank.
vp::IoReqStatus Cache::handle_req_multi(vp::IoReq *req, bool queued, bool *blocked)
{
    unsigned int line_index;
    unsigned int tag;
    unsigned int line_offset;
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool is_write = req->get_is_write();
    int64_t now = this->clock.get_cycles();

    *blocked = false;

    cache_line_t *line = this->get_line(req, &line_index, &tag, &line_offset);

    if (line != nullptr)
    {
        // Hit. A line which has just been refilled synchronously is only
        // there once its refill has landed.
        if (now < line->timestamp)
        {
            req->inc_latency(line->timestamp - now);
        }
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Cache miss\n");
        uint64_t offset = req->get_addr();
        this->refill_event.set(offset);

        // The line is already on its way: wait for it.
        for (int i = 0; i < this->nb_slots; i++)
        {
            RefillSlot *slot = &this->slots[i];
            if (slot->busy && slot->tag == tag)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG,
                    "Line already being refilled, waiting for it\n");
                slot->waiters.push_back({req, line_offset});
                return vp::IO_REQ_GRANTED;
            }
        }

        // A new refill: behind the requests already waiting for one in the
        // same bank.
        if (this->refill_retry_pending ||
            (!queued && this->bank_has_waiting(tag)))
        {
            *blocked = true;
            return vp::IO_REQ_GRANTED;
        }

        // A free entry, with room left in the bank of the line: lines are
        // spread over the banks by their low address bits, and each bank has
        // its own share of the table.
        uint32_t bank_mask = this->cfg.refill_banks - 1;
        int in_bank = 0;
        RefillSlot *slot = nullptr;
        for (int i = 0; i < this->nb_slots; i++)
        {
            if (!this->slots[i].busy)
            {
                if (slot == nullptr) slot = &this->slots[i];
            }
            else if ((this->slots[i].tag & bank_mask) == (tag & bank_mask))
            {
                in_bank++;
            }
        }
        if (in_bank >= (int)this->cfg.max_refills)
        {
            slot = nullptr;
        }
        int way = slot ? this->get_refill_way_multi(line_index) : -1;
        if (way == -1)
        {
            *blocked = true;
            return vp::IO_REQ_GRANTED;
        }

        line = &this->lines[line_index * this->cfg.ways + way];

        uint32_t full_addr = ((offset & ~((1U << this->line_size_bits) - 1))
                              << this->cfg.refill_shift) + this->cfg.refill_offset;

        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Refilling line (addr: 0x%x, index: %d, way: %d)\n",
            full_addr, line_index, way);

        line->tag_event.event((uint8_t *)&full_addr);

        vp::IoReq *r = &slot->req;
        r->prepare();
        r->is_first = true;
        r->is_last = true;
        r->burst_id = -1;
        r->set_addr(full_addr);
        r->set_is_write(false);
        r->set_size(1U << this->line_size_bits);
        r->set_data(line->data);

        this->refill_event_clear_event.cancel();

        vp::IoReqStatus st = this->refill_itf.req(r);

        if (st == vp::IO_REQ_GRANTED)
        {
            // The data comes in while other requests are served: the line it
            // replaces is gone from now on.
            line->tag = -1;
            slot->busy = true;
            slot->line = line;
            slot->tag = tag;
            slot->waiters.clear();
            slot->waiters.push_back({req, line_offset});
            this->pending_refill.set(1);
            return vp::IO_REQ_GRANTED;
        }

        if (st == vp::IO_REQ_DENIED)
        {
            this->refill_retry_pending = true;
            return vp::IO_REQ_DENIED;
        }

        // Synchronous refill: the line is there after its latency.
        int64_t latency = r->get_full_latency() + this->cfg.refill_latency;
        line->tag = tag;
        line->timestamp = now + latency;
        req->inc_latency(latency);
        this->refill_event_clear_event.enqueue(latency);
    }

    if (data)
    {
        if (!is_write)
        {
            memcpy(data, &line->data[line_offset], size);
        }
        else
        {
            memcpy(&line->data[line_offset], data, size);
        }
    }

    return vp::IO_REQ_DONE;
}


vp::IoRespAck Cache::refill_resp_multi(RefillSlot *slot, vp::IoReq *req)
{
    // A beat-streaming downstream answers one beat at a time; the line is
    // filled in place and complete on the last one.
    if (!req->is_last)
    {
        return vp::IO_RESP_ACCEPTED;
    }

    cache_line_t *line = slot->line;
    line->tag = slot->tag;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Received refill response (tag: 0x%x, waiters: %d)\n",
        slot->tag, (int)slot->waiters.size());

    // Free the entry before answering: a master may send its next request
    // from inside resp().
    std::vector<RefillSlot::Waiter> waiters;
    waiters.swap(slot->waiters);
    slot->busy = false;
    this->drain_blocked = false;
    this->update_pending_refill();
    this->refill_event.release();

    for (RefillSlot::Waiter &waiter : waiters)
    {
        uint8_t *data = waiter.req->get_data();
        if (data)
        {
            if (!waiter.req->get_is_write())
            {
                memcpy(data, &line->data[waiter.line_offset], waiter.req->get_size());
            }
            else
            {
                memcpy(&line->data[waiter.line_offset], data, waiter.req->get_size());
            }
        }
        this->input_itf.resp(waiter.req);
    }

    this->check_state();

    return vp::IO_RESP_ACCEPTED;
}


// Drainer of the waiting requests: one per cycle, the oldest one which can be
// served (a bank which is full does not hold the requests of another one).
void Cache::fsm_multi()
{
    if (!this->refill_retry_pending && !this->waiting.empty())
    {
        bool served = false;

        for (auto it = this->waiting.begin(); it != this->waiting.end(); ++it)
        {
            vp::IoReq *req = *it;
            bool blocked = false;
            vp::IoReqStatus st = this->handle_req_multi(req, true, &blocked);
            if (blocked)
            {
                continue;
            }

            served = true;
            if (st == vp::IO_REQ_DENIED)
            {
                // Stays where it is, sent again once the refill port retries.
                break;
            }

            this->trace.msg(vp::Trace::LEVEL_TRACE,
                "Resumed req (req: %p, is_write: %d, offset: 0x%lx, size: 0x%lx)\n",
                req, req->get_is_write(), req->get_addr(), req->get_size());

            // Out of the list before answering: a master may send its next
            // request from inside resp().
            this->waiting.erase(it);
            if (st == vp::IO_REQ_DONE)
            {
                this->input_itf.resp(req);
            }
            break;
        }

        if (!served)
        {
            this->drain_blocked = true;
        }
    }

    this->check_state();
}


void Cache::check_state_multi()
{
    if (!this->refill_retry_pending && !this->drain_blocked
        && !this->waiting.empty())
    {
        if (!this->fsm_event->is_enqueued())
        {
            this->event_enqueue(this->fsm_event, 1);
        }
    }
}


// ---------------------------------------------------------------------------
// Pseudo-random LRU (8-bit LFSR, matches cache_v3)
// ---------------------------------------------------------------------------

// Way to refill in a set. With refill_free_way_first, the first free one if
// there is one, like the PULP caches, which only draw a random victim once
// all the ways of the set are valid; the LFSR is then only stepped in that
// case, as in the hardware.
unsigned int Cache::get_refill_way(unsigned int line_index)
{
    if (this->cfg.refill_free_way_first)
    {
        for (unsigned int i = 0; i < this->cfg.ways; i++)
        {
            cache_line_t *line = &this->lines[line_index * this->cfg.ways + i];
            // A line being refilled asynchronously is only tagged when its
            // data is back: it is not free.
            if (line->tag == (uint32_t)-1 &&
                !(this->pending_refill.get() && line == this->refill_line))
            {
                return i;
            }
        }
    }
    return this->step_lru() % this->cfg.ways;
}

unsigned int Cache::step_lru()
{
    int feedback = !(((this->lru_out >> 7) & 1)
                  ^ ((this->lru_out >> 3) & 1)
                  ^ ((this->lru_out >> 2) & 1)
                  ^ ((this->lru_out >> 1) & 1));
    this->lru_out = (this->lru_out << 1) | (feedback & 1);
    return (this->lru_out >> 1) & (this->cfg.ways - 1);
}


// ---------------------------------------------------------------------------
// Wire-side plumbing (unchanged)
// ---------------------------------------------------------------------------

void Cache::enable_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    _this->enable(active);
}

void Cache::flush_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    if (active) _this->flush();
}

void Cache::flush_line_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    if (active) _this->flush_line_op(_this->flush_line_addr);
}

void Cache::flush_line_addr_sync(vp::Block *__this, uint32_t addr)
{
    Cache *_this = (Cache *)__this;
    _this->flush_line_addr = addr;
}

void Cache::refill_event_clear_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Cache *_this = (Cache *)__this;
    _this->refill_event.release();
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Cache(config);
}
