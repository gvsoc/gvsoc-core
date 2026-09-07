// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// memory.ramulator — GVSoC wrapper around Ramulator 2.x (cycle-level DRAM
// simulator) used as a timing engine, on the io_v2 *beat* protocol (IoV2Beat).
//
// Protocol (matches the IoV2Beat contract, incl. the per-burst write
// acknowledgement — see io_v2.hpp "Write acknowledgement"):
//   - Read : the master sends ONE data-less request for the whole burst.
//     The wrapper returns IO_REQ_GRANTED and answers with a stream of beats:
//     ceil(size / beat_width) per-cycle resp() calls, one beat_width slice per
//     cycle, with is_first / is_last / burst_id set. beat_width is the
//     interconnect/DRAM transaction width (Ramulator's get_tx_bytes()).
//   - Write: beats arrive as requests — either one request carrying the whole
//     payload (big-packet-form write = a one-beat burst) or N per-beat
//     requests (beat-form write). Each accepted beat is consumed at submit
//     (payload copied into the backing store) and GRANTED: a non-last beat is
//     freed right away, the burst-final beat (is_last) is parked. The burst
//     is acknowledged exactly ONCE, with a single data-less allocator-backed
//     ack — the parked last beat recycled — via resp(). Every beat still
//     occupies its slot in the per-cycle ack pacing as a SILENT virtual tick
//     (same cursors, head-of-queue and 1-beat/cycle rate as the former
//     per-beat ack stream), so the single ack fires on exactly the cycle the
//     last per-beat ack fired before the protocol change.
//   - Atomic opcodes (opcode != WRITE/READ) keep the classic per-request
//     round-trip: the master's own object comes back through resp().
//
// Timing: a permanent ClockEvent ticks the Ramulator memory system every cycle
// (no idle optimization in this MVP — intentional). Each whole-burst request is
// split into ceil(size / tx_bytes) Ramulator transactions; as those complete
// they make beats available, which the tick handler streams out one per cycle.
// Because we tick at the DRAM clock rate, the completion time already is the
// correct GVSoC time — no cycle->ps translation.
//
// Functional data lives in a local backing store (Ramulator models timing
// only, like memory_v3). Ramulator is linked directly (C++ API, not a C ABI);
// the model is only built when libramulator is found (see CMakeLists.txt) and
// is compiled with -std=c++20.
//
// Back-pressure is entirely Ramulator's: the controller request queue is its
// read/write buffer, so the wrapper keeps no request FIFO and no outstanding
// cap. A request is IO_REQ_DENIED only when Ramulator's buffer can't accept a
// transaction (receive_external_requests() returns false), and retried when a
// buffer slot frees. An accepted read burst is queued so its data beats can be
// streamed back as Ramulator completes its transactions — that queue is the
// outstanding-request table, not a controller FIFO.
//
// Accepted MVP limitations (documented for follow-up):
//   - beat_width == tx_bytes (one beat == one DRAM transaction).
//   - Bursts are answered in arrival (FIFO) order; only the head burst emits.

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/signal.hpp>

#include <ramulator/base/config.h>
#include <ramulator/base/factory.h>
#include <ramulator/base/request.h>
#include <ramulator/frontend/i_frontend.h>
#include <ramulator/memory_system/i_memory_system.h>

#include "ramulator_bridge.hpp"
#include <memory/ramulator/ramulator_config.hpp>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>


// Component class is named RamulatorModel to avoid clashing with Ramulator's
// own `Ramulator::` namespace. The GVSoC component name (memory.ramulator) is
// set by the Python generator, independently of this C++ class name.
class RamulatorModel : public vp::Component, public ramulator_bridge::IDramCommandSink
{
public:
    RamulatorModel(vp::ComponentConf &conf);
    ~RamulatorModel();

    void reset(bool active) override;

    // IDramCommandSink — fed by the GvsocBridge Ramulator plugin so the DRAM
    // command stream can be drawn on the gvsoc-gui3 timeline.
    void on_dram_setup(int channel, const std::vector<std::string> &level_names,
        const std::vector<int> &level_sizes) override;
    void on_dram_command(const ramulator_bridge::DramCommand &cmd) override;

private:
    // One in-flight request (a whole read burst / big-packet-form write, or a
    // single beat of a beat-form write burst — pacing is per REQUEST: each
    // request gets its own Inflight and drains its own DRAM transactions).
    // A read burst is data-less (beat protocol): its functional data is
    // snapshot into the read_data staging buffer at submit and travels
    // upstream inside distinct allocator-backed beats; the master's request
    // is never touched. A WRITE beat is consumed at submit: a non-last beat
    // is freed immediately (req == nullptr, silent virtual-ack entry), the
    // burst-final beat (burst_last) is parked in req and later recycled as
    // the burst's single data-less ack (owns_req). Atomic opcodes keep the
    // classic round-trip of the master's own object through req/master_data.
    struct Inflight
    {
        vp::IoReq *req;
        uint8_t *master_data;   // snapshot of req->get_data() at submit (atomics)
        std::vector<uint8_t> read_data; // staging for read bursts
        void *initiator;        // snapshot of req->initiator at submit
        uint64_t total_size;    // snapshot of req->get_size() at submit
        uint64_t burst_addr;    // snapshot of req->get_addr() at submit
        uint64_t aligned_start; // tx-aligned base address of the burst
        int64_t burst_id;       // snapshot of req->burst_id at submit
        int type;               // Ramulator::Request::Type::Read / Write
        uint64_t bytes_filled;  // bytes whose DRAM timing is done (<= total_size)
        uint64_t bytes_emitted; // bytes already emitted as beats
        int tx_total;           // number of Ramulator transactions for this burst
        int tx_injected;        // transactions pushed into Ramulator so far
        int tx_done;            // transactions completed so far
        bool emitted_done;      // true once the last beat has been emitted
        // Per-burst write-ack bookkeeping (io_v2.hpp "Write acknowledgement").
        vp::IoReqOpcode opcode = vp::READ; // snapshot of req->get_opcode()
        bool burst_last = false; // snapshot of req->is_last at submit (writes)
        bool owns_req = false;   // parked burst-final write beat: ours to free
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void tick_handler(vp::Block *__this, vp::ClockEvent *event);

    // Push pending transactions of in-flight bursts into Ramulator's controller
    // queue until Ramulator is full (its buffer is the real back-pressure).
    void pump_injections();
    bool inject_one(Inflight *infl);
    // Re-admit the master once an outstanding-burst slot frees up.
    void maybe_retry();
    void emit_one_beat();

    // Map a command's address vector to a per-bank GUI lane (or the scope lane
    // for rank/all-bank commands like refresh).
    int lane_for(const int *addr_vec, int level_count);

    // All tunables, filled from the RamulatorConfig dataclass (memory_v3 style).
    RamulatorConfig cfg;

    vp::Trace trace;
    vp::IoSlave in_itf;
    vp::ClockEvent tick_event;

    // --- DRAM command visualization (per-bank timeline lanes) ---------------
    // Lanes 0..num_banks-1 are individual banks; lane `num_banks` is the scope
    // lane (rank/all-bank commands such as refresh). Sized from the `num_banks`
    // generator param and validated against the Ramulator geometry at setup.
    int num_banks = 0;
    int scope_lane = 0;
    std::vector<int> lane_levels;   // address levels that identify a bank
    std::vector<int> lane_sizes;    // their sizes (mixed-radix for lane index)
    int row_level = -1;
    std::vector<vp::Trace *> cmd_trace;          // per-lane command label (string)
    std::vector<vp::Signal<uint32_t> *> row_sig; // per-lane open row (persistent)
    std::vector<vp::Signal<uint32_t> *> cmdid_sig; // per-lane command id (pulse)

    // Functional backing store. Ramulator only models timing.
    uint8_t *mem_data = nullptr;
    uint64_t size = 0;
    uint64_t truncate_mask = (uint64_t)-1;

    // Ramulator instance (timing only).
    Ramulator::IFrontEnd *frontend = nullptr;
    Ramulator::IMemorySystem *memory_system = nullptr;
    int tx_bytes = 0;
    int tx_align_log2 = 0;
    int beat_width = 0;   // == tx_bytes
    // Shared pool serving beat-sized requests with their payload co-allocated;
    // read response beats are drawn from it and freed by the terminal master.
    vp::IoReqAllocator *beat_allocator = nullptr;

    // Back-pressure is entirely Ramulator's: a request is DENIED only when
    // Ramulator's controller buffer can't accept a transaction, and retried when
    // a buffer slot frees (slot_freed, set by the completion callback). No
    // wrapper request FIFO and no artificial outstanding cap.
    bool retry_pending = false;
    bool slot_freed = false;

    std::deque<Inflight *> inflight_queue;  // FIFO; only the head emits beats
};


RamulatorModel::RamulatorModel(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      in_itf(&RamulatorModel::req_handler),
      tick_event(this, &RamulatorModel::tick_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in_itf);

    // Every tunable comes from the compiled RamulatorConfig struct.
    this->size = (uint64_t)this->cfg.size;
    bool init = this->cfg.init;
    std::string config_yaml = this->cfg.config_yaml ? this->cfg.config_yaml : "";
    int cfg_beat_width = this->cfg.beat_width;
    this->num_banks = this->cfg.num_banks;

    this->truncate_mask = this->cfg.truncate ? (this->size - 1) : (uint64_t)-1;

    if (config_yaml.empty())
    {
        this->trace.fatal("No Ramulator config provided (config_yaml is empty)\n");
        return;
    }

    this->mem_data = (uint8_t *)calloc(this->size, 1);
    if (this->mem_data == nullptr) throw std::bad_alloc();
    if (init && this->size < (2 << 24))
    {
        memset(this->mem_data, 0x57, this->size);
    }

    // Per-bank command-visualization signals (lanes 0..num_banks-1) plus one
    // scope lane (num_banks) for rank/all-bank commands. Created before the
    // Ramulator instance so they exist when the bridge plugin's setup() and the
    // first on_dram_command() fire during connect below.
    if (this->num_banks > 0)
    {
        this->scope_lane = this->num_banks;
        int lanes = this->num_banks + 1;
        for (int i = 0; i < lanes; i++)
        {
            std::string base = i < this->num_banks ? "bank" + std::to_string(i) : "scope";
            vp::Trace *t = new vp::Trace();
            this->traces.new_trace_event_string(base + "_cmd", t);
            this->cmd_trace.push_back(t);
            this->row_sig.push_back(new vp::Signal<uint32_t>(
                *this, base + "_row", 32, vp::SignalCommon::ResetKind::HighZ));
            this->cmdid_sig.push_back(new vp::Signal<uint32_t>(
                *this, base + "_cmdid", 8, vp::SignalCommon::ResetKind::HighZ));
        }
    }

    this->trace.msg("Loading Ramulator config (path: %s)\n", config_yaml.c_str());
    auto ramulator_config = Ramulator::Config::parse_config_file(config_yaml);
    this->frontend = Ramulator::Factory::create_frontend(ramulator_config);
    this->memory_system = Ramulator::Factory::create_memory_system(ramulator_config);
    // Publish ourselves as the command sink while the bridge plugin's setup()
    // runs (during connect), then clear it.
    ramulator_bridge::set_pending_sink(this);
    this->frontend->connect_memory_system(this->memory_system);
    this->memory_system->connect_frontend(this->frontend);
    ramulator_bridge::set_pending_sink(nullptr);

    this->tx_bytes = this->memory_system->get_tx_bytes();
    if (this->tx_bytes <= 0)
    {
        this->trace.fatal("Ramulator reported invalid tx_bytes: %d\n", this->tx_bytes);
        return;
    }
    this->beat_width = this->tx_bytes;
    this->beat_allocator = vp::IoReqAllocator::get(this->beat_width);
    if (cfg_beat_width != this->tx_bytes)
    {
        this->trace.fatal(
            "beat_width mismatch: generator declares %d but Ramulator tx_bytes is %d. "
            "Set Ramulator(..., beat_width=%d).\n",
            cfg_beat_width, this->tx_bytes, this->tx_bytes);
        return;
    }
    this->tx_align_log2 = 0;
    while ((1 << this->tx_align_log2) < this->tx_bytes) this->tx_align_log2++;

    this->trace.msg("Ramulator instantiated (size: 0x%llx, beat_width: %d)\n",
        (unsigned long long)this->size, this->beat_width);
}


RamulatorModel::~RamulatorModel()
{
    for (auto *infl : this->inflight_queue)
    {
        // A parked burst-final write beat is ours (consumed, awaiting
        // recycling as the ack) — return it to its pool. Silent write entries
        // hold no master object; read/atomic requests are master-owned.
        if (infl->owns_req && infl->req != nullptr) infl->req->free();
        delete infl;
    }
    if (this->frontend) this->frontend->finalize();
    if (this->memory_system) this->memory_system->finalize();
    if (this->mem_data) free(this->mem_data);
    for (auto *t : this->cmd_trace) delete t;
    for (auto *s : this->row_sig) delete s;
    for (auto *s : this->cmdid_sig) delete s;
}


// Capture the DRAM address layout and validate the lane count. Lane-identifying
// levels are everything except the Row and Column levels; the lane index is a
// mixed-radix encoding over them (so it is unique per channel/rank/bankgroup/
// bank). Refresh / all-bank commands carry -1 in the bank levels and land on
// the scope lane.
void RamulatorModel::on_dram_setup(int /*channel*/,
    const std::vector<std::string> &level_names,
    const std::vector<int> &level_sizes)
{
    if (this->num_banks <= 0) return;  // visualization disabled

    // The layout is global (same for every controller/channel); compute once.
    if (!this->lane_levels.empty()) return;

    int computed = 1;
    for (int i = 0; i < (int)level_names.size(); i++)
    {
        const std::string &n = level_names[i];
        if (n == "Row") { this->row_level = i; continue; }
        if (n == "Column" || n == "Col") continue;
        this->lane_levels.push_back(i);
        this->lane_sizes.push_back(level_sizes[i]);
        computed *= level_sizes[i];
    }

    if (computed != this->num_banks)
    {
        this->trace.fatal(
            "num_banks mismatch: generator declares %d but Ramulator geometry has "
            "%d banks. Set Ramulator(..., num_banks=%d).\n",
            this->num_banks, computed, computed);
    }
}


int RamulatorModel::lane_for(const int *addr_vec, int level_count)
{
    int lane = 0;
    for (size_t k = 0; k < this->lane_levels.size(); k++)
    {
        int lvl = this->lane_levels[k];
        int idx = lvl < level_count ? addr_vec[lvl] : -1;
        if (idx < 0) return this->scope_lane;  // rank/all-bank scope command
        lane = lane * this->lane_sizes[k] + idx;
    }
    return lane;
}


void RamulatorModel::on_dram_command(const ramulator_bridge::DramCommand &cmd)
{
    if (this->num_banks <= 0 || this->lane_levels.empty()) return;

    int lane = this->lane_for(cmd.addr_vec, cmd.level_count);
    if (lane < 0 || lane > this->scope_lane) return;

    // The controller issues at most one command per cycle, so per-lane signals
    // never collide within a cycle — emit directly at the current time.
    this->cmd_trace[lane]->event_string(cmd.command_name, false);
    this->cmdid_sig[lane]->set_and_release((uint32_t)cmd.command_id);
    if (this->row_level >= 0 && this->row_level < cmd.level_count
        && cmd.addr_vec[this->row_level] >= 0)
    {
        this->row_sig[lane]->set((uint32_t)cmd.addr_vec[this->row_level]);
    }
}


void RamulatorModel::reset(bool active)
{
    if (active)
    {
        for (auto *infl : this->inflight_queue)
        {
            // Same ownership rules as the destructor: only a parked
            // burst-final write beat (owns_req) is ours to free — silent
            // write entries hold no master object, and read / atomic
            // requests belong to the master.
            if (infl->owns_req && infl->req != nullptr) infl->req->free();
            delete infl;
        }
        this->inflight_queue.clear();
        this->retry_pending = false;
        if (this->tick_event.is_enqueued()) this->tick_event.cancel();
    }
    else
    {
        // Permanent per-cycle tick (MVP: no idle gating).
        this->tick_event.enable();
    }
}


vp::IoReqStatus RamulatorModel::req_handler(vp::Block *__this, vp::IoReq *req)
{
    RamulatorModel *_this = (RamulatorModel *)__this;

    // Snapshot every field we may still need BEFORE anything else: on the
    // write path GRANTED transfers beat ownership (buffer included) to us
    // and the beat is freed below (io_v2.hpp "Write-beat ownership").
    uint64_t offset = req->get_addr() & _this->truncate_mask;
    uint64_t req_size = req->get_size();
    uint8_t *data = req->get_data();
    vp::IoReqOpcode opcode = req->get_opcode();
    bool burst_last = req->is_last;

    _this->trace.msg("Burst (addr: 0x%llx, offset: 0x%llx, size: 0x%llx, write: %d, burst_id: %lld)\n",
        (unsigned long long)req->get_addr(), (unsigned long long)offset,
        (unsigned long long)req_size, req->get_is_write(), (long long)req->burst_id);

    if (offset + req_size > _this->size)
    {
        // Error escape hatch (io_v2.hpp): inline DONE + INVALID is legal on
        // any beat — the master keeps the beat and aborts the burst.
        _this->trace.force_warning_no_error(
            "Out-of-bound burst (offset: 0x%llx, size: 0x%llx, memSize: 0x%llx)\n",
            (unsigned long long)offset, (unsigned long long)req_size,
            (unsigned long long)_this->size);
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    uint64_t aligned_start = (offset >> _this->tx_align_log2) << _this->tx_align_log2;
    int num_tx = 1 + (int)(((offset + req_size - 1) - aligned_start) >> _this->tx_align_log2);
    int type = req->get_is_write() ? Ramulator::Request::Type::Write
                                    : Ramulator::Request::Type::Read;

    Inflight *infl = new Inflight{
        req, data, {}, req->initiator,
        req_size, req->get_addr(), aligned_start, req->burst_id, type,
        /*bytes_filled=*/0, /*bytes_emitted=*/0,
        /*tx_total=*/num_tx, /*tx_injected=*/0, /*tx_done=*/0, /*emitted_done=*/false,
    };
    infl->opcode = opcode;
    infl->burst_last = burst_last;

    // Back-pressure is Ramulator's: if its controller buffer can't even take the
    // first transaction, deny (nothing committed, ownership not transferred)
    // and retry() when a slot frees. There is no wrapper request queue and no
    // artificial outstanding cap.
    if (!_this->inject_one(infl))
    {
        delete infl;
        _this->retry_pending = true;
        return vp::IO_REQ_DENIED;
    }

    // Accepted. Move the functional data now (Ramulator only times the access):
    // a write carries its payload in the request; a read burst is data-less, so
    // its data is snapshot into the staging buffer and streamed back inside
    // allocator-backed beats over the next cycles. Remaining transactions are
    // injected by the tick handler as Ramulator's buffer drains.
    if (opcode == vp::WRITE)
    {
        // Write beat consumed (per-burst ack contract, io_v2.hpp "Write
        // acknowledgement"): copy the payload now, then take ownership.
        _this->traces.assert(req->allocator != nullptr,
            "write beat is not allocator-backed (req=%p) — unported master", req);
        if (data) memcpy(&_this->mem_data[offset], data, req_size);
        if (!burst_last)
        {
            // Non-last beat of a beat-form burst: never resp()'d — free it
            // back to its pool right away. Its Inflight stays behind as a
            // SILENT virtual ack that paces the queue exactly as its
            // per-beat ack did, but emits nothing upstream.
            infl->req = nullptr;
            infl->master_data = nullptr;
            req->free();
        }
        else
        {
            // Burst-final beat (a big-packet-form write is a one-beat
            // burst): consumed too, but parked for recycling as the burst's
            // single data-less ack in emit_one_beat.
            infl->owns_req = true;
        }
    }
    else if (opcode != vp::READ)
    {
        // Atomic opcode: exempt from the per-burst ack rules — classic
        // round-trip of the master's own object through resp(). Only the
        // store side is modeled (no read-modify-write / old-value return),
        // matching the previous behavior.
        if (data) memcpy(&_this->mem_data[offset], data, req_size);
    }
    else
    {
        infl->read_data.resize(req_size);
        memcpy(infl->read_data.data(), &_this->mem_data[offset], req_size);
    }
    _this->inflight_queue.push_back(infl);

    return vp::IO_REQ_GRANTED;
}


// Inject the burst's next not-yet-submitted transaction into Ramulator. Returns
// false if Ramulator's request buffer is full (its own back-pressure).
bool RamulatorModel::inject_one(Inflight *infl)
{
    uint64_t addr = infl->aligned_start + (uint64_t)infl->tx_injected * this->tx_bytes;
    int tx_bytes = this->tx_bytes;
    // On completion, advance this burst's fill counter so the tick handler can
    // emit more beats. Free the in-flight object once its last beat is emitted
    // AND all of its transactions are done.
    RamulatorModel *self = this;
    auto callback = [self, infl, tx_bytes](Ramulator::Request & /*r*/)
    {
        infl->tx_done++;
        infl->bytes_filled =
            std::min(infl->bytes_filled + (uint64_t)tx_bytes, infl->total_size);
        // A controller-buffer slot just freed: a denied master may now fit.
        self->slot_freed = true;
        if (infl->emitted_done && infl->tx_done == infl->tx_total)
        {
            delete infl;
        }
    };

    bool ok = this->frontend->receive_external_requests(
        infl->type, (Ramulator::Addr_t)addr, /*source_id=*/0, callback, this->tx_bytes);
    if (ok) infl->tx_injected++;
    return ok;
}


void RamulatorModel::pump_injections()
{
    // Feed pending transactions into Ramulator's controller queue, in burst
    // arrival order, until Ramulator's buffer is full (then stop — that is the
    // real back-pressure and there is no wrapper-side request FIFO).
    for (Inflight *infl : this->inflight_queue)
    {
        while (infl->tx_injected < infl->tx_total)
        {
            if (!this->inject_one(infl)) return;
        }
    }
}


void RamulatorModel::maybe_retry()
{
    // Re-admit a denied master only once a controller-buffer slot has actually
    // freed this cycle (so the re-sent request has a chance to be injected).
    if (this->retry_pending && this->slot_freed)
    {
        this->retry_pending = false;
        this->in_itf.retry();
    }
    this->slot_freed = false;
}


// Emit at most one beat per cycle for the head in-flight request, with
// is_first / is_last / burst_id set. Reads stream distinct data beats; a
// WRITE burst is acknowledged exactly once (per-burst ack) — every beat's
// slot in this per-cycle pacing is kept as a silent virtual tick, and only
// the final beat of the request whose submitted beat carried is_last
// materializes the resp(); atomics round-trip the master's own request.
void RamulatorModel::emit_one_beat()
{
    if (this->inflight_queue.empty()) return;
    Inflight *infl = this->inflight_queue.front();

    uint64_t avail = infl->bytes_filled - infl->bytes_emitted;
    if (avail == 0) return;

    uint64_t left = infl->total_size - infl->bytes_emitted;
    uint64_t beat_size = std::min({(uint64_t)this->beat_width, avail, left});

    bool is_first = (infl->bytes_emitted == 0);
    bool is_last = (infl->bytes_emitted + beat_size == infl->total_size);

    if (infl->type == Ramulator::Request::Type::Write
        && infl->opcode == vp::WRITE)
    {
        // WRITE: acked once per BURST (io_v2.hpp "Write acknowledgement").
        // The per-beat cursors, head-of-queue rule and 1-beat/cycle rate are
        // exactly what the former per-beat ack stream was — kept as silent
        // virtual ticks — so the single materialized ack below fires on the
        // cycle the last per-beat ack fired before the protocol change.
        // Pacing is per REQUEST: each beat-form beat has its own Inflight
        // and drains independently; only the request whose submitted beat
        // carried is_last (burst_last) materializes an ack, on its final
        // virtual tick.
        if (infl->burst_last && is_last)
        {
            // Release the parked burst-final beat and emit the single
            // data-less ack as a dedicated request. The initiator frees it.
            vp::IoReq *ack = vp::io_v2_write_ack(infl->req);
            infl->req = nullptr;
            infl->owns_req = false;
            // addr/size are informational on an ack. Beats of one burst are
            // not correlated across requests here (each request paces
            // independently), so they carry this request's own base / byte
            // count: the exact burst base / total for a big-packet-form
            // write (one-beat burst), the last beat's addr / size for a
            // beat-form burst.
            ack->set_addr(infl->burst_addr);
            ack->set_size(infl->total_size);
            ack->set_opcode(vp::WRITE);
            ack->is_first = true;
            ack->is_last = true;
            ack->burst_id = infl->burst_id;
            ack->initiator = infl->initiator;
            // Status: the only INVALID source (out-of-bound) is reported
            // inline at submit, so a parked beat always acks OK (prepare()
            // reset it above).

            this->trace.msg("Write burst ack RESP addr=0x%llx burst_id=%lld size=%llu\n",
                (unsigned long long)ack->addr, (long long)infl->burst_id,
                (unsigned long long)infl->total_size);

            this->in_itf.resp(ack);
        }
        else
        {
            this->trace.msg("Write virtual ack tick addr=0x%llx burst_id=%lld size=%llu (%llu/%llu, silent)\n",
                (unsigned long long)(infl->burst_addr + infl->bytes_emitted),
                (long long)infl->burst_id, (unsigned long long)beat_size,
                (unsigned long long)(infl->bytes_emitted + beat_size),
                (unsigned long long)infl->total_size);
        }
    }
    else if (infl->type == Ramulator::Request::Type::Write)
    {
        // Atomic opcode: classic round-trip — the master's own request comes
        // back through resp(), mutated per beat (exempt from the per-burst
        // write-ack rules, it carries response semantics).
        vp::IoReq *req = infl->req;
        req->addr = infl->burst_addr + infl->bytes_emitted;
        req->data = infl->master_data + infl->bytes_emitted;
        req->size = beat_size;
        req->burst_id = infl->burst_id;
        req->is_first = is_first;
        req->is_last = is_last;
        req->status = vp::IO_RESP_OK;

        this->trace.msg("Beat RESP addr=0x%llx burst_id=%lld size=%llu first=%d last=%d (%llu/%llu)\n",
            (unsigned long long)req->addr, (long long)infl->burst_id,
            (unsigned long long)beat_size, is_first, is_last,
            (unsigned long long)(infl->bytes_emitted + beat_size),
            (unsigned long long)infl->total_size);

        this->in_itf.resp(req);
    }
    else
    {
        // Read beat: distinct allocator-backed object whose co-allocated
        // payload carries the data slice; the terminal master copies it out
        // and frees it. The master's data-less burst request is never touched.
        vp::IoReq *req = this->beat_allocator->alloc();
        req->prepare();
        req->set_addr(infl->burst_addr + infl->bytes_emitted);
        req->set_size(beat_size);
        req->set_is_write(false);
        memcpy(req->get_data(), infl->read_data.data() + infl->bytes_emitted,
               beat_size);
        req->burst_id = infl->burst_id;
        req->is_first = is_first;
        req->is_last = is_last;
        req->initiator = infl->initiator;

        this->trace.msg("Beat RESP addr=0x%llx burst_id=%lld size=%llu first=%d last=%d (%llu/%llu)\n",
            (unsigned long long)req->addr, (long long)infl->burst_id,
            (unsigned long long)beat_size, is_first, is_last,
            (unsigned long long)(infl->bytes_emitted + beat_size),
            (unsigned long long)infl->total_size);

        this->in_itf.resp(req);
    }

    infl->bytes_emitted += beat_size;

    if (is_last)
    {
        this->inflight_queue.pop_front();
        infl->emitted_done = true;
        if (infl->tx_done == infl->tx_total)
        {
            delete infl;
        }
    }
}


void RamulatorModel::tick_handler(vp::Block *__this, vp::ClockEvent *event)
{
    RamulatorModel *_this = (RamulatorModel *)__this;

    // Push pending transactions into Ramulator (bounded by its buffer), advance
    // Ramulator one DRAM cycle (completion callbacks advance the fill counters),
    // emit one beat per cycle, then re-admit the master if a burst freed a slot.
    _this->pump_injections();
    _this->memory_system->tick();
    _this->emit_one_beat();
    _this->maybe_retry();
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RamulatorModel(config);
}
