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
 *             is_first=is_last=true. The response is a stream of beats produced
 *             by the per-output BeatResponseAdapter regardless of the downstream
 *             slave's actual response form (sync DONE, async big-packet, or
 *             native beat stream). The router emits one upstream resp() per
 *             response beat; the slot stays alive until the response stream's
 *             is_last.
 *   - Writes: N forward req() beats per burst (size <= width each), with
 *             per-beat is_first/is_last/burst_id from the master. Each forward
 *             produces exactly one upstream resp() (the adapter sees a single
 *             chunk per req).
 *
 * Per-burst routing table: a fixed-size table (`max_pending_bursts` entries).
 * On is_first of a burst the input allocates a slot; subsequent beats (writes
 * only) reuse it. The slot holds the originating input, the translated output,
 * the burst's direction-channel and the master's original burst_id. On forward
 * we remap beat.burst_id to the slot index; the adapter (and the downstream
 * slave) echoes it back on response; the OutputPort::on_beat handler looks up
 * the slot in O(1) via the remapped id.
 *
 * Beat-everywhere on the wire: every output port owns a BeatResponseAdapter,
 * which normalises any slave response form into 1-beat-per-cycle on_beat
 * callbacks. Each callback fans out as one upstream resp(). So a chain of
 * RouterBeat routers terminating at a sync-DONE memory still produces a beat
 * stream at every hop.
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
#include <utils/io_v2_beat_adapter.hpp>

#include "proxy_command.hpp"

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
    InputPort *input = nullptr;        // nullptr marks a proxy-originated burst
    int output_id = -1;                // -1 until fsm has decoded the first beat's mapping
    int channel = 0;                   // resolved direction-channel for this burst
    int64_t original_burst_id = -1;    // master's burst_id, restored on response
    // Non-null when this slot was allocated by handle_proxy_command; on_beat
    // signals it on the burst's is_last instead of calling input->itf.resp().
    vp_router_v2_proxy::ProxyWaiter *proxy_waiter = nullptr;
};

class OutputPort : public BeatResponseAdapter::Handler
{
public:
    OutputPort(RouterBeat *top, int id, std::string name, int beat_width);

    // BeatResponseAdapter::Handler — fired per response beat (any slave form).
    void on_beat(const BeatResponseAdapter::BeatEvent &event) override;
    void on_retry() override;

    void log_access(uint64_t addr, uint64_t size);

    RouterBeat *top;
    int id;
    // Bus-facing master + response normaliser. The router binds
    // `adapter.out()` outward via `new_master_port(...)`.
    BeatResponseAdapter adapter;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    // Input currently holding this output for a burst, per channel; nullptr when
    // the channel is free. With shared_rw_channel=true only [0] is used.
    InputPort *elected_input[NB_CHANNELS] = {nullptr, nullptr};
    // Downstream returned DENIED for the most recent forward; waiting for retry().
    // Stall is per-output (single downstream IoMaster), not per-channel.
    bool stalled = false;
    // Per-mapping VCD traces — pre-translation addr / size of each beat forwarded.
    vp::Signal<uint64_t> current_addr;
    vp::Signal<uint64_t> current_size;
    int64_t last_logged_access = -1;
    int nb_logged_access_in_same_cycle = 0;
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

class RouterBeat : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterBeat(vp::ComponentConf &conf);
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file,
        FILE *reply_file, std::vector<std::string> args,
        std::string cmd_req) override;

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // Proxy mem_read / mem_write dispatch. Forwards a single request directly
    // to the matching output's adapter, bypassing the per-input FIFO and
    // round-robin arbitration. Allocates a burst slot tagged with
    // input=nullptr + proxy_waiter so OutputPort::on_beat routes the
    // is_last beat back to the waiting proxy thread.
    vp::IoReqStatus dispatch_proxy_req(vp::IoReq *req,
        vp_router_v2_proxy::ProxyWaiter *waiter);

    // Set by handle_proxy_command while waiting for retry after a DENIED on
    // the proxy path. Cleared after the proxy resumes.
    vp_router_v2_proxy::ProxyWaiter *proxy_retry_waiter = nullptr;
    std::mutex proxy_retry_mu;

    void schedule_fsm();
    int alloc_burst_slot();
    void free_burst_slot(int slot_idx);
    int channel_of(bool is_write) const
    {
        return this->cfg.shared_rw_channel ? 0 : (is_write ? 1 : 0);
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

    vp::StatScalar stat_reads;
    vp::StatScalar stat_writes;
    vp::StatScalar stat_bytes_read;
    vp::StatScalar stat_bytes_written;
    vp::StatScalar stat_errors;
};


//
// OutputPort / InputPort
//

OutputPort::OutputPort(RouterBeat *top, int id, std::string name, int beat_width)
    : top(top), id(id),
      adapter(top, name + "_adapter", beat_width, this),
      current_addr(*top, name + "/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      current_size(*top, name + "/size", 64, vp::SignalCommon::ResetKind::HighZ)
{
}

void OutputPort::log_access(uint64_t addr, uint64_t size)
{
    int64_t cycles = this->top->clock.get_cycles();
    if (cycles > this->last_logged_access)
        this->nb_logged_access_in_same_cycle = 0;

    int64_t delay = 0;
    if (this->nb_logged_access_in_same_cycle > 0)
    {
        int64_t period = this->top->clock.get_period();
        delay = period - (period >> this->nb_logged_access_in_same_cycle);
    }
    this->current_addr.set_and_release(addr, 0, delay);
    this->current_size.set_and_release(size, 0, delay);
    this->nb_logged_access_in_same_cycle++;
    this->last_logged_access = cycles;
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
      fsm_event(this, &RouterBeat::fsm_handler)
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

        OutputPort *out = new OutputPort(this, mapping_id, name, this->cfg.width);
        out->remove_offset = m.remove_offset;
        out->add_offset = m.add_offset;
        this->entries.push_back(out);
        // Bind the per-output adapter's bus-facing master to the named port.
        // The adapter is the context block: its static resp/retry trampolines
        // dispatch to OutputPort::on_beat / on_retry via the Handler interface.
        this->new_master_port(name, &out->adapter.out(), &out->adapter);

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
    this->burst_table[slot_idx].proxy_waiter = nullptr;
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
    // response is then chunked into beats by the output adapter.
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
    // slot and then fail on FIFO).
    if (in->pending_bytes + size > _this->cfg.max_input_pending_size)
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
        slot.proxy_waiter = nullptr;

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
                !_this->entries[mapping->id]->adapter.out().is_bound())
            {
                _this->stat_errors++;
                beat->set_resp_status(vp::IO_RESP_INVALID);
                in->pending.pop_front();
                in->pending_bytes -= beat->get_size();
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

        if (out->stalled) continue;
        if (out->elected_input[ch] != nullptr && out->elected_input[ch] != in) continue;
        if (output_used[slot.output_id][ch]) continue;

        // Lock output-channel to this input on is_first (writes) or on the
        // single read forward (reads).
        if (beat->is_first)
        {
            out->elected_input[ch] = in;
        }

        // Forward: translate addr, remap burst_id to slot index. We capture
        // originals only for the rollback-on-DENIED path; on accepted forward
        // the response side restores burst_id from slot.original_burst_id.
        uint64_t original_addr = beat->get_addr();
        int64_t original_burst_id = beat->burst_id;
        out->log_access(original_addr, beat->get_size());
        beat->set_addr(original_addr - out->remove_offset + out->add_offset);
        beat->burst_id = slot_idx;

        vp::IoReqStatus st = out->adapter.submit(beat);
        // The adapter never returns IO_REQ_DONE — it converts inline DONE
        // into a scheduled on_beat callback stream. Asserting here protects
        // against future protocol drift.
        vp_assert(st != vp::IO_REQ_DONE, &_this->trace,
            "BeatResponseAdapter must not surface IO_REQ_DONE\n");

        switch (st)
        {
            case vp::IO_REQ_GRANTED:
            {
                in->pending.pop_front();
                in->pending_bytes -= beat->get_size();
                if (in->pending.empty()) in->head_cycle.set(INT64_MAX);
                output_used[slot.output_id][ch] = true;
                // Slot stays alive until on_beat fires for the burst's is_last.
                _this->wake_denied_masters();
                break;
            }
            case vp::IO_REQ_DENIED:
            {
                // Roll back the forward: beat stays at head of FIFO with
                // original addr/burst_id. Output stalls until on_retry().
                beat->set_addr(original_addr);
                beat->burst_id = original_burst_id;
                out->stalled = true;
                break;
            }
            case vp::IO_REQ_DONE:
                // Unreachable (asserted above), but keep the compiler quiet.
                break;
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
// Response path — driven by the per-output BeatResponseAdapter.
//

void OutputPort::on_beat(const BeatResponseAdapter::BeatEvent &event)
{
    // The forward path stashed slot_idx in event.req->burst_id and the adapter
    // captured it as event.burst_id at schedule time. Reading from
    // event.burst_id (instead of the live req field) is critical when this
    // response is being relayed up a chain of BEAT routers: each router does
    // a local restore of req->burst_id back to its own slot after calling
    // in->itf.resp(), so by the time the upstream router's on_beat fires the
    // live req->burst_id holds the deeper router's slot, not ours.
    int slot_idx = (int)event.burst_id;
    BurstEntry &slot = this->top->burst_table[slot_idx];

    bool master_is_last = event.req->is_last;

    // The slot's lifetime is the union of:
    //   - the adapter saying this beat is the last of its current in-flight
    //     request (event.is_last — derived from cumulative bytes routed
    //     against the request's size), AND
    //   - the master saying this request is the last forward of the burst
    //     (master_is_last — snapshot of req->is_last before any mutation).
    //
    // For the asymmetric read case (1 req per burst, master_is_last=true,
    // N response beats from the adapter) both are true only on the last
    // response beat. For the beat-streamed write case (N forward reqs per
    // burst, master_is_last varies per req) the adapter sees a single
    // chunk per req so event.is_last is always true, leaving
    // master_is_last as the burst-level guard.
    bool burst_done = event.is_last && master_is_last;

    // Proxy-originated burst: the input handle is nullptr and the waiter on the
    // slot holds the synchronisation handle for the proxy thread. Notify only
    // on the burst's last beat — earlier beats have already written the
    // payload into the proxy's req->data buffer.
    if (slot.input == nullptr)
    {
        if (burst_done)
        {
            vp_router_v2_proxy::ProxyWaiter *waiter = slot.proxy_waiter;
            event.req->burst_id = slot.original_burst_id;
            event.req->is_last = master_is_last;   // (true on the last forward)
            this->elected_input[slot.channel] = nullptr;
            this->top->free_burst_slot(slot_idx);
            vp_router_v2_proxy::notify_replied(waiter);
        }
        // Intermediate adapter chunks for a proxy burst: data is being
        // accumulated into the proxy's req->data buffer; nothing to signal.
        return;
    }

    InputPort *in = slot.input;

    // Mutate the req to expose this beat to the upstream master.
    event.req->set_data(event.data);
    event.req->set_size(event.size);
    event.req->is_first = event.is_first;
    event.req->is_last = event.is_last;
    event.req->set_resp_status(event.status);
    event.req->burst_id = slot.original_burst_id;

    if (burst_done)
    {
        this->elected_input[slot.channel] = nullptr;
        // The input's in-progress tracker (active_multi_beat_slot) was
        // already cleared by req_muxed at queue-time on the master's is_last
        // beat — the input could start a new multi-beat burst before this
        // response even came back, AXI-style. So nothing to clear here.
        this->top->free_burst_slot(slot_idx);
    }

    in->itf.resp(event.req);

    if (!burst_done)
    {
        // Restore so the next on_beat for this req sees its master state
        // (the lookup key + the master's is_last flag).
        event.req->burst_id = slot_idx;
        event.req->is_last = master_is_last;
    }
    else
    {
        this->top->wake_denied_masters();
        this->top->schedule_fsm();
    }
}

void OutputPort::on_retry()
{
    this->stalled = false;
    this->top->trace.msg(vp::Trace::LEVEL_TRACE,
        "Output unstalled by downstream retry (output: %d)\n", this->id);
    this->top->schedule_fsm();

    // Wake any proxy thread waiting for retry after a DENIED on the proxy path.
    vp_router_v2_proxy::ProxyWaiter *waiter = nullptr;
    {
        std::lock_guard<std::mutex> lk(this->top->proxy_retry_mu);
        waiter = this->top->proxy_retry_waiter;
        this->top->proxy_retry_waiter = nullptr;
    }
    if (waiter)
    {
        vp_router_v2_proxy::notify_retry(waiter);
    }
}


//
// Proxy command path
//

vp::IoReqStatus RouterBeat::dispatch_proxy_req(vp::IoReq *req,
    vp_router_v2_proxy::ProxyWaiter *waiter)
{
    uint64_t addr = req->get_addr();
    uint64_t size = req->get_size();
    vp::MappingTreeEntry *mapping = this->mapping_tree.get(
        addr, size, req->get_is_write());
    bool straddles = mapping && mapping->size != 0 &&
        addr + size > mapping->base + mapping->size;
    if (!mapping || mapping->id == this->error_id || straddles ||
        !this->entries[mapping->id]->adapter.out().is_bound())
    {
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    OutputPort *out = this->entries[mapping->id];
    if (out->stalled)
    {
        return vp::IO_REQ_DENIED;
    }

    int slot_idx = this->alloc_burst_slot();
    if (slot_idx < 0)
    {
        // No free burst slot — wait for one to be released.
        return vp::IO_REQ_DENIED;
    }

    BurstEntry &slot = this->burst_table[slot_idx];
    slot.input = nullptr;
    slot.output_id = (int)mapping->id;
    slot.channel = this->channel_of(req->get_is_write());
    slot.original_burst_id = req->burst_id;
    slot.proxy_waiter = waiter;
    out->elected_input[slot.channel] = nullptr;  // proxy bursts don't lock the channel
                                                 // (no upstream input to track), but we
                                                 // still hold the slot until is_last.

    uint64_t original_addr = addr;
    int64_t saved_burst_id = req->burst_id;
    bool saved_first = req->is_first;
    bool saved_last = req->is_last;

    req->set_addr(addr - out->remove_offset + out->add_offset);
    req->burst_id = slot_idx;
    req->is_first = true;
    req->is_last = true;

    out->log_access(original_addr, size);
    vp::IoReqStatus st = out->adapter.submit(req);

    // The adapter never returns IO_REQ_DONE.
    vp_assert(st != vp::IO_REQ_DONE, &this->trace,
        "BeatResponseAdapter must not surface IO_REQ_DONE on proxy path\n");

    if (st == vp::IO_REQ_GRANTED)
    {
        // Slot stays live until on_beat fires for is_last.
        return st;
    }

    // DENIED: free the slot and undo our mutations so the caller sees the
    // request unchanged. The adapter has already cleaned its in-flight slot.
    this->free_burst_slot(slot_idx);
    req->burst_id = saved_burst_id;
    req->is_first = saved_first;
    req->is_last = saved_last;
    req->set_addr(original_addr);
    out->stalled = true;
    return st;
}

std::string RouterBeat::handle_command(gv::GvProxy *proxy, FILE *req_file,
    FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    return vp_router_v2_proxy::handle_proxy_command(
        proxy, this->get_launcher(), req_file, reply_file, args, cmd_req,
        [this](vp::IoReq *r, vp_router_v2_proxy::ProxyWaiter *w) {
            return this->dispatch_proxy_req(r, w);
        },
        [this](vp_router_v2_proxy::ProxyWaiter *w) {
            std::lock_guard<std::mutex> lk(this->proxy_retry_mu);
            this->proxy_retry_waiter = w;
        });
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterBeat(config);
}
