// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Width-splitting fan-out on the io_v2 protocol.
//
// A single incoming request of up to ``input_width`` bytes is carved into
// up to ``nb_outputs = input_width / output_width`` sub-requests, one per
// output port: the first chunk on ``output_0``, the second on
// ``output_1``, and so on. Chunk boundaries follow the ``output_width``
// alignment of the input address: the first chunk runs from the input
// address up to the next ``output_width`` boundary, subsequent chunks are
// aligned and sized at up to ``output_width`` bytes.
//
// Bursts larger than ``input_width``
// ----------------------------------
//
// A request whose ``size`` exceeds ``input_width`` cannot fit a single
// fan-out phase. The splitter walks it as a sequence of phases, where
// each phase covers at most ``input_width`` bytes:
//
//   - **Phase 0** starts at the incoming address, runs up to the next
//     ``input_width`` boundary, and uses the existing fan-out rules
//     (first chunk truncated to ``output_width`` alignment, subsequent
//     chunks aligned).
//   - **Phase k > 0** starts at the previous phase's end (now naturally
//     ``input_width``-aligned) and covers exactly ``input_width`` bytes,
//     except for the final phase whose tail may be shorter.
//
// Phases issue strictly in order: phase k+1 only fires after every
// chunk of phase k has responded. This matches RTL semantics for an
// AXI→TCDM bridge sitting in front of a wide log interconnect — bursts
// are unrolled into per-cycle beats, each beat is striped across the
// bank lanes, and the next beat does not advance until all banks have
// granted the current one.
//
// Latency model
// -------------
//
// The splitter measures, per phase, the maximum ``req->latency``
// reported by any chunk in that phase (the slowest lane dominates).
// It sums those per-phase maxima into ``accumulated_phase_latency`` —
// the "ideal" bandwidth cost assuming every phase advances back-to-back
// in one cycle plus whatever the bank annotates.
//
// At finalisation, real simulator cycles have already elapsed if any
// chunk responded asynchronously or was denied-and-retried. The final
// annotation is therefore
//
//   req->latency = max(0, accumulated_phase_latency - elapsed_sim)
//
// so the master always observes the larger of (ideal bandwidth time,
// real contention time), never double-counts the two, and never reports
// a smaller window than what already elapsed.
//
// Single-phase requests (``size <= input_width``) degenerate to a
// one-iteration version of this loop and produce the same observable
// behaviour as before.

#include <algorithm>
#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>
#include <interco/splitter_v2/splitter_config.hpp>

// Pulse a GUI signal to `v` now (+`delay` sub-cycle offset) and back to high-Z
// one cycle later, so each access shows as a one-cycle marker in the timeline.
static inline void gui_pulse(vp::Signal<uint64_t> &s, uint64_t v,
                             int64_t delay, int64_t period)
{
    s.set(v, (int64_t)0, delay);
    s.release(0, delay + period);
}

class Splitter : public vp::Component, public vp::DebugMemIf
{
public:
    Splitter(vp::ComponentConf &conf);
    void reset(bool active) override;

    // Backdoor debug access (vp/debug_mem.hpp): pure pass-through into
    // output 0. Chunk-to-output assignment is positional (first chunk goes
    // to output_0), so every output sees the full address space and any
    // single one is a valid debug entry into the downstream tree.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

    SplitterConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id, vp::IoRetryChannel);

    vp::IoReq *alloc_sub();
    void       free_sub(vp::IoReq *sub);

    // Issue successive phases of the pending parent until either the
    // burst is exhausted or a chunk fails to complete inline.
    void issue_phases();

    // Account one chunk as completed: update remaining/latency tracking
    // and decrement the in-flight count for the current phase.
    void account_chunk(vp::IoReq *sub);

    // Called after a chunk completes (either inline or via async resp/
    // retry). If the current phase has no more chunks pending, advance:
    // either issue the next phase or, if the burst is over, finalise.
    void after_chunk_completion();

    // Send the final resp(parent) and clear all per-burst state.
    void finalize_parent();

    vp::Trace trace;

    vp::IoSlave input_itf{&Splitter::input_req};
    std::vector<std::unique_ptr<vp::IoMaster>> outputs;

    int      nb_outputs = 0;
    uint64_t input_width = 0;
    uint64_t output_width = 0;

    // Current parent request being served. Exactly one at a time —
    // concurrent CPU requests from the upstream are refused with DENIED.
    vp::IoReq *pending_parent = nullptr;

    // Multi-phase tracking.
    uint64_t parent_total_size           = 0;
    uint64_t parent_bytes_planned        = 0;
    int      parent_phase_chunks_pending = 0;
    int64_t  parent_phase_max_latency    = 0;
    int64_t  parent_accumulated_latency  = 0;
    int64_t  parent_admission_cycle      = 0;
    bool     parent_inside_input_req     = false;

    // For each output, the sub-request that was DENIED and is waiting for
    // that output's retry(). Null while the output is free.
    std::vector<vp::IoReq *> stuck;

    // Free list of sub-request objects, linked via their ``next`` field.
    vp::IoReq *free_list = nullptr;

    // Set when we refused an upstream request with DENIED. Cleared — and
    // retry() fired — once the pending parent completes.
    bool input_needs_retry = false;

    // --- GUI traces (visible in the model-graph / timeline) ---
    int64_t gui_slot_delay();
    void    gui_log_input(uint64_t addr, uint64_t size, bool is_write);
    void    gui_log_output(int i, uint64_t addr, uint64_t size);
    int64_t gui_last_cycle = -1;
    int     gui_nb_same_cycle = 0;
    vp::Signal<uint64_t> gui_active;
    vp::Signal<uint64_t> gui_addr;
    vp::Signal<uint64_t> gui_size;
    vp::Signal<uint64_t> gui_is_write;
    std::vector<std::unique_ptr<vp::Signal<uint64_t>>> gui_out_addr;
    std::vector<std::unique_ptr<vp::Signal<uint64_t>>> gui_out_size;
};


Splitter::Splitter(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      gui_active(*this, "active", 1, vp::SignalCommon::ResetKind::HighZ),
      gui_addr(*this, "addr", 64, vp::SignalCommon::ResetKind::HighZ),
      gui_size(*this, "size", 64, vp::SignalCommon::ResetKind::HighZ),
      gui_is_write(*this, "is_write", 1, vp::SignalCommon::ResetKind::HighZ)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->input_width  = (uint64_t)this->cfg.input_width;
    this->output_width = (uint64_t)this->cfg.output_width;
    this->nb_outputs   = (int)(this->cfg.input_width / this->cfg.output_width);

    this->outputs.reserve(this->nb_outputs);
    for (int i = 0; i < this->nb_outputs; i++)
    {
        auto m = std::make_unique<vp::IoMaster>(
            i, &Splitter::output_retry, &Splitter::output_resp);
        this->new_master_port("output_" + std::to_string(i), m.get());
        this->outputs.push_back(std::move(m));

        std::string pfx = "output_" + std::to_string(i);
        this->gui_out_addr.push_back(std::make_unique<vp::Signal<uint64_t>>(
            *this, pfx + "/addr", 64, vp::SignalCommon::ResetKind::HighZ));
        this->gui_out_size.push_back(std::make_unique<vp::Signal<uint64_t>>(
            *this, pfx + "/size", 64, vp::SignalCommon::ResetKind::HighZ));
    }

    this->stuck.assign((size_t)this->nb_outputs, nullptr);

    this->new_slave_port("input", &this->input_itf);
}

void Splitter::reset(bool active)
{
    if (active)
    {
        this->pending_parent              = nullptr;
        this->parent_total_size           = 0;
        this->parent_bytes_planned        = 0;
        this->parent_phase_chunks_pending = 0;
        this->parent_phase_max_latency    = 0;
        this->parent_accumulated_latency  = 0;
        this->parent_admission_cycle      = 0;
        this->parent_inside_input_req     = false;
        this->input_needs_retry           = false;
        std::fill(this->stuck.begin(), this->stuck.end(), nullptr);
    }
}


// Sub-cycle offset so several accesses logged in the same cycle are spread
// across the cycle and all remain visible in the GUI.
int64_t Splitter::gui_slot_delay()
{
    int64_t cycles = this->clock.get_cycles();
    if (cycles > this->gui_last_cycle)
    {
        this->gui_nb_same_cycle = 0;
        this->gui_last_cycle = cycles;
    }
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_nb_same_cycle > 0
        ? period - (period >> this->gui_nb_same_cycle) : 0;
    this->gui_nb_same_cycle++;
    return delay;
}

void Splitter::gui_log_input(uint64_t addr, uint64_t size, bool is_write)
{
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_slot_delay();
    gui_pulse(this->gui_addr, addr, delay, period);
    gui_pulse(this->gui_size, size, delay, period);
    gui_pulse(this->gui_is_write, is_write ? 1 : 0, delay, period);
    gui_pulse(this->gui_active, 1, delay, period);
}

void Splitter::gui_log_output(int i, uint64_t addr, uint64_t size)
{
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_slot_delay();
    gui_pulse(*this->gui_out_addr[i], addr, delay, period);
    gui_pulse(*this->gui_out_size[i], size, delay, period);
    gui_pulse(this->gui_active, 1, delay, period);
}


vp::IoReq *Splitter::alloc_sub()
{
    if (this->free_list == nullptr)
    {
        return new vp::IoReq();
    }
    vp::IoReq *sub = this->free_list;
    this->free_list = sub->get_next();
    return sub;
}

void Splitter::free_sub(vp::IoReq *sub)
{
    sub->set_next(this->free_list);
    this->free_list = sub;
}


void Splitter::account_chunk(vp::IoReq *sub)
{
    vp::IoReq *parent = this->pending_parent;
    if (sub->get_resp_status() == vp::IO_RESP_INVALID)
    {
        parent->set_resp_status(vp::IO_RESP_INVALID);
    }
    int64_t lat = sub->get_latency();
    if (lat > this->parent_phase_max_latency)
    {
        this->parent_phase_max_latency = lat;
    }
    parent->remaining_size -= sub->get_size();
    this->parent_phase_chunks_pending--;
    this->free_sub(sub);
}


void Splitter::issue_phases()
{
    vp::IoReq *parent = this->pending_parent;
    uint64_t   ow      = this->output_width;
    uint64_t   ow_mask = ow - 1;
    uint64_t   iw_mask = this->input_width - 1;

    // Keep issuing phases inline until either the burst is exhausted or
    // a chunk fails to complete inline (a GRANTED or DENIED leaves a
    // pending in-flight count on the phase).
    while (this->parent_bytes_planned < this->parent_total_size
        && this->parent_phase_chunks_pending == 0)
    {
        // Compute this phase's byte range. The phase runs from the
        // current offset to either the next input_width boundary or the
        // end of the burst, whichever comes first.
        uint64_t base       = parent->get_addr() + this->parent_bytes_planned;
        uint64_t total_rem  = this->parent_total_size - this->parent_bytes_planned;
        uint64_t iw_rem     = this->input_width - (base & iw_mask);
        uint64_t phase_size = std::min(iw_rem, total_rem);

        // Reset per-phase latency tracking. The accumulated total is
        // bumped once the phase actually completes (i.e. when
        // parent_phase_chunks_pending hits zero again).
        this->parent_phase_max_latency = 0;

        uint64_t  addr   = base;
        uint64_t  size   = phase_size;
        uint8_t  *data   = parent->get_data() + this->parent_bytes_planned;
        auto      opcode = parent->get_opcode();

        // Fan out across outputs. At most nb_outputs chunks; each chunk
        // covers up to output_width bytes, aligned at its tail to the
        // output_width boundary.
        for (int i = 0; i < this->nb_outputs && size > 0; i++)
        {
            uint64_t port_size = ow - (addr & ow_mask);
            uint64_t iter_size = std::min(port_size, size);

            this->gui_log_output(i, addr, iter_size);

            vp::IoReq *sub = this->alloc_sub();
            sub->prepare();
            sub->parent = parent;
            sub->set_addr(addr);
            sub->set_size(iter_size);
            sub->set_data(data);
            sub->set_opcode(opcode);
            sub->set_resp_status(vp::IO_RESP_OK);

            addr += iter_size;
            data += iter_size;
            size -= iter_size;
            this->parent_phase_chunks_pending++;

            vp::IoReqStatus st = this->outputs[i]->req(sub);
            if (st == vp::IO_REQ_DONE)
            {
                this->account_chunk(sub);
            }
            else if (st == vp::IO_REQ_DENIED)
            {
                // Park on this output; resend on output_retry(i).
                this->stuck[i] = sub;
            }
            // IO_REQ_GRANTED: resp will come later via output_resp.
        }

        this->parent_bytes_planned += phase_size;

        if (this->parent_phase_chunks_pending == 0)
        {
            // Phase fully completed inline. Accumulate its latency and
            // let the while-loop advance to the next phase.
            this->parent_accumulated_latency += this->parent_phase_max_latency;
        }
        // else: the loop exits naturally — the phase is in flight and
        // will resume from after_chunk_completion() once chunks respond.
    }
}


void Splitter::after_chunk_completion()
{
    // Called every time a chunk finishes responding. Nothing to do
    // unless the current phase is now fully drained.
    if (this->parent_phase_chunks_pending != 0)
    {
        return;
    }

    // Phase done — bank its max-latency, then advance.
    this->parent_accumulated_latency += this->parent_phase_max_latency;

    if (this->parent_bytes_planned < this->parent_total_size)
    {
        // More phases left. Issuing them may again leave a phase in
        // flight or may exhaust the burst entirely.
        this->issue_phases();
        if (this->parent_phase_chunks_pending != 0)
        {
            return;
        }
    }

    // All phases done. If we're still on the input_req call stack the
    // caller will report IO_REQ_DONE itself; otherwise we have to fire
    // an explicit resp(parent) to the upstream master.
    if (!this->parent_inside_input_req)
    {
        this->finalize_parent();
    }
}


void Splitter::finalize_parent()
{
    vp::IoReq *parent = this->pending_parent;

    // Total observable time = max(accumulated ideal, real elapsed sim).
    // We've already burned elapsed_sim cycles between admission and now;
    // the annotation tops it up to the ideal whenever the burst could
    // theoretically have taken longer than the contention actually did.
    int64_t elapsed = this->clock.get_cycles() - this->parent_admission_cycle;
    int64_t topup   = this->parent_accumulated_latency - elapsed;
    parent->latency = topup > 0 ? topup : 0;
    parent->is_first = true;
    parent->is_last  = true;

    this->pending_parent              = nullptr;
    this->parent_total_size           = 0;
    this->parent_bytes_planned        = 0;
    this->parent_phase_chunks_pending = 0;
    this->parent_phase_max_latency    = 0;
    this->parent_accumulated_latency  = 0;

    this->input_itf.resp(parent);

    if (this->input_needs_retry)
    {
        this->input_needs_retry = false;
        this->input_itf.retry();
    }
}


vp::IoReqStatus Splitter::input_req(vp::Block *__this, vp::IoReq *req)
{
    Splitter *_this = (Splitter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "IO req (req: %p, addr: 0x%llx, size: 0x%llx, write: %d)\n",
        req, (unsigned long long)req->get_addr(),
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0);

    // One parent at a time.
    if (_this->pending_parent != nullptr)
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    uint64_t req_size = req->get_size();

    // Empty requests: nothing to forward, trivially done.
    if (req_size == 0)
    {
        req->set_resp_status(vp::IO_RESP_OK);
        return vp::IO_REQ_DONE;
    }

    _this->gui_log_input(req->get_addr(), req_size, req->get_is_write());

    _this->pending_parent              = req;
    _this->parent_total_size           = req_size;
    _this->parent_bytes_planned        = 0;
    _this->parent_phase_chunks_pending = 0;
    _this->parent_phase_max_latency    = 0;
    _this->parent_accumulated_latency  = 0;
    _this->parent_admission_cycle      = _this->clock.get_cycles();
    req->remaining_size                = req_size;
    req->set_resp_status(vp::IO_RESP_OK);

    _this->parent_inside_input_req = true;
    _this->issue_phases();
    _this->parent_inside_input_req = false;

    // If everything resolved inline, complete synchronously without a
    // resp() round-trip. ``parent_phase_chunks_pending == 0`` *and*
    // ``parent_bytes_planned == parent_total_size`` means every phase
    // landed DONE.
    if (_this->parent_phase_chunks_pending == 0
        && _this->parent_bytes_planned == _this->parent_total_size)
    {
        int64_t topup = _this->parent_accumulated_latency;
        req->latency = topup > 0 ? topup : 0;
        _this->pending_parent              = nullptr;
        _this->parent_total_size           = 0;
        _this->parent_bytes_planned        = 0;
        _this->parent_accumulated_latency  = 0;
        return vp::IO_REQ_DONE;
    }

    return vp::IO_REQ_GRANTED;
}


void Splitter::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    Splitter *_this = (Splitter *)__this;
    _this->account_chunk(req);
    _this->after_chunk_completion();
}


void Splitter::output_retry(vp::Block *__this, int id, vp::IoRetryChannel)
{
    Splitter *_this = (Splitter *)__this;

    vp::IoReq *sub = _this->stuck[id];
    if (sub == nullptr)
    {
        return;
    }
    _this->stuck[id] = nullptr;

    vp::IoReqStatus st = _this->outputs[id]->req(sub);
    if (st == vp::IO_REQ_DONE)
    {
        _this->account_chunk(sub);
        _this->after_chunk_completion();
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Still stuck — hold it until the next retry.
        _this->stuck[id] = sub;
    }
    // IO_REQ_GRANTED: the output will resp() later.
}


// Backdoor target behind output 0, or nullptr.
static vp::DebugMemIf *output0_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int Splitter::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
    bool is_write)
{
    vp::DebugMemIf *child = output0_debug_mem(*this->outputs[0]);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void Splitter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *child = output0_debug_mem(*this->outputs[0]);
    if (child != nullptr)
    {
        child->debug_mem_regions(regions, local_base, window_size, entry_base,
            depth + 1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Splitter(config);
}
