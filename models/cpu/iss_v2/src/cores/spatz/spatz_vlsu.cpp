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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include <cpu/iss_v2/include/cores/vector_unit/vector_unit.hpp>

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

    int nb_ports = iss.get_js_config()->get_child_int("vu/nb_ports");
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

    this->ports.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        // Callback
        this->ports[i].set_resp_meth(&VuLsu::data_response);
        this->ports[i].set_grant_meth(&VuLsu::data_grant);
        iss.new_master_port("vlsu_" + std::to_string(i), &this->ports[i], this);
    }

    int nb_outstanding_reqs = iss.get_js_config()->get_child_int("vu/nb_outstanding_reqs");
    this->req_queues.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->req_queues[i] = new vp::Queue(this, "port_" + std::to_string(i) + "_reqs");
    }

    this->requests.resize(nb_ports * nb_outstanding_reqs);

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
        this->stalled = false;
        this->insn_ongoing = 0;

        for (VuLsuPendingInsn &slot : this->insns)
        {
            slot.done = false;
        }

        // Since the request queues are cleared with the reset, we need to put back requests
        // in each queue
        int nb_ports = this->vu.iss.get_js_config()->get_child_int("vu/nb_ports");
        int nb_outstanding_reqs = this->vu.iss.get_js_config()->get_child_int("vu/nb_outstanding_reqs");
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
            this->rob_next[i] = 0;
            this->rob_first[i] = 0;
            this->rob_count[i] = 0;

            for (int j=0; j<nb_outstanding_reqs; j++)
            {
                this->rob[i][j] = VlsuRobEntry();
            }
        }

        while (!this->delayed_bursts.empty())
        {
            this->delayed_bursts.pop();
        }

        this->op_timestamp = -1;
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

    // ON RTL, it takes some time to switch from one instruction to another, and more if it is from
    // load to store, probably due to latency to write to regfile.
    if (this->op_timestamp != -1)
    {
        this->op_timestamp += this->prev_is_write != is_write ? (is_write ? 7 : 3) : (is_write ? 0 : 1);
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

void VuLsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
    VuLsu *_this = (VuLsu *)__this;

    uint64_t size = req->get_size();

    // Prepare the next bursts and potentially switch to next instruction once all burst have been sent
    if (_this->reg_indexed == -1)
    {
        _this->pending_addr += _this->strided ? _this->stride : size;
    }
    else
    {
        _this->pending_elem++;
    }
    _this->pending_size -= size;
    _this->pending_velem += size;

    if (_this->pending_size == 0)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_ongoing];
        PendingInsn *pending_insn = slot.insn;
        
        // Keep the last bus operation time to model the gap before the next one.
        _this->op_timestamp = _this->vu.iss.clock.get_cycles() + 1;

        // Keep one extra cycle before retirement after the last burst is issued.
        pending_insn->timestamp = pending_insn->timestamp + 1;

        // Mark the instruction done once all bursts have been issued.
        slot.done = true;
    }

    // The last burst which stalled the block has just been granted. Resume the bursts.
    _this->stalled = false;
}


void VuLsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    VuLsu *_this = (VuLsu *)__this;

    // We just received the response to one of the burst, set valid=True
    auto *rob_entry = (VlsuRobEntry *)req->get_args()[0];

    if (rob_entry == nullptr || rob_entry->port < 0 || rob_entry->port >= _this->nb_ports ||
        rob_entry->rob_id < 0 || rob_entry->rob_id >= (int)_this->rob[rob_entry->port].size())
    {
        _this->trace.fatal("Invalid VLSU response context (req: %p, rob_entry: %p)\n",
            req, rob_entry);
    }
    
    int req_nb_args = req->arg_current_index();
    if (req_nb_args > 0)
    {
        req->arg_free(req_nb_args);
    }

    _this->rob[rob_entry->port][rob_entry->rob_id].valid = true;

    _this->trace.msg("Received data response (req: %p)\n", req);

    int port = rob_entry->port;
    int rob_id = rob_entry->rob_id;

    // Check if this response is for the oldest request, and potentially end other requests which are waiting for this one
    if (_this->rob_first[port] == rob_id)
    {
        while (_this->rob[port][rob_id].valid == true && _this->rob[port][rob_id].allocated == true){
            
            auto &entry = _this->rob[port][rob_id];

            // In case chaining is enabled, notify that some elements has been handled as it
            // might start an instruction
            VuLsuPendingInsn &slot = *entry.slot;
            PendingInsn *pending_insn = slot.insn;
            iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);

            int first_elem = entry.vstart;
            int nb_elem = entry.size / entry.elem_size;
            int end_elem = first_elem + nb_elem;

            _this->vu.insn_commit(pending_insn, entry.size);
            _this->vu.exec_insn_chunk(insn, pending_insn, first_elem, end_elem, nb_elem);

            entry.slot->nb_pending_bursts--;
            _this->req_queues[port]->push_back(entry.req);
            _this->rob_count[port]--;
            entry.allocated = false;
            _this->rob_first[port] = (_this->rob_first[port] + 1) % _this->rob[port].size();

            _this->trace.msg("Retiring request (req: %p, pending insn bursts: %d)\n", entry.req, entry.slot->nb_pending_bursts);

            // Move to the next oldest request in the ROB
            rob_id = _this->rob_first[port];
        }
    }
}

void VuLsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    VuLsu *_this = (VuLsu *)__this;

    // Check if any synchronous delayed burst need to be terminated
    while (!_this->delayed_bursts.empty() &&
            _this->delayed_bursts.top().timestamp <= _this->vu.iss.clock.get_cycles())
    {
        vp::IoReq *req = _this->delayed_bursts.top().req;
        _this->data_response(_this, req);
        _this->delayed_bursts.pop();
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
    // - if the first waiting instruction past its enqueue cycle,
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
            
            // If so, it becomes the on-going instruction and gets armed with the allowed start of its request issuing phase
            // according to its instruction latency. 
            // Only the active on-going instruction may consume instruction latency.
            pending_insn->timestamp = _this->vu.iss.clock.get_cycles() + insn->latency;
            ((void (*)(VuLsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
            _this->insn_ongoing = _this->insn_first_waiting;
            _this->insn_first_waiting = (_this->insn_first_waiting + 1) % VuLsu::queue_size;
            _this->nb_waiting_insn--;
        }
        
        
    }

    if (_this->pending_size && _this->op_timestamp <= _this->vu.iss.clock.get_cycles() && !_this->stalled)
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

            for (int i=0; i<_this->ports.size(); i++)
            {
                if (_this->pending_size == 0 || _this->stalled) break;

                // Only use this port if it has requests available otherwise it means too many
                // are pending
                if (!_this->req_queues[i]->empty() && _this->rob_count[i] < _this->rob[i].size())
                {
                    uint64_t size;

                    if (_this->strided ||  _this->reg_indexed != -1)
                    {
                        size = _this->elem_size;
                    }
                    else
                    {
                        size = std::min((iss_addr_t)_this->vu.lane_width, _this->pending_size);
                    }

                    _this->trace.msg(vp::Trace::LEVEL_TRACE,
                        "Sending request (id: %d, port: %d, addr: 0x%lx, size: 0x%lx, pending_size: 0x%lx, is_write: %d)\n",
                        pending_insn->id, i, _this->pending_addr, size, _this->pending_size, _this->pending_is_write);

                    _this->event_addr[i].event((uint8_t *)&_this->pending_addr);
                    _this->event_size[i].event((uint8_t *)&size);
                    _this->event_is_write[i].event((uint8_t *)&_this->pending_is_write);

                    // Pop a request from this port queue to limit number of outstanding requests
                    vp::IoReq *req = (vp::IoReq *)_this->req_queues[i]->pop();

                    req->init();

                    iss_reg_t addr = _this->pending_addr;

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
                    rob_entry.vstart = _this->vstart;
                    rob_entry.elem_size = _this->elem_size;
                    rob_entry.size = size;

                    req->arg_push((void *)&rob_entry);

                    req->set_addr(addr);
                    req->set_is_write(_this->pending_is_write);
                    req->set_size(size);

                    slot.nb_pending_bursts++;
                    _this->vstart += size / _this->elem_size;

                    req->set_data(_this->pending_velem);

                    vp::IoReqStatus err = _this->ports[i].req(req);

                    if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
                    {
                        int64_t latency = req->get_full_latency();
                        if (latency == 0)
                        {
                            _this->data_response(_this, req);
                        }
                        else
                        {
                            uint64_t response_timestamp = _this->vu.iss.clock.get_cycles() + latency;
                            _this->delayed_bursts.push({req, response_timestamp});
                            pending_insn->timestamp = std::max(pending_insn->timestamp, response_timestamp);
                        }
                    }
                    else if (err == vp::IO_REQ_DENIED)
                    {
                        // Any denied burst must prevent this block from sending any other burst.
                        // It will resume only once the burst is granted
                        _this->stalled = true;
                        break;
                    }
                    else
                    {
                        // Nothing to for asynchronous requests, they are handled in the callback
                    }


                    // Prepare the next burst if not DENIED
                    if (err != vp::IO_REQ_DENIED)
                    {
                        if (_this->reg_indexed == -1)
                        {
                            _this->pending_addr += _this->strided ? _this->stride : size;
                        }
                        else
                        {
                            _this->pending_elem++;
                        }
                        _this->pending_size -= size;
                        _this->pending_velem += size;

                        // Switch to next instruction once all burst have been sent
                        if (_this->pending_size == 0)
                        {
                            // Keep the last bus operation time to model the gap before the next one.
                            _this->op_timestamp = _this->vu.iss.clock.get_cycles() + 1;

                            // Keep one extra cycle before retirement after the last burst is issued.
                            pending_insn->timestamp = pending_insn->timestamp + 1;

                            // Mark the instruction done once all bursts have been issued.
                            slot.done = true;
                        }
                    }
                }
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
