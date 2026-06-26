// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Beat -> big-packet collapse adapter on the io_v2 protocol.
//
// The inverse of IoV2BeatAdapter. It makes a beat-streaming slave (e.g. a
// KIND_BEAT router) look like a plain big-packet slave to a big-packet master.
// A master that does not speak beats (a functional/untimed router, a CPU LSU,
// ...) must never see the per-cycle beat stream of the timed layer below it;
// this adapter is the boundary that hides it.
//
// Ownership / lifetime
// --------------------
//
// This adapter is the boundary between two ownership regimes:
//
//   - Upstream (master side): the classic round-trip. The master owns its
//     request object; the adapter hands that very object back via resp(), and
//     the master frees/reuses it.
//   - Downstream (beat side): new/delete. The adapter allocates a fresh
//     request per master access and forwards THAT downstream. The beat slave
//     (and the terminal slave behind it) own it and free it; for a multi-beat
//     read the response arrives as N independent beat objects which this
//     adapter consumes and frees. The master's own request never travels
//     downstream, so nothing downstream can free it out from under the master.
//
// The downstream request's data pointer aliases the master's buffer, so a read
// lands its bytes directly where the master expects them and a write reads its
// payload straight from there — only the request/response *objects* differ.
//
// Timing: zero added latency. A multi-beat read completes (one resp upstream)
// on the cycle its last beat arrives, which is exactly the burst's latency. An
// inline DONE from downstream is relayed inline.
//
// Single outstanding request. A second master access while one is in flight is
// DENIED and retried when the first completes. Masters behind this boundary
// (in-order cores, single-refill icaches) are themselves single-outstanding,
// so this is not a throughput limit in practice.

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>

class IoV2BeatCollapseAdapter : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2BeatCollapseAdapter(vp::ComponentConf &config);
    void reset(bool active) override;

    // Backdoor debug path: the adapter is invisible, forward to the downstream.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    static vp::IoReqStatus in_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   out_resp(vp::Block *__this, vp::IoReq *req);
    static void            out_retry(vp::Block *__this, vp::IoRetryChannel channel);

    void maybe_retry_input();

    vp::Trace trace;

    vp::IoSlave  in{&IoV2BeatCollapseAdapter::in_req};
    vp::IoMaster out{&IoV2BeatCollapseAdapter::out_retry,
                     &IoV2BeatCollapseAdapter::out_resp};

    // The master request currently in flight (single outstanding), handed back
    // on completion. Null when idle.
    vp::IoReq *pending = nullptr;
    // We refused a master access (busy or downstream-denied) and owe it a
    // retry() once we can accept again.
    bool need_retry = false;
};


IoV2BeatCollapseAdapter::IoV2BeatCollapseAdapter(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);
}

void IoV2BeatCollapseAdapter::reset(bool active)
{
    if (active)
    {
        this->pending = nullptr;
        this->need_retry = false;
    }
}

void IoV2BeatCollapseAdapter::maybe_retry_input()
{
    if (this->need_retry && this->pending == nullptr)
    {
        this->need_retry = false;
        this->in.retry();
    }
}


vp::IoReqStatus IoV2BeatCollapseAdapter::in_req(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatCollapseAdapter *>(__this);

    // Single outstanding: refuse while busy; retried on completion.
    if (self->pending != nullptr)
    {
        self->need_retry = true;
        return vp::IO_REQ_DENIED;
    }

    self->trace.msg(vp::Trace::LEVEL_TRACE,
        "Submit (req=%p, addr=0x%lx, size=%lu, write=%d)\n",
        req, req->get_addr(), req->get_size(), req->get_is_write() ? 1 : 0);

    // Forward a fresh downstream request — never the master's own object, which
    // the beat slave (or the terminal slave behind it) may free. Its data
    // pointer aliases the master's buffer so payload lands in/comes from the
    // right place.
    vp::IoReq *dn = new vp::IoReq();
    dn->set_addr(req->get_addr());
    dn->set_size(req->get_size());
    dn->set_data(req->get_data());
    dn->set_opcode(req->get_opcode());
    dn->set_is_write(req->get_is_write());
    dn->is_first = true;
    dn->is_last  = true;
    dn->burst_id = req->burst_id;
    dn->initiator = nullptr;
    dn->set_resp_status(vp::IO_RESP_OK);

    vp::IoReqStatus st = self->out.req(dn);

    if (st == vp::IO_REQ_DONE)
    {
        // Inline completion: bytes already in the master's buffer (aliased).
        // Fold the downstream's full timing (head latency + bandwidth duration)
        // into the master's latency field. The master's request was prepare()'d
        // (duration==0), so both get_latency() and get_full_latency() return the
        // correct total — safe whichever the identity master reads.
        req->set_resp_status(dn->get_resp_status());
        req->set_latency(dn->get_full_latency());
        delete dn;
        return vp::IO_REQ_DONE;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        // Downstream busy: drop the downstream object; the master holds its own
        // request and re-sends it on our retry(), where we build a fresh one.
        delete dn;
        self->need_retry = true;
        return vp::IO_REQ_DENIED;
    }

    // GRANTED: the beat slave will respond (N read beats, or one write ack).
    self->pending = req;
    return vp::IO_REQ_GRANTED;
}


vp::IoRespAck IoV2BeatCollapseAdapter::out_resp(vp::Block *__this, vp::IoReq *req)
{
    auto *self = static_cast<IoV2BeatCollapseAdapter *>(__this);
    vp::IoReq *master = self->pending;

    // `req` is always a downstream-owned object — either an independent read
    // beat or the round-tripped downstream write/single-beat request — never
    // the master's own request (which we never forwarded). Latch any error onto
    // the master, free the downstream object, and on the last beat hand the
    // master's request back as one big-packet response.
    if (req->get_resp_status() == vp::IO_RESP_INVALID)
    {
        master->set_resp_status(vp::IO_RESP_INVALID);
    }
    bool last = req->is_last;
    delete req;

    if (last)
    {
        self->pending = nullptr;
        master->is_first = true;
        master->is_last  = true;
        self->in.resp(master);
        self->maybe_retry_input();
    }

    return vp::IO_RESP_ACCEPTED;
}


void IoV2BeatCollapseAdapter::out_retry(vp::Block *__this, vp::IoRetryChannel)
{
    auto *self = static_cast<IoV2BeatCollapseAdapter *>(__this);
    // The downstream that denied our forward is ready again. If we owe the
    // master a retry and can accept now, let it re-send (synchronously).
    self->maybe_retry_input();
}


// ---- backdoor debug: transparent pass-through to the downstream ------------

static vp::DebugMemIf *downstream_debug_mem(vp::IoMaster &itf)
{
    std::vector<vp::SlavePort *> finals = itf.get_final_ports();
    if (finals.empty() || finals[0]->get_owner() == nullptr)
    {
        return nullptr;
    }
    return finals[0]->get_owner()->debug_mem_if();
}

int IoV2BeatCollapseAdapter::debug_mem_access(uint64_t addr, uint8_t *data,
    uint64_t size, bool is_write)
{
    vp::DebugMemIf *child = downstream_debug_mem(this->out);
    if (child == nullptr)
    {
        return -1;
    }
    return child->debug_mem_access(addr, data, size, is_write);
}

void IoV2BeatCollapseAdapter::debug_mem_regions(
    std::vector<vp::DebugMemRegion> &regions, uint64_t local_base,
    uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }
    vp::DebugMemIf *child = downstream_debug_mem(this->out);
    if (child != nullptr)
    {
        child->debug_mem_regions(regions, local_base, window_size, entry_base,
            depth + 1);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2BeatCollapseAdapter(config);
}
