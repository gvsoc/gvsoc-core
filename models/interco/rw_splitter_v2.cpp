// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Read/write opcode demultiplexer on the io_v2 protocol.
//
// Port of rw_splitter.cpp to io_v2. Functionally unchanged: requests with
// a write opcode are forwarded to ``output_write``, all other requests
// (reads and read-like atomics) are forwarded to ``output_read``. The
// component is stateless — no buffering, no address mutation, no latency.
//
// What changed vs v1:
//   - Single-master slave port. Replies travel back via
//     ``input_itf.resp(req)`` rather than v1's ``req->resp_port`` lookup.
//   - Status codes are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``.
//   - Output masters are muxed (``output_read`` = id 0, ``output_write`` =
//     id 1) so the shared resp/retry callbacks can identify which
//     downstream fired back.
//   - AXI-like deny/retry handshake: a DENY on the read side is tracked
//     separately from a DENY on the write side. ``input_itf.retry()`` is
//     fired only when the specific output that denied the upstream
//     request retries. This preserves the invariant that a master's
//     pending DENIED request will be resent to the same output it was
//     originally denied on.
//
// Timing model: big-packet, zero-latency. The component never calls
// ``inc_latency`` and never schedules a ClockEvent. Whatever latency
// each downstream reports (sync ``IO_REQ_DONE`` with ``req->latency``
// annotated or async ``resp()`` wall-clock) is observed directly by
// the calling master.

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>

// Pulse a GUI signal to `v` now (+`delay` sub-cycle offset) and back to high-Z
// one cycle later, so each access shows as a one-cycle marker in the timeline.
static inline void gui_pulse(vp::Signal<uint64_t> &s, uint64_t v,
                             int64_t delay, int64_t period)
{
    s.set(v, (int64_t)0, delay);
    s.release(0, delay + period);
}

class RwSplitter : public vp::Component, public vp::DebugMemIf
{
public:
    RwSplitter(vp::ComponentConf &conf);
    void reset(bool active) override;

    // Backdoor debug access (vp/debug_mem.hpp): pure pass-through into the
    // read output. Reads and writes reach the same terminals through
    // symmetric downstream trees, so the read-side map is valid for both.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id, vp::IoRetryChannel);

    static constexpr int ID_READ  = 0;
    static constexpr int ID_WRITE = 1;

    vp::Trace trace;

    vp::IoSlave  input_itf{&RwSplitter::input_req};
    vp::IoMaster output_read_itf{ID_READ,  &RwSplitter::output_retry,
                                  &RwSplitter::output_resp};
    vp::IoMaster output_write_itf{ID_WRITE, &RwSplitter::output_retry,
                                  &RwSplitter::output_resp};

    // Id of the downstream that denied the most recent upstream request,
    // or -1 if no DENY is outstanding. At most one upstream request can be
    // in a DENIED state at a time (single slave port), so one int suffices.
    // Tracked so that a retry from the "other" output (one that was never
    // DENYing us) does NOT trigger a spurious upstream retry — the upstream
    // master would resend the same request, which would go back to the
    // still-stuck side and be denied again.
    int denied_output = -1;

    // --- GUI traces (visible in the model-graph / timeline) ---
    void    gui_log(uint64_t addr, uint64_t size, bool is_write);
    int64_t gui_last_cycle = -1;
    int     gui_nb_same_cycle = 0;
    vp::Signal<uint64_t> gui_active;
    vp::Signal<uint64_t> gui_is_write;
    vp::Signal<uint64_t> gui_read_addr;
    vp::Signal<uint64_t> gui_write_addr;
    vp::Signal<uint64_t> gui_size;
};


RwSplitter::RwSplitter(vp::ComponentConf &config)
    : vp::Component(config),
      gui_active(*this, "active", 1, vp::SignalCommon::ResetKind::HighZ),
      gui_is_write(*this, "is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      gui_read_addr(*this, "output_read/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      gui_write_addr(*this, "output_write/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      gui_size(*this, "size", 64, vp::SignalCommon::ResetKind::HighZ)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input", &this->input_itf);
    this->new_master_port("output_read",  &this->output_read_itf);
    this->new_master_port("output_write", &this->output_write_itf);
}

void RwSplitter::gui_log(uint64_t addr, uint64_t size, bool is_write)
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

    gui_pulse(is_write ? this->gui_write_addr : this->gui_read_addr,
              addr, delay, period);
    gui_pulse(this->gui_size, size, delay, period);
    gui_pulse(this->gui_is_write, is_write ? 1 : 0, delay, period);
    gui_pulse(this->gui_active, 1, delay, period);
}

void RwSplitter::reset(bool active)
{
    if (active)
    {
        this->denied_output = -1;
    }
}


vp::IoReqStatus RwSplitter::input_req(vp::Block *__this, vp::IoReq *req)
{
    RwSplitter *_this = (RwSplitter *)__this;

    bool is_write = req->get_is_write();
    int  output_id = is_write ? ID_WRITE : ID_READ;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Routing IO req (req: %p, addr: 0x%llx, size: 0x%llx, write: %d) to %s\n",
        req, (unsigned long long)req->get_addr(),
        (unsigned long long)req->get_size(),
        is_write ? 1 : 0, is_write ? "write" : "read");

    _this->gui_log(req->get_addr(), req->get_size(), is_write);

    vp::IoMaster &out = is_write ? _this->output_write_itf
                                 : _this->output_read_itf;
    vp::IoReqStatus st = out.req(req);

    if (st == vp::IO_REQ_DENIED)
    {
        _this->denied_output = output_id;
    }
    return st;
}


void RwSplitter::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    RwSplitter *_this = (RwSplitter *)__this;
    // Stateless forward. The request object already carries any latency
    // the downstream annotated on it — nothing to add or restore here.
    _this->input_itf.resp(req);
}


void RwSplitter::output_retry(vp::Block *__this, int id, vp::IoRetryChannel)
{
    RwSplitter *_this = (RwSplitter *)__this;
    if (_this->denied_output == id)
    {
        _this->denied_output = -1;
        _this->input_itf.retry();
    }
}


// Backdoor target behind the read output, or nullptr.
static vp::DebugMemIf *read_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int RwSplitter::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
    bool is_write)
{
    vp::DebugMemIf *child = read_debug_mem(this->output_read_itf);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void RwSplitter::debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *child = read_debug_mem(this->output_read_itf);
    if (child != nullptr)
    {
        child->debug_mem_regions(regions, local_base, window_size, entry_base,
            depth + 1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RwSplitter(config);
}
