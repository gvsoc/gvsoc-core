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
// Stateless, pipelined fan-out
// ----------------------------
//
// The splitter keeps no per-request state of its own. Each request is the
// aggregation context for its own chunks: ``remaining_size`` counts down as
// chunks respond and ``latency`` accumulates the slowest chunk (the
// critical path). Every sub-request carries a ``parent`` back-link, so a
// chunk response is folded straight into its parent regardless of how many
// parents are in flight. Consequently the splitter never serialises on a
// single in-flight parent — it forwards a request's chunks and immediately
// returns, ready to accept the next request, so beats stream back-to-back.
//
// The only back-pressure is the per-output one-stuck-chunk rule: a chunk
// that the downstream DENIES is parked on its output until that output
// retries. While any output holds a denied chunk, a *new* parent is refused
// with ``IO_REQ_DENIED`` (and retried once the stuck chunk clears), so an
// output never accumulates more than one denied chunk.
//
// Single-fan-out only
// -------------------
//
// A request must fit a single fan-out: ``(addr % input_width) + size <=
// input_width``. Larger bursts are expected to be chunked into
// ``input_width``-wide beats upstream (e.g. by an AXI beat router / its
// auto-inserted IoV2BeatAdapter) before they reach the splitter. A request
// that would straddle the ``input_width`` window is a configuration error
// and aborts.
//
// Latency model
// -------------
//
// The splitter adds no cycles of its own. A request's reported latency is
// the maximum ``latency`` annotated by any of its chunks — the slowest lane
// dominates the critical path. Downstream models own all cycle accuracy.

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
    static vp::IoRespAck   output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id, vp::IoRetryChannel);

    vp::IoReq *alloc_sub();
    void       free_sub(vp::IoReq *sub);

    // Fan a single-fan-out request out across the outputs and return the
    // status to relay upstream. Aggregation rides entirely on the parent
    // (remaining_size / latency) + each sub's ``parent`` back-link, so any
    // number of parents can be in flight at once.
    vp::IoReqStatus issue(vp::IoReq *parent);
    // Fold one responded chunk into its parent (decrement remaining_size,
    // max-in latency, latch errors) and free the sub.
    void            account(vp::IoReq *sub);
    // True while any output holds a denied chunk awaiting its retry().
    bool            any_stuck();

    vp::Trace trace;

    vp::IoSlave input_itf{&Splitter::input_req};
    std::vector<std::unique_ptr<vp::IoMaster>> outputs;

    int      nb_outputs = 0;
    uint64_t input_width = 0;
    uint64_t output_width = 0;

    // For each output, the sub-request that was DENIED and is waiting for
    // that output's retry(). Null while the output is free.
    std::vector<vp::IoReq *> stuck;

    // Free list of sub-request objects, linked via their ``next`` field.
    vp::IoReq *free_list = nullptr;

    // Set when we refused an upstream request with DENIED (an output was
    // stuck). Cleared — and retry() fired — once every output is free again.
    bool input_needs_retry = false;

    // --- GUI traces (visible in the model-graph / timeline) ---
    int64_t gui_slot_delay(int slot);
    void    gui_log_input(uint64_t addr, uint64_t size, bool is_write);
    void    gui_log_output(int i, uint64_t addr, uint64_t size);
    // Per-channel intra-cycle slot accounting: index 0 = the input signals,
    // 1+i = output i's signals. Distinct signals live on distinct tracks, so
    // each spreads only against its own repeated pulses in a cycle, not against
    // the other channels. The shared 'active' lane is pulsed with the same slot
    // delay as the request it accompanies, so it never lights without a request.
    std::vector<int64_t> gui_slot_last_cycle;
    std::vector<int>     gui_slot_nb;
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

    // One slot per GUI channel: index 0 = input, 1+i = output i.
    this->gui_slot_last_cycle.assign((size_t)this->nb_outputs + 1, -1);
    this->gui_slot_nb.assign((size_t)this->nb_outputs + 1, 0);

    this->new_slave_port("input", &this->input_itf);
}

void Splitter::reset(bool active)
{
    if (active)
    {
        this->input_needs_retry = false;
        std::fill(this->stuck.begin(), this->stuck.end(), nullptr);
    }
}


// Sub-cycle offset so several accesses logged in the same cycle are spread
// across the cycle and all remain visible in the GUI.
int64_t Splitter::gui_slot_delay(int slot)
{
    int64_t cycles = this->clock.get_cycles();
    if (cycles > this->gui_slot_last_cycle[slot])
    {
        this->gui_slot_nb[slot] = 0;
        this->gui_slot_last_cycle[slot] = cycles;
    }
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_slot_nb[slot] > 0
        ? period - (period >> this->gui_slot_nb[slot]) : 0;
    this->gui_slot_nb[slot]++;
    return delay;
}

void Splitter::gui_log_input(uint64_t addr, uint64_t size, bool is_write)
{
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_slot_delay(0);   // input channel
    gui_pulse(this->gui_addr, addr, delay, period);
    gui_pulse(this->gui_size, size, delay, period);
    gui_pulse(this->gui_is_write, is_write ? 1 : 0, delay, period);
    // 'active' shares the input addr's slot so the busy marker coincides with
    // the request it represents — never an active pulse without a request.
    gui_pulse(this->gui_active, 1, delay, period);
}

void Splitter::gui_log_output(int i, uint64_t addr, uint64_t size)
{
    int64_t period = this->clock.get_period();
    int64_t delay = this->gui_slot_delay(1 + i);   // output i channel
    gui_pulse(*this->gui_out_addr[i], addr, delay, period);
    gui_pulse(*this->gui_out_size[i], size, delay, period);
    // 'active' shares this output's slot, so it coincides with the forwarded
    // request and is not pulsed at all when the forward is denied.
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


bool Splitter::any_stuck()
{
    for (vp::IoReq *s : this->stuck)
    {
        if (s != nullptr)
        {
            return true;
        }
    }
    return false;
}


void Splitter::account(vp::IoReq *sub)
{
    vp::IoReq *parent = sub->parent;
    if (sub->get_resp_status() == vp::IO_RESP_INVALID)
    {
        parent->set_resp_status(vp::IO_RESP_INVALID);
    }
    // Critical-path latency: the slowest chunk dominates the parent.
    int64_t lat = sub->get_latency();
    if (lat > parent->latency)
    {
        parent->latency = lat;
    }
    parent->remaining_size -= sub->get_size();
    this->free_sub(sub);
}


vp::IoReqStatus Splitter::issue(vp::IoReq *parent)
{
    uint64_t ow      = this->output_width;
    uint64_t ow_mask = ow - 1;

    uint64_t  addr   = parent->get_addr();
    uint64_t  size   = parent->get_size();
    uint8_t  *data   = parent->get_data();
    auto      opcode = parent->get_opcode();

    // The parent IS the aggregation context: remaining_size counts down as
    // chunks respond, latency takes the max. Nothing is stored on the
    // splitter, so any number of parents can be in flight at once.
    parent->remaining_size = size;
    parent->latency        = 0;
    parent->set_resp_status(vp::IO_RESP_OK);

    for (int i = 0; i < this->nb_outputs && size > 0; i++)
    {
        uint64_t port_size  = ow - (addr & ow_mask);
        uint64_t iter_size  = std::min(port_size, size);
        uint64_t chunk_addr = addr;

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

        vp::IoReqStatus st = this->outputs[i]->req(sub);
        if (st == vp::IO_REQ_DENIED)
        {
            // Park on this output; logged only once actually accepted (in
            // output_retry). While any output is stuck we refuse new parents,
            // preserving the one-stuck-chunk-per-output invariant.
            this->stuck[i] = sub;
        }
        else
        {
            this->gui_log_output(i, chunk_addr, iter_size);
            if (st == vp::IO_REQ_DONE)
            {
                this->account(sub);
            }
            // IO_REQ_GRANTED: folded in later via output_resp.
        }
    }

    // All chunks landed inline → synchronous DONE (parent->latency already
    // holds the max chunk latency). Otherwise GRANTED; the last chunk's
    // response fires resp(parent).
    return parent->remaining_size == 0 ? vp::IO_REQ_DONE : vp::IO_REQ_GRANTED;
}


vp::IoReqStatus Splitter::input_req(vp::Block *__this, vp::IoReq *req)
{
    Splitter *_this = (Splitter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "IO req (req: %p, addr: 0x%llx, size: 0x%llx, write: %d)\n",
        req, (unsigned long long)req->get_addr(),
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0);

    // Refuse while an output still holds a denied chunk (so a new parent can't
    // add a second stuck chunk to the same output); stream otherwise.
    if (_this->any_stuck())
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    uint64_t addr = req->get_addr();
    uint64_t size = req->get_size();

    // Empty request: nothing to forward, trivially done.
    if (size == 0)
    {
        req->set_resp_status(vp::IO_RESP_OK);
        return vp::IO_REQ_DONE;
    }

    // Single-fan-out only: the request must fit in nb_outputs chunks. The
    // fan-out is positional (chunk i -> output_i) and aligns only to
    // output_width, so the bound is on the output_width-misalignment plus
    // size, not on input_width alignment. Larger bursts must be beat-chunked
    // upstream before reaching the splitter.
    if ((addr & (_this->output_width - 1)) + size > _this->input_width)
    {
        _this->trace.fatal(
            "Request needs more than nb_outputs chunks (addr: 0x%llx, "
            "size: 0x%llx, input_width: 0x%llx, output_width: 0x%llx) — "
            "chunk it into beats upstream\n",
            (unsigned long long)addr, (unsigned long long)size,
            (unsigned long long)_this->input_width,
            (unsigned long long)_this->output_width);
        return vp::IO_REQ_DONE;
    }

    _this->gui_log_input(addr, size, req->get_is_write());
    return _this->issue(req);
}


vp::IoRespAck Splitter::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    Splitter *_this = (Splitter *)__this;
    vp::IoReq *parent = req->parent;
    _this->account(req);
    if (parent->remaining_size == 0)
    {
        _this->input_itf.resp(parent);
    }
    return vp::IO_RESP_ACCEPTED;
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

    uint64_t chunk_addr = sub->get_addr();
    uint64_t chunk_size = sub->get_size();

    vp::IoReqStatus st = _this->outputs[id]->req(sub);
    if (st == vp::IO_REQ_DENIED)
    {
        // Still stuck — hold it until the next retry (not yet accepted, so it
        // is still not logged).
        _this->stuck[id] = sub;
        return;
    }

    // Accepted on retry: now it actually goes out — log it once.
    _this->gui_log_output(id, chunk_addr, chunk_size);

    vp::IoReq *parent = sub->parent;
    if (st == vp::IO_REQ_DONE)
    {
        _this->account(sub);
        if (parent->remaining_size == 0)
        {
            _this->input_itf.resp(parent);
        }
    }
    // IO_REQ_GRANTED: folded in later via output_resp.

    // This output is free again; if no output is still stuck, a parent we had
    // refused can now be admitted.
    if (_this->input_needs_retry && !_this->any_stuck())
    {
        _this->input_needs_retry = false;
        _this->input_itf.retry();
    }
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
