/*
 * Copyright (C) 2020 SAS, ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * io_v2 variant of the Spatz vector LSU. Functionally equivalent to the v1
 * model in spatz_vlsu.cpp; the IO-side plumbing follows io_v2 semantics:
 *
 *   - master ports are vp::IoMaster from <vp/itf/io_v2.hpp>, retry/resp
 *     callbacks passed at port construction (muxed variant so one pair of
 *     callbacks dispatches by port id)
 *   - request statuses are IO_REQ_DONE / IO_REQ_GRANTED / IO_REQ_DENIED;
 *     errors travel on the response status (IO_RESP_OK / IO_RESP_INVALID)
 *   - a DENIED request is parked on its port and re-issued synchronously
 *     inside the retry() callback, as required by deny/retry arbiters such
 *     as interco.log_ico_v2 (their accept window is only open for the
 *     duration of the synchronous retry call). Contrary to the v1 model,
 *     a denied burst stalls only its own port: the other ports keep
 *     streaming, matching the per-port valid/ready handshake of the RTL
 *     TCDM interconnect.
 *
 * Burst bookkeeping (per-port in-order ROB, delayed synchronous responses,
 * VRF commit for chaining) mirrors the v1 implementation. The request is
 * associated to its ROB entry through req->initiator since io_v2 removed
 * the argument stack.
 */

#include <cpu/iss_v2/include/cores/vector_unit/vector_unit.hpp>

// Cycle the RTL VLSU spends between two vector memory instructions. It is a
// structural one, not a fitted number: in spatz_vlsu.sv a port's element
// counter reaching its maximum is registered (mem_port_finished_q), the next
// instruction is only accepted the cycle after (mem_spatz_req_ready), and its
// counters load then (mem_counter_load), so no request can go out in that
// cycle. The steady-state cost of a unit-stride load is therefore K + 1,
// where K = vl/NrMemPorts is the number of per-port requests.
//
// Measured differentially against the RTL (tests/calibration/vlsu_rate),
// cycles per instruction against the K+1 prediction:
//
//     m1: 3.27 (3)   m2: 4.92 (5)   m4: 9.04 (9)   m8: 17.10 (17)
//
// Loads only: the store path already reproduces the RTL (12.10 measured
// against 12.16 modelled at m4), so charging it there would overshoot.
#define VLSU_INSN_DISPATCH_COST 1

VuLsu::VuLsu(Vu &vu, Iss &iss)
: VuBlock(&vu, "vlsu"), vu(vu),
nb_pending_insn(*this, "nb_pending_insn", 8, true),
fsm_event(this, &VuLsu::fsm_handler),
event_label(*this, "label", 0, gv::Vcd_event_type_string)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);

    this->insns.resize(VuLsu::queue_size);

    int nb_ports = iss.cfg.vu.nb_ports;
    this->nb_ports = nb_ports;

    this->event_addr.resize(nb_ports);
    this->event_size.resize(nb_ports);
    this->event_is_write.resize(nb_ports);

    for (int i=0; i<nb_ports; i++)
    {
        this->traces.new_trace_event("port_" + std::to_string(i) + "/addr",
            &this->event_addr[i], 64);
        this->traces.new_trace_event("port_" + std::to_string(i) + "/size",
            &this->event_size[i], 64);
        this->traces.new_trace_event("port_" + std::to_string(i) + "/is_write",
            &this->event_is_write[i], 1);
    }

    // io_v2 IoMaster has no default constructor — every port must be
    // built with its retry/resp callbacks. Use the muxed variant so a
    // single pair of callbacks demuxes by port id.
    this->ports.reserve(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->ports.emplace_back(i, &VuLsu::port_retry_muxed, &VuLsu::port_resp_muxed);
        iss.new_master_port("vlsu_" + std::to_string(i), &this->ports[i], this);
    }

    int nb_outstanding_reqs = iss.cfg.vu.nb_outstanding_reqs;
    this->req_queues.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->req_queues[i] = new vp::Queue(this, "port_" + std::to_string(i) + "_reqs");
    }

    this->requests.resize(nb_ports * nb_outstanding_reqs);
    this->denied_reqs.resize(nb_ports);

    this->rob.resize(nb_ports);
    this->rob_next.resize(nb_ports);
    this->rob_first.resize(nb_ports);
    this->rob_count.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->rob[i].resize(nb_outstanding_reqs);
    }
}

void VuLsu::start()
{
}

void VuLsu::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->event_active.event(&zero);
        for (int i=0; i<nb_ports; i++)
        {
            this->event_addr[i].event(&zero);
            this->event_size[i].event(&zero);
            this->event_is_write[i].event(&zero);
        }
        this->insn_first = 0;
        this->insn_first_waiting = 0;
        this->insn_last = 0;
        this->nb_waiting_insn = 0;
        this->pending_size = 0;
        this->insn_ongoing = 0;

        for (VuLsuPendingInsn &slot : this->insns)
        {
            slot.done = false;
        }

        // Since the request queues are cleared with the reset, we need to put back requests
        // in each queue
        int nb_ports = this->vu.iss.cfg.vu.nb_ports;
        int nb_outstanding_reqs = this->vu.iss.cfg.vu.nb_outstanding_reqs;
        int req_id = 0;
        for (int i=0; i<nb_ports; i++)
        {
            for (int j=0; j<nb_outstanding_reqs; j++)
            {
                this->req_queues[i]->push_back(&this->requests[req_id++]);
            }
        }

        for (int i=0; i<nb_ports; i++)
        {
            this->denied_reqs[i] = nullptr;
            this->rob_next[i] = 0;
            this->rob_first[i] = 0;
            this->rob_count[i] = 0;

            for (size_t j=0; j<this->rob[i].size(); j++)
            {
                this->rob[i][j] = VlsuRobEntry();
            }
        }

        while (!this->delayed_bursts.empty())
        {
            this->delayed_bursts.pop();
        }

        this->op_timestamp = -1;
        this->prev_is_indexed = false;
    }
}

void VuLsu::enqueue_insn(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->vu.iss.exec.get_insn(pending_insn->entry);
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx, id: %d)\n",
        insn->addr, pending_insn->id);
    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&insn->addr);

    // Push the instruction in the queue for the FSM. The +1 keeps it from being executed immediately in the same cycle.
    pending_insn->timestamp = this->vu.iss.clock.get_cycles() + 1;
    VuLsuPendingInsn &slot = this->insns[this->insn_last];
    slot.enqueue_cycle = this->vu.iss.clock.get_cycles();
    this->insn_last = (this->insn_last + 1) % VuLsu::queue_size;
    slot.insn = pending_insn;
    slot.nb_pending_bursts = 0;
    slot.done = false;
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn++;

    this->fsm_event.enable();
}

void VuLsu::isa_init()
{
    // Attach handlers to instructions so that we can quickly handle load and stores differently
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_load;
    }
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_store;
    }
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vload_strided"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_load_strided;
    }
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vstore_strided"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_store_strided;
    }
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vload_indexed"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_load_indexed;
    }
    for (iss_decoder_item_t *insn: *this->vu.iss.decode.get_insns_from_tag("vstore_indexed"))
    {
        insn->u.insn.block_handler = (void *)&VuLsu::handle_insn_store_indexed;
    }
}

void VuLsu::handle_insn_load_strided(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], true, _this->insns[_this->insn_first_waiting].insn->reg_2);
}

void VuLsu::handle_insn_store_strided(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], true, _this->insns[_this->insn_first_waiting].insn->reg_3);
}

void VuLsu::handle_insn_load_indexed(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], false, 0, insn->in_regs[1]);
}

void VuLsu::handle_insn_store_indexed(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], false, 0, insn->in_regs[2]);
}

void VuLsu::handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride, iss_reg_t stride, int reg_indexed)
{
    // A load or store instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = this->vu.iss.vector.sewb;
    unsigned int lmul = this->vu.iss.vector.lmul;
    this->pending_vreg = reg;
    this->pending_velem = velem_get(&this->vu.iss, reg, 0, sewb, lmul);
    this->vstart = this->vu.iss.csr.vstart.value;
    this->pending_addr = this->insns[this->insn_first_waiting].insn->reg;
    this->pending_is_write = is_write;
    int inst_elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    int elem_size = reg_indexed != -1 ? sewb : inst_elem_size;
    this->pending_size = (this->vu.iss.csr.vl.value - this->vu.iss.csr.vstart.value) * elem_size;
    this->stride = stride;
    this->strided = do_stride;
    this->elem_size = elem_size;
    this->inst_elem_size = inst_elem_size;
    this->reg_indexed = reg_indexed;
    this->pending_elem = 0;

    // Masked access: v0 selects which elements take part. On RTL this never
    // changes the request stream -- spatz_vlsu.sv issues a request for every
    // element position and applies v0 as a byte strobe on stores
    // (mem_req_strb) and as a VRF write byte enable on loads (vrf_req_d.wbe)
    // -- so the model applies it the same way, per request, at no cycle cost.
    this->masked = insn->desc->has_vm && insn->uim[0] == 0;

    // A unit-stride access whose base address is not aligned on the lane
    // width is issued one element per request, like on RTL where the VLSU
    // falls back to a single-element mode. This also keeps every request
    // within a single TCDM bank word, since the interconnect does not split
    // requests. Elements themselves must be naturally aligned, like on RTL.
    this->single_element = !do_stride && reg_indexed == -1 &&
        (this->pending_addr & (this->vu.lane_width - 1)) != 0;

    if (this->single_element)
    {
        // Each port owns full lane-width words of the vector, like on RTL,
        // so that elements sharing a word are issued on the same port on
        // consecutive cycles instead of conflicting on the same memory bank
        // in the same cycle.
        this->se_base_addr = this->pending_addr;
        this->se_base_velem = this->pending_velem;
        this->se_base_vstart = this->vstart;
        this->se_nb_elems = this->pending_size / elem_size;
        this->se_port_count.assign(this->nb_ports, 0);
    }

    // ON RTL, it takes some time to switch from one instruction to another, and more if it is from
    // load to store, probably due to latency to write to regfile.
    // Same-type transitions pace at 1 cycle (the +1 below); switches add
    // their calibrated turnaround on top.
    // Re-verified against the RTL (e64, LMUL=4): an alternating load/store
    // stream costs 13.7 cycles/insn and a load/load/store/store stream
    // 12.6, against 9.8 for pure loads and 14.8 for pure stores. Most of
    // the 7/3 is absorbed by the ongoing transfer, and what remains matches
    // those two streams to within 0.5 cycles; setting the penalties to zero
    // makes them 14% and 8% fast, so they are load-bearing as they stand.
    if (this->op_timestamp != -1)
    {
        this->op_timestamp += this->prev_is_write != is_write ? (is_write ? 7 : 3) : 0;
    }

    this->prev_is_write = is_write;
    this->started = false;
}

void VuLsu::handle_insn_load(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0]);
}

void VuLsu::handle_insn_store(VuLsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1]);
}

// A denied request's port got told the downstream is ready again. The io_v2
// contract requires the re-issue to happen synchronously inside this
// callback — deny/retry arbiters (log_ico_v2) only forward inline during the
// synchronous retry call.
void VuLsu::port_retry_muxed(vp::Block *__this, int id, vp::IoRetryChannel)
{
    VuLsu *_this = (VuLsu *)__this;

    vp::IoReq *req = _this->denied_reqs[id];
    if (req == nullptr)
    {
        // Spurious retry (e.g. broadcast "ready" from a slave which denied
        // someone else). Nothing is parked here, nothing to do.
        return;
    }

    _this->denied_reqs[id] = nullptr;

    vp::IoReqStatus err = _this->ports[id].req(req);

    if (err == vp::IO_REQ_DENIED)
    {
        // Lost the election again; keep holding for the next retry.
        _this->denied_reqs[id] = req;
        return;
    }

    if (err == vp::IO_REQ_DONE)
    {
        _this->handle_done(req);
    }
    // GRANTED: the response callback will complete the burst.

    // The port is free again, resume issuing from the FSM.
    _this->fsm_event.enable();
}

// io_v2 response — fires when an async (GRANTED) request completes.
vp::IoRespAck VuLsu::port_resp_muxed(vp::Block *__this, vp::IoReq *req, int id)
{
    VuLsu *_this = (VuLsu *)__this;

    _this->burst_done(req);
    _this->fsm_event.enable();

    return vp::IO_RESP_ACCEPTED;
}

void VuLsu::handle_done(vp::IoReq *req)
{
    if (req->get_resp_status() == vp::IO_RESP_INVALID)
    {
        this->trace.fatal("Invalid request (req: %p, addr: 0x%lx, size: 0x%lx, is_write: %d)\n",
            req, req->get_addr(), req->get_size(), req->get_is_write());
        return;
    }

    int64_t latency = req->get_full_latency();
    if (latency == 0)
    {
        this->burst_done(req);
    }
    else
    {
        // The request was served synchronously with a latency annotation;
        // use a priority queue to model the synchronous response delay.
        VlsuRobEntry *rob_entry = (VlsuRobEntry *)req->initiator;
        uint64_t response_timestamp = this->vu.iss.clock.get_cycles() + latency;
        this->delayed_bursts.push({req, response_timestamp});
        PendingInsn *pending_insn = rob_entry->slot->insn;
        pending_insn->timestamp = std::max<int64_t>(pending_insn->timestamp,
            (int64_t)response_timestamp);
    }
}

void VuLsu::burst_done(vp::IoReq *req)
{
    auto *rob_entry = (VlsuRobEntry *)req->initiator;

    if (rob_entry == nullptr || rob_entry->port < 0 || rob_entry->port >= this->nb_ports ||
        rob_entry->rob_id < 0 || rob_entry->rob_id >= (int)this->rob[rob_entry->port].size())
    {
        this->trace.fatal("Invalid VLSU response context (req: %p, rob_entry: %p)\n",
            req, rob_entry);
    }

    // We just received the response to one of the requests, set valid=True
    this->rob[rob_entry->port][rob_entry->rob_id].valid = true;

    this->trace.msg("Received data response (req: %p)\n", req);

    int port = rob_entry->port;
    int rob_id = rob_entry->rob_id;

    // Check if this response is for the oldest request, and potentially end other requests
    // which were waiting for this one
    if (this->rob_first[port] == rob_id)
    {
        while (this->rob[port][rob_id].valid == true && this->rob[port][rob_id].allocated == true)
        {
            auto &entry = this->rob[port][rob_id];

            // In case chaining is enabled, notify that some elements have been handled as it
            // might start an instruction
            VuLsuPendingInsn &slot = *entry.slot;
            PendingInsn *pending_insn = slot.insn;
            iss_insn_t *insn = this->vu.iss.exec.get_insn(pending_insn->entry);

            int first_elem = entry.vstart;
            int nb_elem = entry.size / entry.elem_size;
            int end_elem = first_elem + nb_elem;

            // Masked load: the data landed in the entry's scratch buffer, so
            // only the active elements reach the register file.
            if (entry.masked && entry.vrf_dest != nullptr)
            {
                for (int i = 0; i < entry.size; i++)
                {
                    if (entry.strb[i])
                    {
                        entry.vrf_dest[i] = entry.scratch[i];
                    }
                }
                entry.vrf_dest = nullptr;
            }

            this->vu.insn_commit(pending_insn, entry.size);
            this->vu.exec_insn_chunk(insn, pending_insn, first_elem, end_elem, nb_elem);

            entry.slot->nb_pending_bursts--;
            this->req_queues[port]->push_back(entry.req);
            this->rob_count[port]--;
            entry.allocated = false;
            this->rob_first[port] = (this->rob_first[port] + 1) % this->rob[port].size();

            this->trace.msg("Retiring request (req: %p, pending insn bursts: %d)\n",
                entry.req, entry.slot->nb_pending_bursts);

            // Move to the next oldest request in the ROB
            rob_id = this->rob_first[port];
        }
    }
}

void VuLsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    VuLsu *_this = (VuLsu *)__this;

    // Check if any synchronous delayed burst needs to be terminated
    while (!_this->delayed_bursts.empty() &&
            _this->delayed_bursts.top().timestamp <= (uint64_t)_this->vu.iss.clock.get_cycles())
    {
        vp::IoReq *req = _this->delayed_bursts.top().req;
        _this->delayed_bursts.pop();
        _this->burst_done(req);
    }

    // In case nothing is on-going, disable the FSM
    if (_this->nb_pending_insn.get() == 0)
    {
        uint8_t zero = 0;
        _this->event_queue.event_highz();
        _this->event_pc.event_highz();
        _this->event_active.event(&zero);
        for (int i=0; i<_this->nb_ports; i++)
        {
            _this->event_addr[i].event_highz();
            _this->event_size[i].event_highz();
            _this->event_is_write[i].event(&zero);
        }
        _this->event_label.dump_highz();

        _this->fsm_event.disable();
    }

    // Check if the first waiting instruction can be started:
    // - if the current on-going instruction finished issuing all requests i.e. _this->pending_size == 0;
    // - if the first waiting instruction is past its enqueue cycle,
    // - and is ready dependency-wise.
    if (_this->nb_waiting_insn > 0 && _this->pending_size == 0)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;
        iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);

        if (pending_insn->timestamp <= _this->vu.iss.clock.get_cycles() && _this->vu.insn_ready(pending_insn))
        {
#ifdef CONFIG_GVSOC_STATS_ACTIVE
            // Instruction leaves the waiting queue and its memory op is set up:
            // real execution starts now, stamped for the per-label duration
            // accounted at Vu::insn_end.
            if (_this->vu.stats_enabled && pending_insn->exec_start_cycle < 0)
            {
                pending_insn->exec_start_cycle = _this->vu.iss.clock.get_cycles();
            }
#endif

            // If so, it becomes the on-going instruction and gets armed with the allowed start of
            // its request issuing phase according to its instruction latency.
            // Only the active on-going instruction may consume instruction latency.
            // A load following a load streams in the RTL: its dispatch
            // (NrParallelInstructions slots) happened while the previous one
            // was still issuing requests, so the cycles already spent waiting
            // in the queue (beyond the enqueue guard) count against the
            // dispatch latency. The discount bottoms out at
            // VLSU_INSN_DISPATCH_COST, not at zero: even in a perfect stream
            // the RTL VLSU cannot start the next instruction's requests in
            // the same cycle it retires the previous one's. Measured on the
            // RTL (e64, LMUL=4, 256 B per access = 8 cycles of data): a load
            // stream costs 9.3-9.8 cycles/insn, i.e. ~1 cycle of
            // per-instruction overhead, flat in LMUL (m8: 16.5 vs 16 of
            // data) and in core count (1 core: 8.6 vs 2 cores: 9.0).
            // Other transitions (stores, load/store switches) keep the full
            // calibrated pacing.
            int64_t lat = insn->latency;
            void *handler = insn->decoder_item->u.insn.block_handler;
            bool is_load = handler == (void *)&VuLsu::handle_insn_load ||
                handler == (void *)&VuLsu::handle_insn_load_strided ||
                handler == (void *)&VuLsu::handle_insn_load_indexed;
            if (_this->op_timestamp != -1 && !_this->prev_is_write && is_load)
            {
                int64_t elapsed = _this->vu.iss.clock.get_cycles() - slot.enqueue_cycle - 1;
                lat = lat - elapsed;
                if (lat < 0)
                {
                    lat = 0;
                }
            }
            // Stores pay one extra dispatch cycle to read their data out of
            // the VRF before the requests can go out. Like the indexed
            // startup, it only shows in back-to-back store streams;
            // interleaved with other work it is hidden.
            if (!is_load && _this->prev_is_write)
            {
                lat += 1;
            }
            // Indexed accesses first fetch the index vector from the VRF
            // before the address generation can start. This only shows in
            // back-to-back indexed streams, where the index fetches compete
            // for the VRF port; interleaved with other work it is hidden.
            // RTL delta over a unit-stride load stream: 12.9 - 9.8 = 3.1
            // cycles (e64, LMUL=4).
            bool is_indexed = handler == (void *)&VuLsu::handle_insn_load_indexed ||
                handler == (void *)&VuLsu::handle_insn_store_indexed;
            if (is_indexed && _this->prev_is_indexed)
            {
                lat += 3;
            }
            // The instruction handover cycle (see the constant above),
            // paid by every load whatever came before it -- it is the
            // counter reload of spatz_vlsu.sv, not a property of the
            // transition. Stores are not charged: the store path already
            // reproduces the RTL rate (12.10 measured, 12.16 modelled).
            if (is_load)
            {
                lat += VLSU_INSN_DISPATCH_COST;
            }
            _this->prev_is_indexed = is_indexed;
            pending_insn->timestamp = _this->vu.iss.clock.get_cycles() + lat;
            ((void (*)(VuLsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
            _this->insn_ongoing = _this->insn_first_waiting;
            _this->insn_first_waiting = (_this->insn_first_waiting + 1) % VuLsu::queue_size;
            _this->nb_waiting_insn--;
        }
    }

    if (_this->pending_size && _this->op_timestamp <= _this->vu.iss.clock.get_cycles())
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_ongoing];
        PendingInsn *pending_insn = slot.insn;

        // If the on-going instruction is ready and its instruction latency has elapsed,
        // try to send requests to available ports
        if (pending_insn->timestamp <= _this->vu.iss.clock.get_cycles() && _this->vu.insn_ready(pending_insn))
        {
            iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);

            if (!_this->started)
            {
                _this->started = true;
                _this->event_label.dump(insn->desc->label);
                _this->event_pc.event((uint8_t *)&insn->addr);
            }

            for (size_t i=0; i<_this->ports.size(); i++)
            {
                if (_this->pending_size == 0) break;

                // Skip a port with a parked denied request — contrary to the
                // v1 model a denied burst only stalls its own port. Also skip
                // it when too many requests are outstanding.
                if (_this->denied_reqs[i] != nullptr || _this->req_queues[i]->empty() ||
                    _this->rob_count[i] >= (int)_this->rob[i].size())
                {
                    continue;
                }

                uint64_t size;

                // In single-element mode, get the next element owned by
                // this port, and skip the port once it went through all
                // its elements
                int se_elem = -1;
                if (_this->single_element)
                {
                    int elems_per_word = _this->vu.lane_width / _this->elem_size;
                    int count = _this->se_port_count[i];
                    se_elem = (count / elems_per_word) * elems_per_word * _this->nb_ports +
                        i * elems_per_word + (count % elems_per_word);
                    if (se_elem >= _this->se_nb_elems)
                    {
                        continue;
                    }
                }

                if (_this->strided || _this->reg_indexed != -1 || _this->single_element)
                {
                    size = _this->elem_size;
                }
                else
                {
                    size = std::min((iss_addr_t)_this->vu.lane_width, _this->pending_size);
                }

                _this->trace.msg(vp::Trace::LEVEL_TRACE,
                    "Sending request (id: %d, port: %zd, addr: 0x%lx, size: 0x%lx, pending_size: 0x%lx, is_write: %d)\n",
                    pending_insn->id, i, _this->pending_addr, size, _this->pending_size, _this->pending_is_write);

                _this->event_addr[i].event((uint8_t *)&_this->pending_addr);
                _this->event_size[i].event((uint8_t *)&size);
                _this->event_is_write[i].event((uint8_t *)&_this->pending_is_write);

                // Pop a request from this port queue to limit the number of outstanding requests
                vp::IoReq *req = (vp::IoReq *)_this->req_queues[i]->pop();

                // io_v2 reset of per-send fields (latency/duration). Status starts OK; the
                // slave will downgrade it to IO_RESP_INVALID on error.
                req->prepare();
                req->set_resp_status(vp::IO_RESP_OK);

                iss_reg_t addr = _this->pending_addr;

                if (_this->single_element)
                {
                    addr = _this->se_base_addr + se_elem * _this->elem_size;
                }

                if (_this->reg_indexed != -1)
                {
                    uint64_t offset = velem_get_value(&_this->vu.iss, _this->reg_indexed, _this->pending_elem,
                        _this->inst_elem_size, _this->vu.iss.vector.lmul);
                    addr += offset;
                }

                int rob_id = _this->rob_next[i];
                _this->rob_next[i] = (rob_id + 1) % _this->rob[i].size();
                _this->rob_count[i]++;

                VlsuRobEntry &rob_entry = _this->rob[i][rob_id];
                rob_entry.port = i;
                rob_entry.rob_id = rob_id;
                rob_entry.allocated = true;
                rob_entry.valid = false;
                rob_entry.req = req;
                rob_entry.slot = &slot;
                rob_entry.vreg = _this->pending_vreg;
                rob_entry.vstart = _this->single_element ?
                    _this->se_base_vstart + se_elem : _this->vstart;
                rob_entry.elem_size = _this->elem_size;
                rob_entry.size = size;

                // io_v2 has no argument stack; the back-link to the ROB entry
                // travels in the initiator field.
                req->initiator = (void *)&rob_entry;

                req->set_addr(addr);
                req->set_is_write(_this->pending_is_write);
                req->set_size(size);

                slot.nb_pending_bursts++;
                _this->vstart += size / _this->elem_size;

                uint8_t *velem = _this->single_element ?
                    _this->se_base_velem + se_elem * _this->elem_size : _this->pending_velem;

                rob_entry.masked = _this->masked;
                if (!_this->masked)
                {
                    req->set_data(velem);
                }
                else
                {
                    // Expand v0 into a per-byte enable covering the elements
                    // this request carries. A store hands it to the target as
                    // the request's strobe; a load lands in the entry's
                    // scratch buffer and only the enabled bytes are merged
                    // into the register file when the entry retires, so the
                    // inactive elements keep their value (mask-undisturbed).
                    int nb_elem = size / _this->elem_size;
                    for (int e = 0; e < nb_elem; e++)
                    {
                        bool active = velem_is_active(&_this->vu.iss,
                            rob_entry.vstart + e, 0);
                        memset(&rob_entry.strb[e * _this->elem_size],
                            active ? 0xff : 0, _this->elem_size);
                    }

                    if (_this->pending_is_write)
                    {
                        req->set_data(velem);
                        req->set_strb(rob_entry.strb);
                    }
                    else
                    {
                        rob_entry.vrf_dest = velem;
                        req->set_data(rob_entry.scratch);
                    }
                }

                // The burst is dispatched to this port whatever the downstream
                // answers: a DENIED burst is parked on the port and re-issued
                // on retry, so the sequencing state advances now in all cases.
                if (_this->single_element)
                {
                    _this->se_port_count[i]++;
                }
                else if (_this->reg_indexed == -1)
                {
                    _this->pending_addr += _this->strided ? _this->stride : size;
                }
                else
                {
                    _this->pending_elem++;
                }
                _this->pending_size -= size;
                _this->pending_velem += size;

                // Switch to the next instruction once all bursts have been dispatched
                if (_this->pending_size == 0)
                {
                    // Keep the last bus operation time to model the gap before the next one.
                    _this->op_timestamp = _this->vu.iss.clock.get_cycles() + 1;

                    // Keep one extra cycle before retirement after the last burst is issued.
                    pending_insn->timestamp = pending_insn->timestamp + 1;

                    // Mark the instruction done once all bursts have been issued.
                    slot.done = true;
                }

                vp::IoReqStatus err = _this->ports[i].req(req);

                if (err == vp::IO_REQ_DONE)
                {
                    _this->handle_done(req);
                }
                else if (err == vp::IO_REQ_DENIED)
                {
                    // Park the request on its port; it is re-issued
                    // synchronously inside the retry() callback.
                    _this->denied_reqs[i] = req;
                }
                // GRANTED: nothing to do, completion arrives via port_resp_muxed.
            }
        }
    }

    // Check if the first enqueued instruction must be removed.
    if (_this->nb_pending_insn.get() > 0)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_first];
        PendingInsn *pending_insn = slot.insn;
        if (slot.done && slot.nb_pending_bursts == 0 &&
            pending_insn->timestamp <= _this->vu.iss.clock.get_cycles())
        {
            _this->event_label.dump_highz_next();
            _this->insn_first = (_this->insn_first + 1) % VuLsu::queue_size;
            _this->nb_pending_insn.dec(1);
            slot.done = false;
            pending_insn->timestamp = _this->vu.iss.clock.get_cycles() + 1;
            _this->vu.insn_end(pending_insn);
        }
    }
}
