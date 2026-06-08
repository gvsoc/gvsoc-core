// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * CDCTester β€” bidirectional io_v2 clock-domain-bridge testbench driver.
 *
 * Issues a fixed sequence of writes followed by reads against its single
 * io_v2 master port, verifying each read returns the pattern written.
 *
 * `pipeline_burst` controls how many transactions the tester keeps in
 * flight at once. With pipeline_burst=1 (default) each access waits for
 * its response before the next is issued β€” single-in-flight, matches the
 * 35-test calibration matrix unchanged. With pipeline_burst>1 the tester
 * fires up to N requests back-to-back from a pool of slots, demonstrating
 * pipelining when the bridge under test has depth>=N.
 *
 * Cross-tester quit coordination is unchanged: each tester pulses
 * `done_out` when its sequence completes; receiving the partner's pulse
 * on `done_in` plus being locally done causes engine->quit().
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/itf/wire.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>


class CDCTester : public vp::Component
{
public:
    CDCTester(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    enum Phase
    {
        PHASE_WRITE,
        PHASE_READ,
        PHASE_WAIT_PARTNER,
        PHASE_DONE,
    };

    static constexpr int MAX_BURST = 16;

    struct Slot
    {
        bool     active = false;
        uint64_t index;
        bool     is_write;
        uint8_t  data[64];
        uint8_t  expected[64];
        vp::IoReq req;
    };

    static void out_resp(vp::Block *__this, vp::IoReq *req);
    static void out_retry(vp::Block *__this, vp::IoRetryChannel);
    static void done_in_sync(vp::Block *__this, bool value);
    static void step_handler(vp::Block *__this, vp::ClockEvent *event);
    static void timeout_handler(vp::Block *__this, vp::ClockEvent *event);

    uint8_t pattern_byte(uint64_t off) const
    {
        return (uint8_t)((((uint32_t)this->pattern_seed ^ (off & 0xff))
                          + (uint32_t)(off >> 8)) & 0xff);
    }

    void schedule_step(int delay = 1);
    void step();
    bool issue_into_slot(int slot_idx, uint64_t index, bool is_write);
    int  find_free_slot();
    int  find_slot_for_req(vp::IoReq *req);
    bool issue_next_if_possible();
    void finish_local();
    void try_quit(int status);

    void fail(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void pass();

    vp::IoMaster out{&CDCTester::out_retry, &CDCTester::out_resp};
    vp::WireSlave<bool> done_in;
    vp::WireMaster<bool> done_out;
    vp::ClockEvent step_event;
    vp::ClockEvent timeout_event;
    vp::Trace trace;

    // Configuration:
    std::string logname;
    uint64_t base;
    uint64_t access_size;
    uint64_t nb_accesses;
    uint32_t pattern_seed;
    int64_t  quit_after_cycles;
    int      pipeline_burst;

    // State:
    Phase phase;
    uint64_t cursor;             // next access index to issue
    int64_t start_cycle = 0;
    bool partner_done = false;
    bool local_done = false;
    bool failed = false;
    bool retry_pending = false;  // last issue was DENIED; wait for retry

    Slot slots[MAX_BURST];
    int  in_flight_count = 0;
};


CDCTester::CDCTester(vp::ComponentConf &config)
    : vp::Component(config),
      step_event(this, &CDCTester::step_handler),
      timeout_event(this, &CDCTester::timeout_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_master_port("output", &this->out);

    this->done_in.set_sync_meth(&CDCTester::done_in_sync);
    this->new_slave_port("done_in", &this->done_in);

    this->new_master_port("done_out", &this->done_out);

    js::Config *cfg = this->get_js_config();
    this->logname     = cfg->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();
    this->base        = (uint64_t)cfg->get_child_int("base");
    this->access_size = (uint64_t)cfg->get_child_int("access_size");
    if (this->access_size == 0 || this->access_size > sizeof(this->slots[0].data))
    {
        this->trace.fatal("access_size %lu out of range (1..%zu)\n",
                          this->access_size, sizeof(this->slots[0].data));
    }
    this->nb_accesses = (uint64_t)cfg->get_child_int("nb_accesses");
    this->pattern_seed = (uint32_t)cfg->get_child_int("pattern_seed");
    this->quit_after_cycles = cfg->get_child_int("quit_after_cycles");
    if (this->quit_after_cycles <= 0) this->quit_after_cycles = 1000000;

    js::Config *pb = cfg->get("pipeline_burst");
    this->pipeline_burst = pb != nullptr ? pb->get_int() : 1;
    if (this->pipeline_burst < 1) this->pipeline_burst = 1;
    if (this->pipeline_burst > MAX_BURST) this->pipeline_burst = MAX_BURST;

    for (int i = 0; i < MAX_BURST; i++)
    {
        this->slots[i].req.set_data(this->slots[i].data);
        this->slots[i].req.set_size(this->access_size);
    }
}


void CDCTester::reset(bool active)
{
    if (!active)
    {
        this->phase = PHASE_WRITE;
        this->cursor = 0;
        this->partner_done = false;
        this->local_done = false;
        this->failed = false;
        this->retry_pending = false;
        this->in_flight_count = 0;
        for (int i = 0; i < MAX_BURST; i++) this->slots[i].active = false;
        this->start_cycle = this->clock.get_cycles();
        printf("[%ld] %s START accesses=%lu access_size=%lu base=0x%lx seed=0x%x burst=%d\n",
            this->clock.get_cycles(), this->logname.c_str(),
            this->nb_accesses, this->access_size, this->base, this->pattern_seed,
            this->pipeline_burst);
        this->schedule_step(1);
        this->timeout_event.enqueue(this->quit_after_cycles);
    }
}


void CDCTester::schedule_step(int delay)
{
    if (delay < 1) delay = 1;
    if (!this->step_event.is_enqueued())
        this->step_event.enqueue(delay);
}


int CDCTester::find_free_slot()
{
    for (int i = 0; i < MAX_BURST; i++)
        if (!this->slots[i].active) return i;
    return -1;
}


int CDCTester::find_slot_for_req(vp::IoReq *req)
{
    for (int i = 0; i < MAX_BURST; i++)
        if (this->slots[i].active && &this->slots[i].req == req) return i;
    return -1;
}


bool CDCTester::issue_into_slot(int slot_idx, uint64_t index, bool is_write)
{
    Slot &s = this->slots[slot_idx];
    uint64_t off = index * this->access_size;
    uint64_t addr = this->base + off;

    for (uint64_t i = 0; i < this->access_size; i++)
        s.expected[i] = this->pattern_byte(off + i);

    if (is_write)
    {
        for (uint64_t i = 0; i < this->access_size; i++)
            s.data[i] = s.expected[i];
    }
    else
    {
        for (uint64_t i = 0; i < this->access_size; i++)
            s.data[i] = 0xee;   // poison
    }

    s.req.prepare();
    s.req.set_addr(addr);
    s.req.set_size(this->access_size);
    s.req.set_is_write(is_write);
    s.req.set_data(s.data);
    s.req.is_first = true;
    s.req.is_last  = true;
    s.req.burst_id = -1;
    s.req.set_resp_status(vp::IO_RESP_OK);

    s.active   = true;
    s.index    = index;
    s.is_write = is_write;
    this->in_flight_count++;

    printf("[%ld] %s %-5s idx=%lu addr=0x%lx slot=%d\n",
        this->clock.get_cycles(), this->logname.c_str(),
        is_write ? "WRITE" : "READ",
        index, addr, slot_idx);

    vp::IoReqStatus st = this->out.req(&s.req);
    if (st == vp::IO_REQ_DONE)
    {
        if (s.req.get_resp_status() != vp::IO_RESP_OK)
        {
            this->fail("sync DONE INVALID idx=%lu is_write=%d", index, is_write);
            return false;
        }
        if (!is_write && memcmp(s.data, s.expected, this->access_size) != 0)
        {
            this->fail("sync DONE mismatch idx=%lu", index);
            return false;
        }
        s.active = false;
        this->in_flight_count--;
        return true;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        // Roll back: undo slot allocation.
        s.active = false;
        this->in_flight_count--;
        this->retry_pending = true;
        return false;
    }
    // GRANTED β€” response will arrive via out_resp.
    return true;
}


bool CDCTester::issue_next_if_possible()
{
    if (this->retry_pending) return false;
    if (this->in_flight_count >= this->pipeline_burst) return false;
    if (this->cursor >= this->nb_accesses) return false;
    int slot = this->find_free_slot();
    if (slot < 0) return false;
    bool ok = this->issue_into_slot(slot, this->cursor,
                                    this->phase == PHASE_WRITE);
    if (ok)
    {
        this->cursor++;
    }
    return ok;
}


void CDCTester::step()
{
    switch (this->phase)
    {
        case PHASE_WRITE:
            // Drain accepted writes; move to read phase when all done.
            while (this->issue_next_if_possible()) {}
            if (this->cursor >= this->nb_accesses
                && this->in_flight_count == 0
                && !this->retry_pending)
            {
                printf("[%ld] %s WRITES_DONE\n",
                    this->clock.get_cycles(), this->logname.c_str());
                this->phase = PHASE_READ;
                this->cursor = 0;
                this->schedule_step(1);
                return;
            }
            break;

        case PHASE_READ:
            while (this->issue_next_if_possible()) {}
            if (this->cursor >= this->nb_accesses
                && this->in_flight_count == 0
                && !this->retry_pending)
            {
                this->pass();
                return;
            }
            break;

        case PHASE_WAIT_PARTNER:
        case PHASE_DONE:
            break;
    }

    // If we still have outstanding requests or pending retry, idle. The
    // resp / retry handlers reschedule the step when state changes.
    if (this->in_flight_count == 0
        && !this->retry_pending
        && this->phase != PHASE_DONE
        && this->phase != PHASE_WAIT_PARTNER)
    {
        this->schedule_step(1);
    }
}


void CDCTester::out_resp(vp::Block *__this, vp::IoReq *req)
{
    CDCTester *_this = (CDCTester *)__this;
    if (_this->failed) return;

    int slot_idx = _this->find_slot_for_req(req);
    if (slot_idx < 0)
    {
        _this->fail("resp for unknown req=%p", (void *)req);
        return;
    }
    Slot &s = _this->slots[slot_idx];

    if (req->get_resp_status() != vp::IO_RESP_OK)
    {
        _this->fail("async resp INVALID idx=%lu is_write=%d",
            s.index, s.is_write ? 1 : 0);
        return;
    }
    if (!s.is_write
        && memcmp(s.data, s.expected, _this->access_size) != 0)
    {
        _this->fail("read mismatch idx=%lu (async)", s.index);
        return;
    }

    s.active = false;
    _this->in_flight_count--;
    _this->schedule_step(1);
}


void CDCTester::out_retry(vp::Block *__this, vp::IoRetryChannel)
{
    CDCTester *_this = (CDCTester *)__this;
    if (_this->retry_pending)
    {
        _this->retry_pending = false;
        _this->schedule_step(1);
    }
}


void CDCTester::done_in_sync(vp::Block *__this, bool value)
{
    CDCTester *_this = (CDCTester *)__this;
    if (!value) return;
    if (_this->partner_done) return;
    _this->partner_done = true;
    printf("[%ld] %s PARTNER_DONE\n",
        _this->clock.get_cycles(), _this->logname.c_str());
    if (_this->local_done)
        _this->try_quit(_this->failed ? 1 : 0);
}


void CDCTester::finish_local()
{
    this->local_done = true;
    if (this->done_out.is_bound())
        this->done_out.sync(true);
    if (this->partner_done)
        this->try_quit(this->failed ? 1 : 0);
    else
        this->phase = PHASE_WAIT_PARTNER;
}


void CDCTester::try_quit(int status)
{
    if (this->phase == PHASE_DONE) return;
    this->phase = PHASE_DONE;
    if (this->timeout_event.is_enqueued())
        this->timeout_event.cancel();
    this->time.get_engine()->quit(status);
}


void CDCTester::step_handler(vp::Block *__this, vp::ClockEvent *event)
{
    CDCTester *_this = (CDCTester *)__this;
    _this->step();
}


void CDCTester::timeout_handler(vp::Block *__this, vp::ClockEvent *event)
{
    CDCTester *_this = (CDCTester *)__this;
    _this->fail("timeout after %ld cycles in phase %d (cursor=%lu in_flight=%d)",
        _this->quit_after_cycles, (int)_this->phase,
        _this->cursor, _this->in_flight_count);
}


void CDCTester::fail(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("[%ld] %s FAIL %s\n", this->clock.get_cycles(), this->logname.c_str(), buf);
    this->failed = true;
    this->try_quit(1);
}


void CDCTester::pass()
{
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s PASS writes=%lu reads=%lu cycles=%ld\n",
        now, this->logname.c_str(),
        this->nb_accesses, this->nb_accesses,
        (long)(now - this->start_cycle));
    this->finish_local();
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new CDCTester(config);
}
