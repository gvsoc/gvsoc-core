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
 */

#include <bit>
#include <vp/vp.hpp>
#include <vp/queue.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/signal.hpp>
#include <vector>
#include <cache/cache_config.hpp>

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
} cache_line_t;

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
    static void refill_resp(vp::Block *__this, vp::IoReq *req);
    static void refill_retry(vp::Block *__this);

    vp::IoReqStatus handle_req(vp::IoReq *req);
    void check_state();

    cache_line_t *refill(int line_index, unsigned int addr, unsigned int tag,
                          vp::IoReq *req, bool *pending);
    cache_line_t *get_line(vp::IoReq *req, unsigned int *line_index,
                            unsigned int *tag, unsigned int *line_offset);

    unsigned int step_lru();
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
            line->data = new uint8_t[this->cfg.line_size];
            this->traces.new_trace_event(
                "set_" + std::to_string(j) + "/line_" + std::to_string(i),
                &line->tag_event, 32);
        }
    }

    this->fsm_event = this->event_new(&Cache::fsm_handler);

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
    }
}


// ---------------------------------------------------------------------------
// Refill path (master-side response / retry)
// ---------------------------------------------------------------------------

void Cache::refill_resp(vp::Block *__this, vp::IoReq *req)
{
    Cache *_this = (Cache *)__this;

    // Bypass path: the cache is disabled and we simply pass upstream requests
    // through. The request we receive here is the CPU's own request (not our
    // refill_req), so we forward the response to the CPU on our own slave port.
    if (req != &_this->refill_req)
    {
        _this->input_itf.resp(req);
        return;
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
}


void Cache::refill_retry(vp::Block *__this)
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
    if (!this->pending_refill.get() && !this->refill_retry_pending
        && !this->refill_pending_reqs.empty())
    {
        if (!this->fsm_event->is_enqueued())
        {
            this->event_enqueue(this->fsm_event, 1);
        }
    }
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

    unsigned int refill_way = this->step_lru() % this->cfg.ways;
    cache_line_t *line = &this->lines[line_index * this->cfg.ways + refill_way];

    uint32_t full_addr = ((addr & ~((1U << this->line_size_bits) - 1))
                          << this->cfg.refill_shift) + this->cfg.refill_offset;

    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Refilling line (addr: 0x%x, index: %d, way: %d)\n",
        full_addr, line_index, refill_way);

    line->tag_event.event((uint8_t *)&full_addr);

    vp::IoReq *r = &this->refill_req;
    r->prepare();
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
    latency += r->get_latency() + this->cfg.refill_latency;

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
    return st;
}


// ---------------------------------------------------------------------------
// Pseudo-random LRU (8-bit LFSR, matches cache_v3)
// ---------------------------------------------------------------------------

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
