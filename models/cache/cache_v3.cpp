// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <bit>
#include <vp/vp.hpp>
#include <vp/queue.hpp>
#include <vp/itf/io.hpp>
#include <vp/signal.hpp>
#include <vector>

int ceil_log2(unsigned int n)
{
    if (n <= 1) return 0;
    return std::bit_width(n - 1);
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

    friend class CacheSet;
    friend class CacheLine;

public:
    Cache(vp::ComponentConf &conf);

    void reset(bool active);

private:
    static void refill_event_clear_handler(vp::Block *__this, vp::ClockEvent *event);
    static void enable_sync(vp::Block *_this, bool active);
    static void flush_sync(vp::Block *_this, bool active);
    static void flush_line_sync(vp::Block *_this, bool active);
    static void flush_line_addr_sync(vp::Block *_this, uint32_t addr);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    vp::IoReqStatus handle_req(vp::IoReq *req);
    void check_state();
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    inline unsigned int getLineAddr(unsigned int addr) { return addr >> line_size_bits; }
    inline unsigned int get_line_base(unsigned int addr) { return addr & ~((1 << line_size_bits) - 1); }
    inline unsigned int get_tag(unsigned int addr) { return addr >> (line_size_bits + nb_sets_bits); }
    inline unsigned int get_line_index(unsigned int addr) { return (addr >> (line_size_bits)) & ((1 << nb_sets_bits) - 1); }
    inline unsigned int get_line_offset(unsigned int addr) { return addr & ((1 << line_size_bits) - 1); }
    inline unsigned int getAddr(unsigned int index, unsigned int tag) { return (tag << (line_size_bits + nb_sets_bits)) | (index << line_size_bits); }

    cache_line_t *refill(int line_index, unsigned int addr, unsigned int tag, vp::IoReq *req, bool *pending);
    static void refill_response(vp::Block *__this, vp::IoReq *req);
    cache_line_t *get_line(vp::IoReq *req, unsigned int *line_index, unsigned int *tag, unsigned int *line_offset);

    unsigned int stepLru();
    bool ioReq(vp::IoReq *req, int i);
    void enable(bool enable);
    void flush();
    void flush_line(unsigned int addr);

    unsigned int size;
    unsigned int line_size_bits;
    unsigned int nb_sets_bits;
    unsigned int nb_ways;
    unsigned int nb_sets;
    unsigned int line_size;

    bool enabled = false;
    bool enabled_at_reset;

    vp::Trace trace;

    vp::IoSlave input_itf;
    vp::IoMaster refill_itf;
    vp::WireSlave<bool> enable_itf;
    vp::WireSlave<bool> flush_itf;
    vp::WireMaster<bool> flush_ack_itf;
    vp::WireSlave<bool> flush_line_itf;
    vp::WireSlave<uint32_t> flush_line_addr_itf;

    vp::IoReq refill_req;

    int refill_latency;
    int refill_shift;
    uint32_t refill_offset;
    int64_t refill_timestamp;

    int64_t nextPacketStart;
    unsigned int R1;
    unsigned int R2;

    uint8_t lru_out;

    uint32_t line_offset_mask;
    uint32_t line_index_mask;
    uint32_t flush_line_addr;

    vp::Signal<uint64_t> req_event;
    vp::Signal<uint64_t> refill_event;
    vp::Trace io_event;

    vp::Queue refill_pending_reqs;

    cache_line_t *lines;

    cache_line_t *refill_line;
    uint32_t refill_tag;
    vp::Signal<bool> pending_refill;
    unsigned int pending_line_offset;

    vp::ClockEvent *fsm_event;
    vp::ClockEvent refill_event_clear_event;
};

Cache::Cache(vp::ComponentConf &config)
    : vp::Component(config),
    refill_pending_reqs(this, "refill_queue"),
    pending_refill(*this, "refill", 0),
    refill_event(*this, "refill_addr", 32),
    refill_event_clear_event(this, &Cache::refill_event_clear_handler),
    req_event(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ)
{
    this->enabled_at_reset = this->get_js_config()->get_child_bool("enabled");
    this->size = this->get_js_config()->get_child_int("size");
    this->nb_ways = this->get_js_config()->get_child_int("ways");
    this->line_size = this->get_js_config()->get_child_int("line_size");
    this->line_size_bits = ceil_log2(line_size);
    this->nb_sets = this->size / this->nb_ways / this->line_size;
    this->nb_sets_bits = ceil_log2(this->nb_sets);
    this->refill_latency = this->get_js_config()->get_child_int("refill_latency");
    this->refill_shift = this->get_js_config()->get_child_int("refill_shift");
    this->refill_offset = this->get_js_config()->get_child_int("refill_offset");

    this->R1 = 0xd3b6;
    this->R2 = 0x7775;

    this->lru_out = 0;

    this->input_itf.set_req_meth(&Cache::req);
    this->new_slave_port("input", &this->input_itf);

    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->enable_itf.set_sync_meth(Cache::enable_sync);
    this->new_slave_port("enable", &this->enable_itf);

    this->flush_itf.set_sync_meth(Cache::flush_sync);
    this->new_slave_port("flush", &this->flush_itf);

    this->flush_line_itf.set_sync_meth(Cache::flush_line_sync);
    this->new_slave_port("flush_line", &this->flush_line_itf);

    this->flush_line_addr_itf.set_sync_meth(Cache::flush_line_addr_sync);
    this->new_slave_port("flush_line_addr", &this->flush_line_addr_itf);

    this->refill_itf.set_resp_meth(&Cache::refill_response);
    this->new_master_port("refill", &this->refill_itf);

    this->new_master_port("flush_ack", &this->flush_ack_itf);

    traces.new_trace_event("port", &this->io_event, 32);

    lines = new cache_line_t[this->nb_sets * this->nb_ways];
    for (int i = 0; i < this->nb_sets; i++)
    {
        for (int j = 0; j < this->nb_ways; j++)
        {
            cache_line_t *line = &lines[i * this->nb_ways + j];
            line->timestamp = -1;
            line->tag = -1;
            line->data = new uint8_t[this->line_size];
            traces.new_trace_event("set_" + std::to_string(j) + "/line_" + std::to_string(i), &line->tag_event, 32);
        }
    }

    this->refill_timestamp = -1;
    this->line_index_mask = this->nb_sets - 1;
    this->line_offset_mask = this->line_size - 1;

    this->fsm_event = this->event_new(Cache::fsm_handler);

    this->trace.msg(vp::Trace::LEVEL_INFO, "Instantiating cache (nb_sets: %d, nb_ways: %d, line_size: %d)\n", 1 << this->nb_sets_bits, this->nb_ways, 1 << this->line_size_bits);
}

void Cache::reset(bool active)
{
    if (active)
    {
        this->flush();
        this->enabled = this->enabled_at_reset;
        this->refill_event.release();
    }
}

void Cache::refill_response(vp::Block *__this, vp::IoReq *req)
{
    Cache *_this = (Cache *)__this;

    vp_assert(_this->refill_pending_reqs.size() != 0, &_this->trace,
        "Received asynchronous response while no response is expected\n");

    vp::IoReq *pending_req = (vp::IoReq *)_this->refill_pending_reqs.pop();

    pending_req->restore();
    uint8_t *data = pending_req->get_data();
    bool is_write = req->get_is_write();
    uint64_t size = pending_req->get_size();

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received refill response (req: %p, is_write: %d, data: %p, size: 0x%x)\n",
                     pending_req, is_write, data, size);

    _this->pending_refill.set(0);
    _this->refill_event.release();

    if (data)
    {
        unsigned int line_index;
        unsigned int tag;
        cache_line_t *line = _this->refill_line;

        line->tag = _this->refill_tag;

        if (!is_write)
        {
            memcpy(data, (void *)&line->data[_this->pending_line_offset], size);
        }
        else
        {
            // hitLine->setDirty();
            memcpy((void *)&line->data[_this->pending_line_offset], data, size);
        }
    }

    pending_req->get_resp_port()->resp(pending_req);

    _this->check_state();
}

void Cache::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Cache *_this = (Cache *)__this;
    if (!_this->pending_refill.get() && !_this->refill_pending_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)_this->refill_pending_reqs.pop();
        req->restore();
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Resuming req (req: %p, is_write: %d, offset: 0x%x, size: 0x%x)\n",
                         req, req->get_is_write(), req->get_addr(), req->get_size());
        if (_this->handle_req(req) == vp::IO_REQ_OK)
        {
            req->get_resp_port()->resp(req);
        }
    }

    _this->check_state();
}

void Cache::check_state()
{
    if (!this->pending_refill.get() && !this->refill_pending_reqs.empty())
    {
        if (!this->fsm_event->is_enqueued())
        {
            this->event_enqueue(this->fsm_event, 1);
        }
    }
}

cache_line_t *Cache::refill(int line_index, unsigned int addr, unsigned int tag, vp::IoReq *req, bool *pending)
{
    // The cache supports only 1 refill at the same time.
    // If a refill occurs while another one is already pending, just enqueue the request and return.
    if (this->pending_refill.get())
    {
        req->save();
        this->refill_pending_reqs.push_back(req);
        *pending = true;
        return NULL;
    }

    unsigned int refillWay;

#if 1
    refillWay = this->stepLru() % this->nb_ways;
#else
    int electedAge = 0;
    int elected = -1;
    // Go through all lines to find the oldest one
    // At the same, decrease the age for everyone
    for (int i = 0; i < cache->nb_ways; i++)
    {
        if (elected == -1 || linesAge[i] < electedAge)
        {
            electedAge = linesAge[i];
            elected = i;
        }
        if (linesAge[i])
            linesAge[i]--;
    }
    // Set age of elected one to the max
    linesAge[elected] = cache->nb_ways - 1;
    refillWay = elected;
#endif

    cache_line_t *line = &this->lines[line_index * this->nb_ways + refillWay];

    uint32_t full_addr = (this->get_line_base(addr << this->refill_shift) + this->refill_offset);

    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Refilling line (addr: 0x%x, index: %d)\n", full_addr, line_index);
    // Flush the line in case it is dirty to copy it back outside
    // flush();

    line->tag_event.event((uint8_t *)&full_addr);

    // And get the data from outside
    vp::IoReq *refill_req = &this->refill_req;
    refill_req->init();
    refill_req->set_addr(full_addr);
    refill_req->set_is_write(false);
    refill_req->set_size(1 << this->line_size_bits);
    refill_req->set_data(line->data);

    this->refill_event_clear_event.cancel();

    vp::IoReqStatus err = this->refill_itf.req(refill_req);
    if (err != vp::IO_REQ_OK)
    {
        if (err == vp::IO_REQ_PENDING)
        {
            req->save();
            this->refill_pending_reqs.push_front(req);
            this->refill_line = line;
            this->refill_tag = tag;
            this->pending_refill.set(1);
            *pending = true;
            return NULL;
        }
        else
        {
            return NULL;
        }
    }

    line->tag = tag;

    if (!req->is_debug())
    {
        // This cache supports only one refill at the same time. Since we allow
        // synchronous request responses, make sure we report the delay in the latency
        // in case the cache is still supposed to be reilling a line.
        int64_t latency = 0;
        if (this->clock.get_cycles() < this->refill_timestamp)
        {
            latency += this->refill_timestamp - this->clock.get_cycles();
        }

        latency += refill_req->get_full_latency() + this->refill_latency;

        this->refill_timestamp = this->clock.get_cycles() + latency;
        this->refill_event_clear_event.enqueue(latency);

        req->inc_latency(latency);

        line->timestamp = this->clock.get_cycles() + latency;
    }

    return line;
}

void Cache::flush_line(unsigned int addr)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Flushing cache line (addr: 0x%x)\n", addr);
    unsigned int tag = addr >> this->line_size_bits;
    unsigned int line_index = this->get_line_index(addr);
    for (int i = 0; i < this->nb_ways; i++)
    {
        cache_line_t *line = &this->lines[line_index * this->nb_ways + i];
        if (line->tag == tag)
            line->tag = -1;
    }
}

void Cache::flush()
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Flushing whole cache\n");
    for (int i = 0; i < this->nb_sets; i++)
    {
        for (int j = 0; j < this->nb_ways; j++)
        {
            this->lines[i * this->nb_ways + j].tag = -1;
        }
    }

    if (this->flush_ack_itf.is_bound())
    {
        this->flush_ack_itf.sync(true);
    }
}

void Cache::enable(bool enable)
{
    this->enabled = enable;
    if (enable)
        this->trace.msg(vp::Trace::LEVEL_INFO, "Enabling cache\n");
    else
        this->trace.msg(vp::Trace::LEVEL_INFO, "Disabling cache\n");
}

cache_line_t *Cache::get_line(vp::IoReq *req, unsigned int *line_index, unsigned int *tag, unsigned int *line_offset)
{
    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    unsigned int nb_sets = this->nb_sets;
    unsigned int nb_ways = this->nb_ways;
    unsigned int line_size = 1 << this->line_size_bits;

    *tag = offset >> this->line_size_bits;
    *line_index = *tag & (nb_sets - 1);
    *line_offset = offset & (line_size - 1);

    cache_line_t *hit_line = NULL;

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Cache access (is_write: %d, offset: 0x%x, size: 0x%x, tag: 0x%x, line_index: %d, line_offset: 0x%x)\n", is_write, offset, size, offset, *line_index, *line_offset);

    cache_line_t *line = &this->lines[*line_index * nb_ways];

    for (int i = 0; i < nb_ways; i++)
    {
        if (line->tag == *tag)
        {
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Cache hit (way: %d)\n", i);
            hit_line = line;
            break;
        }
        line++;
    }

    return hit_line;
}

vp::IoReqStatus Cache::handle_req(vp::IoReq *req)
{
    unsigned int line_index;
    unsigned int tag;
    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();
    unsigned int line_offset;
    cache_line_t *hit_line = this->get_line(req, &line_index, &tag, &line_offset);

    if (hit_line == NULL)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Cache miss\n");
        this->refill_event.set(offset);
        bool pending = false;
        hit_line = this->refill(line_index, offset, tag, req, &pending);
        if (hit_line == NULL)
        {
            if (pending)
            {
                this->pending_line_offset = line_offset;
                return vp::IO_REQ_PENDING;
            }
            else
                return vp::IO_REQ_INVALID;
        }
    }
    else
    {
        // In case we hit the line, the line might have been refilled synchronously.
        // If so we need to apply the time taken by the refill.
        if (!req->is_debug())
        {
            if (this->clock.get_cycles() < hit_line->timestamp)
            {
                req->inc_latency(hit_line->timestamp - this->clock.get_cycles());
            }
        }
    }

    if (data)
    {
        if (!is_write)
        {
            memcpy(data, (void *)&hit_line->data[line_offset], size);
        }
        else
        {
            // hitLine->setDirty();
            memcpy((void *)&hit_line->data[line_offset], data, size);
        }
    }

    return vp::IO_REQ_OK;
}

vp::IoReqStatus Cache::req(vp::Block *__this, vp::IoReq *req)
{
    Cache *_this = (Cache *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Received req (req: %p, is_write: %d, offset: 0x%x, size: 0x%x)\n",
        req, is_write, offset, size);

    _this->req_event.set_and_release(offset);

    if (!_this->enabled)
    {
        req->set_addr((req->get_addr() << _this->refill_shift) + _this->refill_offset);
        return _this->refill_itf.req_forward(req);
    }

    _this->io_event.event((uint8_t *)&offset);

    return _this->handle_req(req);
}

unsigned int Cache::stepLru()
{
    if (1)
    {
        // 8 bits LFSR used on GAP FC icache
        int linear_feedback = !(((this->lru_out >> 7) & 1) ^ ((this->lru_out >> 3) & 1) ^ ((this->lru_out >> 2) & 1) ^ ((this->lru_out >> 1) & 1)); // TAPS for XOR feedback

        this->lru_out = (this->lru_out << 1) | (linear_feedback & 1);

        int way = (this->lru_out >> 1) & (this->nb_ways - 1);

        return way;
    }
    else
    {
        unsigned int b1 = (((R1 & 0x00000008) >> 3) ^ ((R1 & 0x00001000) >> 12) ^ ((R1 & 0x00004000) >> 14) ^ ((R1 & 0x00008000) >> 15)) & 1;
        unsigned int b2 = (((R2 & 0x00000008) >> 3) ^ ((R2 & 0x00001000) >> 12) ^ ((R2 & 0x00004000) >> 14) ^ ((R2 & 0x00008000) >> 15)) & 1;
        R1 <<= 1;
        R1 |= b1;
        R2 <<= 1;
        R2 |= b2;
        return (((R2 & 0x00010000) >> 15) | ((R1 & 0x00010000) >> 16));
    }
}

void Cache::enable_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    _this->enable(active);
}

void Cache::flush_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    if (active)
    {
        _this->flush();
    }
}

void Cache::flush_line_sync(vp::Block *__this, bool active)
{
    Cache *_this = (Cache *)__this;
    if (active)
        _this->flush_line(_this->flush_line_addr);
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
