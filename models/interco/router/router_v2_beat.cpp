// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Beat-streaming address-decoding router on the io_v2 protocol.
 *
 * Each incoming IoReq is one beat of size <= `width` bytes. A burst is a sequence of
 * beats sharing one `burst_id`, with `is_first` on the first and `is_last` on the last.
 *
 * Per-burst routing table: the router has a fixed-size table (`max_pending_bursts`
 * entries). On the first beat of a burst, a slot is allocated; subsequent beats use
 * the same slot. The slot holds everything the response path needs: originating
 * input, translated output, and the master's original burst_id. On forward, we remap
 * beat.burst_id to the slot index; downstream echoes it back on response; resp_muxed
 * looks up the slot directly by req.burst_id. No per-beat state, no InFlight.
 *
 * Throughput: 1 beat/cycle/output.
 * Scheduling: single ClockEvent (fsm_event). Arbitration and forwarding happen in
 * one pass, gated by a per-input ClockedSignal<int64_t> head_cycle that ensures
 * beats pushed during cycle T are only visible to an fsm running at cycle T+1
 * (regardless of event ordering).
 *
 * Burst atomicity: once an output is elected to carry an input's burst, it stays
 * locked to that input until the is_last response arrives.
 */

#include <climits>
#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/stats/stats.hpp>
#include <vp/mapping_tree.hpp>
#include <vp/clocked_signal.hpp>
#include <interco/router_config.hpp>

class RouterBeat;
class InputPort;

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
};

class OutputPort
{
public:
    OutputPort(RouterBeat *top, int id, std::string name);
    RouterBeat *top;
    int id;
    vp::IoMaster itf;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    // Input currently holding this output for a burst, per channel; nullptr when
    // the channel is free. With shared_rw_channel=true only [0] is used.
    InputPort *elected_input[NB_CHANNELS] = {nullptr, nullptr};
    // Downstream returned DENIED for the most recent forward; waiting for retry().
    // Stall is per-output (single downstream IoMaster), not per-channel.
    bool stalled = false;
};

class InputPort
{
public:
    InputPort(RouterBeat *top, int id, std::string name);
    RouterBeat *top;
    int id;
    vp::IoSlave itf;
    // Pending beats FIFO.
    std::deque<vp::IoReq *> pending;
    uint64_t pending_bytes = 0;
    // Cycle-latched "head arrival" gate. See router_async_v2 docs.
    vp::ClockedSignal<int64_t> head_cycle;
    // Currently active burst slot (-1 if no burst in flight for this input). One
    // burst per input at a time.
    int active_burst_slot = -1;
    // Set when handle_req denied the master (FIFO full or no burst slot). Cleared
    // and propagated to the master via retry() when the blocking condition clears.
    bool denied_upstream = false;
};

class RouterBeat : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterBeat(vp::ComponentConf &conf);

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int id);
    static void retry_muxed(vp::Block *__this, int id);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    void schedule_fsm();
    int alloc_burst_slot();
    void free_burst_slot(int slot_idx);
    // Map a request direction to an output-channel index. Returns 0 for both
    // reads and writes when ``shared_rw_channel`` is true.
    int channel_of(bool is_write) const
    {
        return this->cfg.shared_rw_channel ? 0 : (is_write ? 1 : 0);
    }
    // Called when a blocking condition clears (FIFO drained, slot freed). Fires
    // retry() on any input that had been denied upstream.
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
      itf(id, &RouterBeat::retry_muxed, &RouterBeat::resp_muxed)
{
}

InputPort::InputPort(RouterBeat *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterBeat::req_muxed),
      head_cycle(*top, name + "/head_cycle", 64, true, /*reset=*/INT64_MAX)
{
}


//
// RouterBeat
//

RouterBeat::RouterBeat(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      mapping_tree(&this->trace),
      fsm_event(this, &RouterBeat::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->stats.register_stat(&this->stat_reads, "reads", "Number of read beats");
    this->stats.register_stat(&this->stat_writes, "writes", "Number of write beats");
    this->stats.register_stat(&this->stat_bytes_read, "bytes_read", "Total bytes read");
    this->stats.register_stat(&this->stat_bytes_written, "bytes_written", "Total bytes written");
    this->stats.register_stat(&this->stat_errors, "errors", "Beats rejected due to size or mapping");

    if (this->cfg.width <= 0)
    {
        this->trace.fatal("router_async_v2 needs a positive `width`\n");
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
        this->new_master_port(name, &out->itf);

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
    this->burst_table[slot_idx].in_use = false;
    this->burst_table[slot_idx].input = nullptr;
    this->burst_table[slot_idx].output_id = -1;
    this->burst_table[slot_idx].original_burst_id = -1;
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
    // Fire retry() on any input that had been denied. The master re-sends; if the
    // blocking condition (FIFO or slot) is still present, handle_req denies again.
    for (auto *in : this->inputs)
    {
        if (in->denied_upstream)
        {
            in->denied_upstream = false;
            in->itf.retry();
        }
    }
}

vp::IoReqStatus RouterBeat::req_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterBeat *_this = (RouterBeat *)__this;
    InputPort *in = _this->inputs[port];
    uint64_t size = req->get_size();
    int64_t now = _this->clock.get_cycles();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Beat arrived (input: %d, addr: 0x%lx, size: %lu, write: %d, burst_id: %ld, first: %d, last: %d)\n",
        port, req->get_addr(), size, req->get_is_write() ? 1 : 0,
        (long)req->burst_id, req->is_first ? 1 : 0, req->is_last ? 1 : 0);

    // Size must fit one beat.
    if (size > (uint64_t)_this->cfg.width)
    {
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // Protocol checks.
    if (req->is_first && in->active_burst_slot != -1)
    {
        // Previous burst not finished yet.
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }
    if (!req->is_first && in->active_burst_slot == -1)
    {
        // Continuation beat without a started burst.
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // FIFO overflow check (before slot allocation — we don't want to consume a slot
    // and then fail on FIFO).
    if (in->pending_bytes + size > _this->cfg.max_input_pending_size)
    {
        in->denied_upstream = true;
        return vp::IO_REQ_DENIED;
    }

    // First beat of a new burst: allocate a slot. If the table is full, deny the
    // beat; we'll wake the master when a slot frees.
    if (req->is_first)
    {
        int slot_idx = _this->alloc_burst_slot();
        if (slot_idx == -1)
        {
            in->denied_upstream = true;
            return vp::IO_REQ_DENIED;
        }
        BurstEntry &slot = _this->burst_table[slot_idx];
        slot.input = in;
        slot.output_id = -1;                          // decoded later by fsm
        slot.channel = _this->channel_of(req->get_is_write());
        slot.original_burst_id = req->burst_id;
        in->active_burst_slot = slot_idx;
    }

    if (req->get_is_write()) { _this->stat_writes++; _this->stat_bytes_written += size; }
    else                     { _this->stat_reads++;  _this->stat_bytes_read    += size; }

    bool was_empty = in->pending.empty();
    in->pending.push_back(req);
    in->pending_bytes += size;
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
    // is touched for both directions, restoring the single-slot behaviour.
    std::vector<std::array<bool, NB_CHANNELS>> output_used(
        _this->entries.size(), {false, false});

    for (int k = 0; k < n; k++)
    {
        int i = (_this->round_robin_next + k) % n;
        InputPort *in = _this->inputs[i];
        if (in->pending.empty()) continue;

        // ClockedSignal gate: head must have been committed to a prior cycle.
        if (in->head_cycle.get() >= now) continue;

        vp::IoReq *beat = in->pending.front();

        // Slot was allocated in handle_req; invariant: active_burst_slot != -1.
        int slot_idx = in->active_burst_slot;
        BurstEntry &slot = _this->burst_table[slot_idx];

        // First time the fsm sees this burst: decode the mapping for the first beat.
        if (slot.output_id == -1)
        {
            vp::MappingTreeEntry *mapping = _this->mapping_tree.get(
                beat->get_addr(), beat->get_size(), beat->get_is_write());
            bool straddles = mapping && mapping->size != 0 &&
                beat->get_addr() + beat->get_size() > mapping->base + mapping->size;

            if (!mapping || mapping->id == _this->error_id || straddles ||
                !_this->entries[mapping->id]->itf.is_bound())
            {
                _this->stat_errors++;
                beat->set_resp_status(vp::IO_RESP_INVALID);
                in->pending.pop_front();
                in->pending_bytes -= beat->get_size();
                if (in->pending.empty()) in->head_cycle.set(INT64_MAX);
                // Free the burst (it never actually got routed).
                _this->free_burst_slot(slot_idx);
                in->active_burst_slot = -1;
                in->itf.resp(beat);
                _this->wake_denied_masters();
                continue;
            }
            slot.output_id = mapping->id;
        }

        OutputPort *out = _this->entries[slot.output_id];
        int ch = slot.channel;  // direction-channel locked in handle_req

        if (out->stalled) continue;
        if (out->elected_input[ch] != nullptr && out->elected_input[ch] != in) continue;
        if (output_used[slot.output_id][ch]) continue;

        // Lock output-channel to this input on is_first.
        if (beat->is_first)
        {
            out->elected_input[ch] = in;
        }

        // Forward: translate addr, remap burst_id to slot index.
        uint64_t original_addr = beat->get_addr();
        int64_t original_burst_id = beat->burst_id;
        beat->set_addr(original_addr - out->remove_offset + out->add_offset);
        beat->burst_id = slot_idx;

        vp::IoReqStatus st = out->itf.req(beat);

        switch (st)
        {
            case vp::IO_REQ_DONE:
            {
                bool was_last = beat->is_last;
                // Restore burst_id for the master. (Addr stays translated — matches
                // v1 router behavior and the sync router_v2.)
                beat->burst_id = original_burst_id;

                in->pending.pop_front();
                in->pending_bytes -= beat->get_size();
                if (in->pending.empty()) in->head_cycle.set(INT64_MAX);

                output_used[slot.output_id][ch] = true;

                if (was_last)
                {
                    out->elected_input[ch] = nullptr;
                    in->active_burst_slot = -1;
                    _this->free_burst_slot(slot_idx);
                }
                in->itf.resp(beat);
                _this->wake_denied_masters();
                break;
            }
            case vp::IO_REQ_GRANTED:
            {
                in->pending.pop_front();
                in->pending_bytes -= beat->get_size();
                if (in->pending.empty()) in->head_cycle.set(INT64_MAX);
                output_used[slot.output_id][ch] = true;
                // Slot stays alive until resp_muxed (possibly is_last).
                _this->wake_denied_masters();
                break;
            }
            case vp::IO_REQ_DENIED:
            {
                // Roll back the forward: beat stays at head of FIFO with original
                // addr and burst_id. Output stalls until downstream retry().
                beat->set_addr(original_addr);
                beat->burst_id = original_burst_id;
                out->stalled = true;
                break;
            }
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

void RouterBeat::resp_muxed(vp::Block *__this, vp::IoReq *req, int /*out_id*/)
{
    RouterBeat *_this = (RouterBeat *)__this;

    // Look up the burst via the remapped burst_id we wrote on forward.
    int slot_idx = (int)req->burst_id;
    BurstEntry &slot = _this->burst_table[slot_idx];
    InputPort *in = slot.input;
    OutputPort *out = _this->entries[slot.output_id];
    bool was_last = req->is_last;

    // Restore the master's original burst_id.
    req->burst_id = slot.original_burst_id;

    if (was_last)
    {
        out->elected_input[slot.channel] = nullptr;
        in->active_burst_slot = -1;
        _this->free_burst_slot(slot_idx);
    }

    in->itf.resp(req);

    if (was_last)
    {
        _this->wake_denied_masters();
        _this->schedule_fsm();
    }
}

void RouterBeat::retry_muxed(vp::Block *__this, int id)
{
    RouterBeat *_this = (RouterBeat *)__this;
    _this->entries[id]->stalled = false;
    _this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Output unstalled by downstream retry (output: %d)\n", id);
    _this->schedule_fsm();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterBeat(config);
}
