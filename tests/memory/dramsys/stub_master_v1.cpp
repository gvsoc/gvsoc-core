/*
 * Testbench master for the memory.dramsys wrapper (io v1 protocol).
 *
 * Reads a schedule from get_js_config()/schedule: a list of entries with
 *   { cycle, addr, size, is_write, name, data_hex }
 * For each entry the master allocates an IoReq, fills its data buffer
 * (from data_hex for writes, zeroed for reads), and sends it at the
 * issue cycle. It logs SEND, then one of:
 *   - DONE inline           on IO_REQ_OK    (the wrapper does this for writes
 *                                            it can immediately accept)
 *   - DENY/GRANT then RESP  on IO_REQ_DENIED + later async resp()
 *                           (the wrapper's denied_req_queue path)
 *   - RESP                  on IO_REQ_PENDING + later async resp()
 *                           (the wrapper's normal read path)
 *
 * Note on DENIED: the wrapper keeps the req on its own denied_req_queue,
 * so on grant() we do NOT re-issue — we just log GRANT and wait for resp()
 * to fire (or, for writes, for the wrapper's reqCallback to call resp()
 * straight after grant()).
 *
 * When the last in-flight request resolves, the master enqueues a quit
 * event quit_after_cycles cycles later.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

class StubMasterV1 : public vp::Component
{
public:
    StubMasterV1(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    struct ScheduleEntry {
        int64_t  cycle;
        uint64_t addr;
        uint64_t size;
        bool     is_write;
        std::string name;
        vp::IoReq *req = nullptr;
        uint8_t  *data = nullptr;
        bool     resolved = false;
        // If true, this entry is NOT fired by the cycle-timer; instead it
        // is issued from mark_resolved() when the immediately preceding
        // entry resolves (DONE or RESP). Use to build serialised chains.
        bool     chain_to_prev = false;
        size_t   idx = 0;
    };

    static void resp_handler(vp::Block *__this, vp::IoReq *req);
    static void grant_handler(vp::Block *__this, vp::IoReq *req);
    static void issue_handler(vp::Block *__this, vp::ClockEvent *event);
    static void chain_handler(vp::Block *__this, vp::ClockEvent *event);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    void issue(ScheduleEntry *entry);
    void mark_resolved(ScheduleEntry *entry);
    void release_if_idle();
    ScheduleEntry *entry_from_req(vp::IoReq *req);
    void log_done(const char *tag, ScheduleEntry *e);

    vp::Trace trace;
    vp::IoMaster out;
    vp::ClockEvent issue_event;
    vp::ClockEvent chain_event;
    vp::ClockEvent quit_event;
    ScheduleEntry *pending_chain = nullptr;

    // VCD-visible request trace: pulses on each SEND (high-Z when idle) plus
    // a sticky counter of in-flight (PENDING/DENIED) requests.
    vp::Signal<uint64_t> sig_req_addr;
    vp::Signal<uint64_t> sig_req_size;
    vp::Signal<bool>     sig_req_is_write;
    vp::Signal<uint64_t> sig_pending;

    std::vector<ScheduleEntry *> schedule;
    size_t      next_to_schedule = 0;
    size_t      nb_resolved = 0;
    int         nb_inflight = 0;
    std::string logname;
    int64_t     quit_after_cycles = 10000;
};

StubMasterV1::StubMasterV1(vp::ComponentConf &config)
    : vp::Component(config),
      issue_event(this, &StubMasterV1::issue_handler),
      chain_event(this, &StubMasterV1::chain_handler),
      quit_event(this, &StubMasterV1::quit_handler),
      sig_req_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
      sig_req_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
      sig_req_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      sig_pending(*this, "pending", 64)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->out.set_resp_meth(&StubMasterV1::resp_handler);
    this->out.set_grant_meth(&StubMasterV1::grant_handler);
    this->new_master_port("output", &this->out);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();

    int qac = this->get_js_config()->get_child_int("quit_after_cycles");
    if (qac > 0) this->quit_after_cycles = qac;

    js::Config *schedule_cfg = this->get_js_config()->get("schedule");
    if (schedule_cfg != nullptr)
    {
        for (auto &item : schedule_cfg->get_elems())
        {
            ScheduleEntry *e = new ScheduleEntry();
            e->cycle    = item->get_int("cycle");
            e->addr     = (uint64_t)item->get_int("addr");
            e->size     = (uint64_t)item->get_int("size");
            e->is_write = item->get_child_bool("is_write");
            e->name     = item->get_child_str("name");
            if (e->name.empty()) e->name = "req" + std::to_string(this->schedule.size());

            e->data = new uint8_t[e->size];
            for (uint64_t i = 0; i < e->size; i++) e->data[i] = 0;

            // Optional contiguous hex pre-fill (e.g. "deadbeef" for a 4-byte write).
            std::string data_hex = item->get_child_str("data_hex");
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

            e->chain_to_prev = item->get_child_bool("chain_to_prev");
            e->idx = this->schedule.size();
            e->req = new vp::IoReq(e->addr, e->data, e->size, e->is_write);
            this->schedule.push_back(e);
        }
    }
}

void StubMasterV1::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_to_schedule == 0)
    {
        int64_t first = this->schedule[0]->cycle;
        if (first <= 0) first = 1;
        this->issue_event.enqueue(first);
    }
}

StubMasterV1::ScheduleEntry *StubMasterV1::entry_from_req(vp::IoReq *req)
{
    for (ScheduleEntry *e : this->schedule)
    {
        if (e->req == req) return e;
    }
    return nullptr;
}

void StubMasterV1::log_done(const char *tag, ScheduleEntry *e)
{
    int64_t now = this->clock.get_cycles();
    char hex[17] = { 0 };
    int n = e->size < 8 ? (int)e->size : 8;
    for (int i = 0; i < n; i++)
        snprintf(&hex[i*2], 3, "%02x", e->data[i]);
    // XOR checksum across the entire payload — lets large transfers be
    // validated end-to-end without printing the whole buffer.
    uint8_t cksum = 0;
    for (uint64_t i = 0; i < e->size; i++) cksum ^= e->data[i];
    printf("[%ld] %s %s name=%s latency=%lu data=%s checksum=%02x\n",
        now, this->logname.c_str(), tag, e->name.c_str(),
        (unsigned long)e->req->get_latency(), hex, cksum);
}

void StubMasterV1::mark_resolved(ScheduleEntry *e)
{
    if (e->resolved) return;
    e->resolved = true;
    this->nb_resolved++;

    // If the next entry is chained to this one and hasn't been issued
    // yet, defer the issue via a 1-cycle clock event rather than calling
    // issue() synchronously here. The reason: this callback typically
    // runs inside the SystemC kernel's slice (DRAMSys's async callback),
    // when GVSoC's TimeEngine hasn't been stepped to current SC time
    // yet. Calling issue() now would collapse onto the resp's GVSoC
    // cycle. Enqueuing instead triggers was_updated -> sync_event.notify
    // -> the systemc_driver SC_THREAD wakes, calls step_until_sync to
    // current SC time, and only then processes our event — so the
    // chained issue lands on a strictly later GVSoC cycle.
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

    if (this->nb_resolved == this->schedule.size() &&
        !this->quit_event.is_enqueued())
    {
        this->quit_event.enqueue(this->quit_after_cycles);
    }
}

void StubMasterV1::issue(ScheduleEntry *e)
{
    int64_t now = this->clock.get_cycles();
    printf("[%ld] %s SEND name=%s addr=0x%lx size=%lu write=%d\n",
        now, this->logname.c_str(), e->name.c_str(),
        e->addr, (unsigned long)e->size, e->is_write ? 1 : 0);

    e->req->prepare();
    e->req->set_addr(e->addr);
    e->req->set_size(e->size);
    e->req->set_is_write(e->is_write);
    e->req->set_data(e->data);

    // Drive the request signals sticky: visible from SEND until the request
    // resolves (release_if_idle below puts them back to high-Z when the
    // in-flight count reaches 0).
    this->sig_req_addr     = e->addr;
    this->sig_req_size     = e->size;
    this->sig_req_is_write = e->is_write;
    this->nb_inflight++;
    this->sig_pending      = this->nb_inflight;

    vp::IoReqStatus st = this->out.req(e->req);
    switch (st)
    {
        case vp::IO_REQ_OK:
            this->log_done("DONE", e);
            this->mark_resolved(e);
            this->release_if_idle();
            break;
        case vp::IO_REQ_PENDING:
            printf("[%ld] %s PEND name=%s\n",
                now, this->logname.c_str(), e->name.c_str());
            break;
        case vp::IO_REQ_DENIED:
            // The wrapper keeps the req on its own denied queue. Wait for
            // grant() — do NOT re-issue.
            printf("[%ld] %s DENY name=%s\n",
                now, this->logname.c_str(), e->name.c_str());
            break;
        case vp::IO_REQ_INVALID:
            printf("[%ld] %s INVALID name=%s\n",
                now, this->logname.c_str(), e->name.c_str());
            this->mark_resolved(e);
            this->release_if_idle();
            break;
    }
}

void StubMasterV1::release_if_idle()
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

void StubMasterV1::issue_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV1 *_this = (StubMasterV1 *)__this;
    if (_this->next_to_schedule >= _this->schedule.size()) return;

    // Stop the cycle-driven chain at the first chain_to_prev entry;
    // mark_resolved will pick it up from the previous request's resp.
    ScheduleEntry *e = _this->schedule[_this->next_to_schedule];
    if (e->chain_to_prev) return;

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

void StubMasterV1::chain_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV1 *_this = (StubMasterV1 *)__this;
    if (_this->pending_chain)
    {
        ScheduleEntry *e = _this->pending_chain;
        _this->pending_chain = nullptr;
        _this->issue(e);
    }
}

void StubMasterV1::grant_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMasterV1 *_this = (StubMasterV1 *)__this;
    ScheduleEntry *e = _this->entry_from_req(req);
    const char *name = e ? e->name.c_str() : "?";
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s GRANT name=%s\n", now, _this->logname.c_str(), name);
}

void StubMasterV1::resp_handler(vp::Block *__this, vp::IoReq *req)
{
    StubMasterV1 *_this = (StubMasterV1 *)__this;
    ScheduleEntry *e = _this->entry_from_req(req);
    if (e == nullptr) return;
    _this->log_done("RESP", e);
    _this->mark_resolved(e);
    _this->release_if_idle();
}

void StubMasterV1::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubMasterV1 *_this = (StubMasterV1 *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    _this->time.get_engine()->quit(0);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubMasterV1(config);
}
