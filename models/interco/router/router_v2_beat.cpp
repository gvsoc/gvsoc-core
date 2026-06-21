// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Beat-streaming address-decoding router on the io_v2 protocol.
 *
 * Wire shapes (per the io_v2 read-asymmetry convention):
 *   - Reads:  exactly one forward req() per burst, with size=total_burst_bytes,
 *             is_first=is_last=true. The response is a stream of beats. The
 *             framework auto-inserts an IoV2BeatAdapter between this router's
 *             outbound IoMaster and any downstream IoV2BigPacket slave, so the
 *             response stream is uniform per-beat regardless of the slave's
 *             actual response form (sync DONE, async big-packet, native beat
 *             stream). The router emits one upstream resp() per response beat;
 *             the slot stays alive until the response stream's is_last.
 *   - Writes: N forward req() beats per burst (size <= width each), with
 *             per-beat is_first/is_last/burst_id from the master. Each forward
 *             produces exactly one upstream resp() (the upstream slave answers
 *             one beat per req for writes).
 *
 * Per-burst routing table: a fixed-size table (`max_pending_bursts` entries).
 * On is_first of a burst the input allocates a slot; subsequent beats (writes
 * only) reuse it. The slot holds the originating input, the translated output,
 * the burst's direction-channel and the master's original burst_id. On forward
 * we remap beat.burst_id to the slot index; the downstream slave (or auto-
 * inserted adapter) echoes it back on response; the response handler looks up
 * the slot in O(1) via the remapped id.
 *
 * Master-side is_last preservation: when the framework auto-inserts an
 * IoV2BeatAdapter on the path, the adapter mutates ``req->is_last`` per resp()
 * to reflect position within the response stream. That destroys the master's
 * per-forward is_last (which the router needs to decide when a write burst is
 * done). We snapshot ``req->is_last`` per forward into the slot's
 * ``pending_master_is_last`` deque and pop it on the matching resp.
 *
 * Throughput: 1 forward beat per cycle per (output, channel) on the forward
 * side; 1 response beat per cycle per output on the response side (paced by
 * the adapter). Burst atomicity: an output channel locked to an input on
 * is_first forward stays locked until is_last response.
 */

#include <climits>
#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/stats/stats.hpp>
#include <vp/mapping_tree.hpp>
#include <vp/clocked_signal.hpp>
#include <vp/signal.hpp>
#include <vp/proxy.hpp>
#include <interco/router_v2/router_config.hpp>

#include "proxy_command.hpp"
#include "router_v2_debug.hpp"

class RouterBeat;
class InputPort;
class OutputPort;

// Number of independent forward channels per output. Indexed [0]=read, [1]=write
// when ``cfg.shared_rw_channel`` is false; both directions collapse onto [0]
// when true (the helper ``RouterBeat::channel_of`` performs the mapping).
static constexpr int NB_CHANNELS = 2;

struct BurstEntry
{
    bool in_use = false;
    InputPort *input = nullptr;
    int output_id = -1;                // -1 until fsm has decoded the first beat's mapping
    int channel = 0;                   // resolved direction-channel for this burst
    int64_t original_burst_id = -1;    // master's burst_id, restored on response
    // Snapshot of req->is_last as the master submitted each forward beat into
    // this slot. Used to detect burst completion in the resp handler because
    // an auto-inserted IoV2BeatAdapter may overwrite req->is_last with the
    // per-response-beat value. Pop on each resp whose resp_is_last_of_chunk
    // is true; peek otherwise (multi-beat read response).
    std::deque<bool> pending_master_is_last;
};

class OutputPort
{
public:
    OutputPort(RouterBeat *top, int id, std::string name);

    void log_access(uint64_t addr, uint64_t size, bool is_write,
                    bool is_first, bool is_last);
    void log_resp(uint64_t addr, uint64_t size, bool is_first, bool is_last);

    RouterBeat *top;
    int id;
    // Bus-facing master. The framework auto-inserts an IoV2BeatAdapter
    // downstream when this master's signature (IoV2Beat) differs from the
    // bound slave's signature (IoV2BigPacket). resp() / retry() are dispatched
    // to RouterBeat via the muxed callbacks, keyed by ``id``.
    vp::IoMaster bus;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    // Input currently holding this output for a burst, per channel; nullptr when
    // the channel is free. With shared_rw_channel=true only [0] is used.
    InputPort *elected_input[NB_CHANNELS] = {nullptr, nullptr};
    // Downstream returned DENIED for the most recent forward on a channel;
    // waiting for retry(). Stall is per (output, channel) so a back-pressured
    // write does not block reads to the same output (and vice versa). With
    // shared_rw_channel only [0] is ever used.
    bool stalled[NB_CHANNELS] = {false, false};
    // Input that owns the single beat denied on this (output, channel) while
    // stalled. The denied beat is that input's pending.front(); on retry() we
    // re-issue it synchronously (see retry_muxed). At most one beat is stalled
    // per (output, channel): the FSM skips a stalled channel, and one input's
    // in-order FIFO front is a single beat (one direction), so this never
    // aliases.
    InputPort *stalled_input[NB_CHANNELS] = {nullptr, nullptr};
    // Per-mapping VCD traces split into request and response sub-trees.
    // req/* logs every outgoing forward — read setup reqs and per-beat writes.
    // resp/* logs every read beat returned upstream (writes get no per-resp
    // beats in the beat-stream form).
    vp::Signal<uint64_t> req_addr;
    vp::Signal<uint64_t> req_size;
    vp::Signal<uint64_t> req_is_write;
    vp::Signal<uint64_t> req_is_first;
    vp::Signal<uint64_t> req_is_last;
    vp::Signal<uint64_t> resp_addr;
    vp::Signal<uint64_t> resp_size;
    vp::Signal<uint64_t> resp_is_first;
    vp::Signal<uint64_t> resp_is_last;
    // Explicit per-mapping busy bit: pulsed for one cycle on every log_access
    // or log_resp on this output. Used as the "busy strip" path of the
    // mapping's row in the GUI.
    vp::Signal<uint64_t> active;
    int64_t last_logged_req = -1;
    int nb_logged_req_in_same_cycle = 0;
    int64_t last_logged_resp = -1;
    int nb_logged_resp_in_same_cycle = 0;
};

class InputPort
{
public:
    InputPort(RouterBeat *top, int id, std::string name);
    RouterBeat *top;
    int id;
    vp::IoSlave itf;
    // One entry per queued beat. Each beat carries the slot it was assigned in
    // req_muxed so the fsm doesn't need any per-input "current burst slot"
    // lookup; multiple bursts can coexist in the queue.
    struct PendingBeat
    {
        vp::IoReq *req;
        int        slot_idx;
    };
    std::deque<PendingBeat> pending;
    uint64_t pending_bytes = 0;
    // Cycle-latched "head arrival" gate — beats pushed in cycle T are only
    // visible to an fsm running at cycle T+1.
    vp::ClockedSignal<int64_t> head_cycle;
    // Tracks the in-progress multi-beat burst on this input (between its
    // is_first and is_last beats). -1 when no such burst is open. Used only by
    // continuation beats (is_first=false) to look up their slot; single-beat
    // bursts (is_first=is_last=true) don't touch it and can pipeline freely
    // alongside others. Reads and writes use it symmetrically — io_v2 lets a
    // master send a multi-beat read as N reqs the same way it does for writes.
    int active_multi_beat_slot = -1;
    // Set when handle_req denied the master (FIFO full or no burst slot). Cleared
    // and propagated to the master via retry() when the blocking condition clears.
    bool denied_upstream = false;
};

class RouterBeat : public vp::Component, public vp::DebugMemIf
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterBeat(vp::ComponentConf &conf);
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file,
        FILE *reply_file, std::vector<std::string> args,
        std::string cmd_req) override;

    // Backdoor debug access (proxy mem_read/mem_write, GDB) — resolved
    // through the lazily-built flat map, bypassing the timed path entirely.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void retry_muxed(vp::Block *__this, int port, vp::IoRetryChannel channel);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // Flat backdoor map, built lazily on first debug access
    vp::DebugMemMap debug_map;

    void schedule_fsm();
    // Forward one committed beat (front of `in`'s FIFO, already assigned to
    // `out`/`slot_idx`) to its output: translate addr, remap burst_id, snapshot
    // is_last, and submit. On GRANTED, pop the beat and account it; on DENIED,
    // fully roll the beat back so it stays re-issuable at the FIFO head. The
    // caller owns the stall/output_used/wake bookkeeping. Shared by the FSM
    // arbitration sweep and the synchronous retry path.
    vp::IoReqStatus forward_beat(InputPort *in, OutputPort *out, int slot_idx,
                                 vp::IoReq *beat);
    int alloc_burst_slot();
    void free_burst_slot(int slot_idx);
    int channel_of(bool is_write) const
    {
        return this->cfg.shared_rw_channel ? 0 : (is_write ? 1 : 0);
    }
    // Channel indices a retry on `channel` should re-issue: ANY -> both;
    // READ/WRITE -> the matching index, collapsed to 0 when shared_rw_channel.
    // Fills `out` (capacity NB_CHANNELS) and returns the count.
    int channels_for(vp::IoRetryChannel channel, int *out) const
    {
        if (this->cfg.shared_rw_channel) { out[0] = 0; return 1; }
        if (channel == vp::IO_RETRY_READ)  { out[0] = 0; return 1; }
        if (channel == vp::IO_RETRY_WRITE) { out[0] = 1; return 1; }
        out[0] = 0; out[1] = 1; return 2;  // IO_RETRY_ANY
    }
    void wake_denied_masters();

    RouterConfig cfg;
    int max_pending_bursts;
    vp::MappingTree mapping_tree;
    int error_id = -1;
    std::vector<InputPort *> inputs;
    std::vector<OutputPort *> entries;
    std::vector<BurstEntry> burst_table;
    vp::ClockEvent fsm_event;
    int round_robin_next = 0;

    vp::Trace trace;

    // Top-level busy bit: pulsed for one cycle whenever any output port
    // logs an access or a response. Used as the path for the router row's
    // busy strip in the GUI.
    vp::Signal<uint64_t> active;

    vp::StatScalar stat_reads;
    vp::StatScalar stat_writes;
    vp::StatScalar stat_bytes_read;
    vp::StatScalar stat_bytes_written;
    vp::StatScalar stat_errors;
};


//
// OutputPort / InputPort
//

OutputPort::OutputPort(RouterBeat *top, int id, std::string name)
    : top(top), id(id),
      bus(id, &RouterBeat::retry_muxed, &RouterBeat::resp_muxed),
      req_addr(*top, name + "/req/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      req_size(*top, name + "/req/size", 64, vp::SignalCommon::ResetKind::HighZ),
      req_is_write(*top, name + "/req/is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      req_is_first(*top, name + "/req/is_first", 1, vp::SignalCommon::ResetKind::HighZ),
      req_is_last(*top, name + "/req/is_last", 1, vp::SignalCommon::ResetKind::HighZ),
      resp_addr(*top, name + "/resp/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      resp_size(*top, name + "/resp/size", 64, vp::SignalCommon::ResetKind::HighZ),
      resp_is_first(*top, name + "/resp/is_first", 1, vp::SignalCommon::ResetKind::HighZ),
      resp_is_last(*top, name + "/resp/is_last", 1, vp::SignalCommon::ResetKind::HighZ),
      active(*top, name + "/active", 1, vp::SignalCommon::ResetKind::HighZ)
{
}

void OutputPort::log_access(uint64_t addr, uint64_t size, bool is_write,
                            bool is_first, bool is_last)
{
    int64_t cycles = this->top->clock.get_cycles();
    if (cycles > this->last_logged_req)
        this->nb_logged_req_in_same_cycle = 0;

    int64_t period = this->top->clock.get_period();
    int64_t delay = 0;
    if (this->nb_logged_req_in_same_cycle > 0)
    {
        delay = period - (period >> this->nb_logged_req_in_same_cycle);
    }
    // Dump value now and high-Z at +1 cycle. set_and_release() would defer the
    // high-Z via dump_highz_next, which only fires on the next clock tick;
    // when the router is idle for many cycles between events that deferred
    // dump never lands and the value visually persists. release() with a
    // non-zero time_delay dumps high-Z immediately into the trace buffer at
    // the future timestamp, independent of clock ticks.
    this->req_addr.set(addr, (int64_t)0, delay);
    this->req_addr.release(0, delay + period);
    this->req_size.set(size, (int64_t)0, delay);
    this->req_size.release(0, delay + period);
    this->req_is_write.set(is_write ? 1 : 0, (int64_t)0, delay);
    this->req_is_write.release(0, delay + period);
    this->req_is_first.set(is_first ? 1 : 0, (int64_t)0, delay);
    this->req_is_first.release(0, delay + period);
    this->req_is_last.set(is_last ? 1 : 0, (int64_t)0, delay);
    this->req_is_last.release(0, delay + period);
    // Pulse the per-mapping and top-level busy bits for one cycle.
    this->active.set((uint64_t)1, (int64_t)0, delay);
    this->active.release(0, delay + period);
    this->top->active.set((uint64_t)1, (int64_t)0, delay);
    this->top->active.release(0, delay + period);
    this->nb_logged_req_in_same_cycle++;
    this->last_logged_req = cycles;
}

void OutputPort::log_resp(uint64_t addr, uint64_t size,
                          bool is_first, bool is_last)
{
    int64_t cycles = this->top->clock.get_cycles();
    if (cycles > this->last_logged_resp)
        this->nb_logged_resp_in_same_cycle = 0;

    int64_t period = this->top->clock.get_period();
    int64_t delay = 0;
    if (this->nb_logged_resp_in_same_cycle > 0)
    {
        delay = period - (period >> this->nb_logged_resp_in_same_cycle);
    }
    // See log_access — use explicit release(time_delay=delay+period) so the
    // high-Z lands in the trace at the right time even when the router has no
    // clock event at +1 cycle (which is the case for the gap between the
    // first read-response beat and the second).
    this->resp_addr.set(addr, (int64_t)0, delay);
    this->resp_addr.release(0, delay + period);
    this->resp_size.set(size, (int64_t)0, delay);
    this->resp_size.release(0, delay + period);
    this->resp_is_first.set(is_first ? 1 : 0, (int64_t)0, delay);
    this->resp_is_first.release(0, delay + period);
    this->resp_is_last.set(is_last ? 1 : 0, (int64_t)0, delay);
    this->resp_is_last.release(0, delay + period);
    // Pulse the per-mapping and top-level busy bits for one cycle.
    this->active.set((uint64_t)1, (int64_t)0, delay);
    this->active.release(0, delay + period);
    this->top->active.set((uint64_t)1, (int64_t)0, delay);
    this->top->active.release(0, delay + period);
    this->nb_logged_resp_in_same_cycle++;
    this->last_logged_resp = cycles;
}

InputPort::InputPort(RouterBeat *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterBeat::req_muxed),
      head_cycle(*top, name + "/head_cycle", 64, true, /*reset=*/INT64_MAX)
{
}


//
// RouterBeat construction
//

RouterBeat::RouterBeat(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      mapping_tree(&this->trace),
      fsm_event(this, &RouterBeat::fsm_handler),
      active(*this, "active", 1, vp::SignalCommon::ResetKind::HighZ)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->stats.register_stat(&this->stat_reads, "reads", "Number of read requests");
    this->stats.register_stat(&this->stat_writes, "writes", "Number of write beats");
    this->stats.register_stat(&this->stat_bytes_read, "bytes_read", "Total bytes read");
    this->stats.register_stat(&this->stat_bytes_written, "bytes_written", "Total bytes written");
    this->stats.register_stat(&this->stat_errors, "errors", "Beats rejected due to size or mapping");

    if (this->cfg.width <= 0)
    {
        this->trace.fatal("router_v2_beat needs a positive `width`\n");
    }
    if (this->cfg.max_input_pending_size == 0)
    {
        this->cfg.max_input_pending_size = (uint64_t)this->cfg.width;
    }
    this->max_pending_bursts = this->cfg.max_pending_bursts;
    if (this->max_pending_bursts <= 0)
    {
        // Default: enough for one burst per input.
        this->max_pending_bursts = this->cfg.nb_input_port > 0 ? this->cfg.nb_input_port : 1;
    }

    this->burst_table.resize(this->max_pending_bursts);

    this->inputs.resize(this->cfg.nb_input_port);
    for (int i = 0; i < this->cfg.nb_input_port; i++)
    {
        std::string name = i == 0 ? "input" : "input_" + std::to_string(i);
        InputPort *in = new InputPort(this, i, name);
        this->inputs[i] = in;
        this->new_slave_port(name, &in->itf, this);
    }

    for (int mapping_id = 0; mapping_id < (int)this->cfg.mappings_count; mapping_id++)
    {
        const RouterMapping &m = this->cfg.mappings[mapping_id];
        std::string name = m.name ? m.name : "";

        this->mapping_tree.insert(mapping_id, name, m.base, m.size);

        OutputPort *out = new OutputPort(this, mapping_id, name);
        out->remove_offset = m.remove_offset;
        out->add_offset = m.add_offset;
        this->entries.push_back(out);
        // Bind the per-output bus master to the named port. resp() and retry()
        // dispatch back through RouterBeat::resp_muxed / retry_muxed keyed by
        // the mapping id (set as the master's mux_id in OutputPort's ctor).
        this->new_master_port(name, &out->bus, this);

        if (m.is_error) this->error_id = mapping_id;
    }
    this->mapping_tree.build();
}

int RouterBeat::alloc_burst_slot()
{
    for (int i = 0; i < (int)this->burst_table.size(); i++)
    {
        if (!this->burst_table[i].in_use)
        {
            this->burst_table[i].in_use = true;
            return i;
        }
    }
    return -1;
}

void RouterBeat::free_burst_slot(int slot_idx)
{
    BurstEntry &slot = this->burst_table[slot_idx];
    slot.in_use = false;
    slot.input = nullptr;
    slot.output_id = -1;
    slot.original_burst_id = -1;
    slot.pending_master_is_last.clear();
}

void RouterBeat::schedule_fsm()
{
    if (!this->fsm_event.is_enqueued())
    {
        this->fsm_event.enqueue(1);
    }
}

void RouterBeat::wake_denied_masters()
{
    for (auto *in : this->inputs)
    {
        if (in->denied_upstream)
        {
            in->denied_upstream = false;
            in->itf.retry();
        }
    }
}

vp::IoReqStatus RouterBeat::forward_beat(InputPort *in, OutputPort *out,
                                         int slot_idx, vp::IoReq *beat)
{
    BurstEntry &slot = this->burst_table[slot_idx];

    // Translate addr, remap burst_id to slot index. We capture originals only
    // for the rollback-on-DENIED path; on accepted forward the response side
    // restores burst_id from slot.original_burst_id.
    uint64_t original_addr = beat->get_addr();
    int64_t original_burst_id = beat->burst_id;
    out->log_access(original_addr, beat->get_size(), beat->get_is_write(),
                    beat->is_first, beat->is_last);
    beat->set_addr(original_addr - out->remove_offset + out->add_offset);
    beat->burst_id = slot_idx;

    // Snapshot the master's is_last so the resp handler can detect burst
    // completion. The downstream (or auto-inserted IoV2BeatAdapter) is free to
    // overwrite req->is_last with a per-response-beat value, so we cannot rely
    // on reading it back from the request later.
    slot.pending_master_is_last.push_back(beat->is_last);

    vp::IoReqStatus st = out->bus.req(beat);
    // An IoV2Beat-side master never surfaces IO_REQ_DONE: an auto-inserted
    // IoV2BeatAdapter converts inline DONE into a scheduled beat-callback
    // stream, and a directly-bound IoV2Beat slave is required to respond
    // asynchronously per beat. Assert defensively in case the contract is
    // violated at runtime.
    vp_assert(st != vp::IO_REQ_DONE, &this->trace,
        "IoV2Beat master must not surface IO_REQ_DONE\n");

    if (st == vp::IO_REQ_GRANTED)
    {
        in->pending.pop_front();
        // Mirror the directional accounting from req_muxed.
        in->pending_bytes -= beat->get_is_write() ? beat->get_size() : 0;
        if (in->pending.empty()) in->head_cycle.set(INT64_MAX);
        // Slot stays alive until the resp handler fires for the burst's is_last.
    }
    else // IO_REQ_DENIED
    {
        // Roll back the forward: beat stays at head of FIFO with original
        // addr/burst_id so it can be re-issued verbatim. The caller stalls the
        // output until retry().
        beat->set_addr(original_addr);
        beat->burst_id = original_burst_id;
        slot.pending_master_is_last.pop_back();
    }

    return st;
}


//
// Forward path
//

vp::IoReqStatus RouterBeat::req_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterBeat *_this = (RouterBeat *)__this;
    InputPort *in = _this->inputs[port];
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();
    int64_t now = _this->clock.get_cycles();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Beat arrived (input: %d, addr: 0x%lx, size: %lu, write: %d, burst_id: %ld, first: %d, last: %d)\n",
        port, req->get_addr(), size, is_write ? 1 : 0,
        (long)req->burst_id, req->is_first ? 1 : 0, req->is_last ? 1 : 0);

    // Size check is direction-asymmetric: a write beat fits in `width`, but a
    // read request carries the full burst size and may exceed `width` — the
    // response is then chunked into beats by the downstream adapter.
    if (is_write && size > (uint64_t)_this->cfg.width)
    {
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // Protocol checks. Multiple bursts may coexist on the same input, just
    // like multiple outstanding AXI ARs / AWs. The only constraint is that
    // continuation beats of a multi-beat burst must follow its is_first beat
    // without another multi-beat burst interleaving — that's tracked via
    // active_multi_beat_slot.
    if (!req->is_first && in->active_multi_beat_slot == -1)
    {
        // Continuation beat without a started multi-beat burst.
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // FIFO overflow check (before slot allocation — we don't want to consume a
    // slot and then fail on FIFO). Only write beats consume FIFO bytes: their
    // payload sits in the queue until the fsm forwards it. Read reqs carry
    // only the descriptor (the data streams back later as response beats), so
    // their `size` field (the full burst length) MUST NOT be charged against
    // the input FIFO — otherwise a 4 KB read against a width=32 router would
    // always exceed the default cap and deadlock.
    uint64_t fifo_cost = is_write ? size : 0;
    if (in->pending_bytes + fifo_cost > _this->cfg.max_input_pending_size)
    {
        in->denied_upstream = true;
        return vp::IO_REQ_DENIED;
    }

    // Determine the slot this beat belongs to.
    int slot_idx;
    if (req->is_first)
    {
        // New burst: allocate a fresh slot. If the table is full, deny the
        // beat; we'll wake the master when a slot frees.
        slot_idx = _this->alloc_burst_slot();
        if (slot_idx == -1)
        {
            in->denied_upstream = true;
            return vp::IO_REQ_DENIED;
        }
        BurstEntry &slot = _this->burst_table[slot_idx];
        slot.input = in;
        slot.output_id = -1;                          // decoded later by fsm
        slot.channel = _this->channel_of(is_write);
        slot.original_burst_id = req->burst_id;

        // Lock the input only while a multi-beat burst is mid-stream so its
        // continuation beats can find their slot. Single-beat bursts
        // (is_first=is_last=true) don't need any locking — multiple of them
        // can pipeline freely on the same input alongside one open
        // multi-beat burst. Reads and writes are symmetric here: io_v2 lets a
        // master spread a multi-beat read across N reqs the same way as a
        // multi-beat write.
        if (!req->is_last)
        {
            in->active_multi_beat_slot = slot_idx;
        }
    }
    else
    {
        // Continuation beat of the in-progress multi-beat burst on this input.
        slot_idx = in->active_multi_beat_slot;
        if (req->is_last)
        {
            // Last beat of the multi-beat burst — the input is free to start
            // another multi-beat burst as soon as this one is queued.
            in->active_multi_beat_slot = -1;
        }
    }

    if (is_write) { _this->stat_writes++; _this->stat_bytes_written += size; }
    else          { _this->stat_reads++;  _this->stat_bytes_read    += size; }

    bool was_empty = in->pending.empty();
    in->pending.push_back(InputPort::PendingBeat{req, slot_idx});
    in->pending_bytes += fifo_cost;
    if (was_empty)
    {
        in->head_cycle.set(now);
    }

    _this->schedule_fsm();
    return vp::IO_REQ_GRANTED;
}

void RouterBeat::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    RouterBeat *_this = (RouterBeat *)__this;
    int64_t now = _this->clock.get_cycles();
    int n = (int)_this->inputs.size();

    // Per-(output, channel) "already used this cycle" tracker. With
    // shared_rw_channel=false an output can carry one R beat AND one W beat
    // per cycle (entries [0] and [1]); with shared_rw_channel=true only [0]
    // is touched for both directions.
    std::vector<std::array<bool, NB_CHANNELS>> output_used(
        _this->entries.size(), {false, false});

    for (int k = 0; k < n; k++)
    {
        int i = (_this->round_robin_next + k) % n;
        InputPort *in = _this->inputs[i];
        if (in->pending.empty()) continue;

        // ClockedSignal gate: head must have been committed to a prior cycle.
        if (in->head_cycle.get() >= now) continue;

        InputPort::PendingBeat head = in->pending.front();
        vp::IoReq *beat = head.req;
        int slot_idx = head.slot_idx;
        BurstEntry &slot = _this->burst_table[slot_idx];

        // First time the fsm sees this burst: decode the mapping.
        if (slot.output_id == -1)
        {
            vp::MappingTreeEntry *mapping = _this->mapping_tree.get(
                beat->get_addr(), beat->get_size(), beat->get_is_write());
            bool straddles = mapping && mapping->size != 0 &&
                beat->get_addr() + beat->get_size() > mapping->base + mapping->size;

            if (!mapping || mapping->id == _this->error_id || straddles ||
                !_this->entries[mapping->id]->bus.is_bound())
            {
                _this->stat_errors++;
                beat->set_resp_status(vp::IO_RESP_INVALID);
                in->pending.pop_front();
                // Mirror the directional accounting from req_muxed.
                in->pending_bytes -= beat->get_is_write() ? beat->get_size() : 0;
                if (in->pending.empty()) in->head_cycle.set(INT64_MAX);
                // Free the burst (it never actually got routed).
                _this->free_burst_slot(slot_idx);
                // If this beat opened a multi-beat burst that's now dead,
                // also clear the input's in-progress tracker so a future
                // multi-beat burst can start.
                if (in->active_multi_beat_slot == slot_idx)
                {
                    in->active_multi_beat_slot = -1;
                }
                in->itf.resp(beat);
                _this->wake_denied_masters();
                continue;
            }
            slot.output_id = mapping->id;
        }

        OutputPort *out = _this->entries[slot.output_id];
        int ch = slot.channel;

        if (out->stalled[ch]) continue;
        if (out->elected_input[ch] != nullptr && out->elected_input[ch] != in) continue;
        if (output_used[slot.output_id][ch]) continue;

        // Lock output-channel to this input on is_first (writes) or on the
        // single read forward (reads).
        if (beat->is_first)
        {
            out->elected_input[ch] = in;
        }

        // Forward the committed beat. On success mark the output-channel used
        // this cycle and wake any FIFO-denied masters; on DENIED stall the
        // output and remember this input so retry() can re-issue the beat
        // synchronously without re-arbitrating.
        vp::IoReqStatus st = _this->forward_beat(in, out, slot_idx, beat);
        if (st == vp::IO_REQ_GRANTED)
        {
            output_used[slot.output_id][ch] = true;
            _this->wake_denied_masters();
        }
        else // IO_REQ_DENIED
        {
            out->stalled[ch] = true;
            out->stalled_input[ch] = in;
        }
    }

    _this->round_robin_next = (_this->round_robin_next + 1) % n;

    bool any_pending = false;
    for (auto *in : _this->inputs)
    {
        if (!in->pending.empty()) { any_pending = true; break; }
    }
    if (any_pending)
    {
        _this->schedule_fsm();
    }
}


//
// Response path
//
// The framework auto-inserts an IoV2BeatAdapter on the downstream side when
// the slave's signature is IoV2BigPacket. Either way, the resp() arriving here
// already carries per-response-beat is_first/is_last/data/size/status; only
// the master's per-forward is_last is reconstructed from the slot's pending
// FIFO (see BurstEntry::pending_master_is_last).
//

void RouterBeat::resp_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterBeat *_this = (RouterBeat *)__this;
    OutputPort *self = _this->entries[port];

    // The forward path stashed slot_idx in req->burst_id. The downstream
    // adapter (when present) snapshots burst_id at schedule time and restores
    // it per beat, so this is robust across cascaded beat-aware routers.
    int slot_idx = (int)req->burst_id;
    BurstEntry &slot = _this->burst_table[slot_idx];

    // Per-response-beat is_last (from the adapter or a native beat slave) —
    // tells us whether the current in-flight request's response stream is
    // complete. Distinct from the master's per-forward is_last (stored in
    // slot.pending_master_is_last); both must be true for the burst itself
    // to be done.
    bool resp_is_last_of_chunk = req->is_last;
    bool master_is_last;
    if (resp_is_last_of_chunk)
    {
        master_is_last = slot.pending_master_is_last.front();
        slot.pending_master_is_last.pop_front();
    }
    else
    {
        master_is_last = slot.pending_master_is_last.front();
    }
    bool burst_done = resp_is_last_of_chunk && master_is_last;

    InputPort *in = slot.input;

    // Restore burst_id to the master's original (everything else — data,
    // size, is_first, is_last, status — is already the per-beat value the
    // upstream master expects).
    req->burst_id = slot.original_burst_id;

    if (burst_done)
    {
        self->elected_input[slot.channel] = nullptr;
        // The input's in-progress tracker (active_multi_beat_slot) was
        // already cleared by req_muxed at queue-time on the master's is_last
        // beat — the input could start a new multi-beat burst before this
        // response even came back, AXI-style. So nothing to clear here.
        _this->free_burst_slot(slot_idx);
    }

    // Reads only: writes don't get a per-beat response stream — they finish
    // inline on the forward. Reverse-translate addr back to master coord so
    // the resp/* trace lines up with req/* in the GUI.
    if (!req->get_is_write())
    {
        uint64_t master_addr =
            req->get_addr() - self->add_offset + self->remove_offset;
        self->log_resp(master_addr, req->get_size(),
                       req->is_first, req->is_last);
    }

    in->itf.resp(req);

    // No need to restore req->burst_id between beats: the downstream adapter
    // (or beat-aware slave) resets it from its per-beat snapshot before the
    // next resp() fires.

    if (burst_done)
    {
        _this->wake_denied_masters();
        _this->schedule_fsm();
    }
}

void RouterBeat::retry_muxed(vp::Block *__this, int port, vp::IoRetryChannel channel)
{
    RouterBeat *_this = (RouterBeat *)__this;
    OutputPort *self = _this->entries[port];

    _this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Output unstalled by downstream retry (output: %d, channel: %d)\n",
        self->id, (int)channel);

    // io_v2 requires retry() to be serviced synchronously: re-issue the beat(s)
    // that were denied on this output *now*, in the same call, not on a later
    // FSM tick. Some downstream slaves (e.g. log_ico_v2) only keep their
    // inline-forward window open for the duration of this retry() call, so
    // deferring the re-issue would deadlock.
    //
    // The retry names a channel (READ/WRITE/ANY); we re-issue only the stalled
    // channels it covers. At most one beat is stalled per (output, channel) and
    // it is exactly that channel's owner input front beat, so no re-arbitration
    // is needed — the rest of the queue resumes on the next FSM tick.
    int chans[NB_CHANNELS];
    int nb = _this->channels_for(channel, chans);
    for (int k = 0; k < nb; k++)
    {
        int c = chans[k];
        if (!self->stalled[c]) continue;
        self->stalled[c] = false;
        InputPort *in = self->stalled_input[c];
        self->stalled_input[c] = nullptr;
        // Copy the head: forward_beat pops the front on success.
        InputPort::PendingBeat head = in->pending.front();
        vp::IoReqStatus st = _this->forward_beat(in, self, head.slot_idx, head.req);
        if (st == vp::IO_REQ_DENIED)
        {
            // Slave filled up again between retry() and our re-send; stay
            // stalled on this channel and wait for the next retry().
            self->stalled[c] = true;
            self->stalled_input[c] = in;
        }
        else
        {
            // Accepted: a FIFO byte may have freed, so let denied masters back in.
            _this->wake_denied_masters();
        }
    }
    // Resume normal 1-beat/cycle pacing for whatever else is queued (other
    // inputs that were blocked on this output, the next beats).
    _this->schedule_fsm();
}


//
// Proxy command path
//

int RouterBeat::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
    bool is_write)
{
    if (!this->debug_map.is_built())
    {
        this->debug_map.build(this);
    }
    return this->debug_map.access(addr, data, size, is_write);
}

void RouterBeat::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    vp_router_v2_debug::collect_regions(this->cfg, this->error_id,
        [this](int id) -> vp::MasterPort * { return &this->entries[id]->bus; },
        regions, local_base, window_size, entry_base, depth);
}

std::string RouterBeat::handle_command(gv::GvProxy *proxy, FILE *req_file,
    FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    return vp_router_v2_proxy::handle_proxy_command(
        proxy, req_file, reply_file, args, cmd_req, this);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterBeat(config);
}
