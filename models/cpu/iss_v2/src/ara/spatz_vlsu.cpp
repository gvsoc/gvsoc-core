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

#include "cpu/iss_v2/include/cores/ara/ara.hpp"

AraVlsu::AraVlsu(Ara &ara, Iss &iss)
: AraBlock(&ara, "vlsu"), ara(ara),
nb_pending_insn(*this, "nb_pending_insn", 8, true),
fsm_event(this, &AraVlsu::fsm_handler),
event_label(*this, "label", 0, gv::Vcd_event_type_string)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);

    this->insns.resize(AraVlsu::queue_size);

    int nb_ports = iss.get_js_config()->get_child_int("ara/nb_ports");
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
        iss.new_master_port("vlsu_" + std::to_string(i), &this->ports[i], this);
    }

    int nb_outstanding_reqs = iss.get_js_config()->get_child_int("ara/nb_outstanding_reqs");
    this->req_queues.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->req_queues[i] = new vp::Queue(this, "port_" + std::to_string(i) + "_reqs");
    }

    this->requests.resize(nb_ports * nb_outstanding_reqs);
}

void AraVlsu::start()
{
}

void AraVlsu::reset(bool active)
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

        // Since the request queues are cleared with the reset, we need to put back requests
        // in each queue
        int nb_ports = this->ara.iss.get_js_config()->get_child_int("ara/nb_ports");
        int nb_outstanding_reqs = this->ara.iss.get_js_config()->get_child_int("ara/nb_outstanding_reqs");
        int req_id = 0;
        for (int i=0; i<nb_ports; i++)
        {
            for (int j=0; j<nb_outstanding_reqs; j++)
            {
                this->req_queues[i]->push_back(&this->requests[req_id++]);
            }
        }
        this->op_timestamp = -1;
    }
}

void AraVlsu::enqueue_insn(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->ara.iss.exec.get_insn(pending_insn->entry);
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx, id: %d)\n",
        insn->addr, pending_insn->id);
    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&insn->addr);

    // Just push the instruction and let the FSM handle it if needed.
    // A delay is added to take into account the time needed on RTL to start the instruction
    pending_insn->timestamp = this->ara.iss.clock.get_cycles() + 1;
    AraVlsuPendingInsn &slot = this->insns[this->insn_last];
    this->insn_last = (this->insn_last + 1) % AraVlsu::queue_size;
    slot.insn = pending_insn;
    slot.nb_pending_bursts = 0;
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn++;

    this->fsm_event.enable();
}

void AraVlsu::isa_init()
{
    // Attach handlers to instructions so that we can quickly handle load and stores differently
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload_strided"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load_strided;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore_strided"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store_strided;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload_indexed"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load_indexed;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore_indexed"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store_indexed;
    }
}

void AraVlsu::handle_insn_load_strided(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], true, _this->insns[_this->insn_first_waiting].insn->reg_2);
}

void AraVlsu::handle_insn_store_strided(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], true, _this->insns[_this->insn_first_waiting].insn->reg_3);
}

void AraVlsu::handle_insn_load_indexed(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], false, 0, insn->in_regs[1]);
}

void AraVlsu::handle_insn_store_indexed(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], false, 0, insn->in_regs[2]);
}

void AraVlsu::handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride, iss_reg_t stride, int reg_indexed)
{
    // A load or store instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = this->ara.iss.vector.sewb;
    unsigned int lmul = this->ara.iss.vector.lmul;
    this->pending_vreg = reg;
    this->pending_velem = velem_get(&this->ara.iss, reg, 0, sewb, lmul);
    this->vstart = this->ara.iss.csr.vstart.value;
    this->pending_addr = this->insns[this->insn_first_waiting].insn->reg;
    this->pending_is_write = is_write;
    int inst_elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    int elem_size = reg_indexed != -1 ? sewb : inst_elem_size;
    this->pending_size = (this->ara.iss.csr.vl.value - this->ara.iss.csr.vstart.value) * elem_size;
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
        this->op_timestamp += this->prev_is_write != is_write ? (is_write ? 7 : 3) : 2;
    }

    this->prev_is_write = is_write;
    this->started = false;
}

void AraVlsu::handle_insn_load(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0]);
}

void AraVlsu::handle_insn_store(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1]);
}

void AraVlsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVlsu *_this = (AraVlsu *)__this;

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

    // Check if the first waiting instruction can be started
    if (_this->nb_waiting_insn > 0)
    {
        AraVlsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;

        if (pending_insn->timestamp <=_this->ara.iss.clock.get_cycles() &&
            _this->pending_size == 0)
        {
            iss_insn_t *insn = _this->ara.iss.exec.get_insn(pending_insn->entry);
            ((void (*)(AraVlsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
        }
    }

    if (_this->pending_size && _this->op_timestamp <= _this->ara.iss.clock.get_cycles())
    {
        AraVlsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;

        if (_this->ara.insn_ready(pending_insn))
        {
            uint64_t iter_size = 0;
            iss_insn_t *insn = _this->ara.iss.exec.get_insn(pending_insn->entry);

            if (!_this->started)
            {
                _this->started = true;
                _this->event_label.dump(insn->desc->label);
                _this->event_pc.event((uint8_t *)&insn->addr);
            }

            // If a pending request is ready, try to send requests to available ports
            for (int i=0; i<_this->ports.size(); i++)
            {
                if (_this->pending_size == 0) break;

                // Only use this port if it has requests available otherwise it means too many
                // are pending
                if (!_this->req_queues[i]->empty())
                {
                    uint64_t size;

                    if (_this->strided ||  _this->reg_indexed != -1)
                    {
                        size = _this->elem_size;
                    }
                    else
                    {
                        size = std::min((iss_addr_t)_this->ara.lane_width, _this->pending_size);
                    }

                    _this->trace.msg(vp::Trace::LEVEL_TRACE,
                        "Sending request (id: %d, port: %d, addr: 0x%lx, size: 0x%lx, pending_size: 0x%lx, is_write: %d)\n",
                        pending_insn->id, i, _this->pending_addr, size, _this->pending_size, _this->pending_is_write);

                    _this->event_addr[i].event((uint8_t *)&_this->pending_addr);
                    _this->event_size[i].event((uint8_t *)&size);
                    _this->event_is_write[i].event((uint8_t *)&_this->pending_is_write);

                    /// Pop a request from this port queue to limit number of outstanding requests
                    vp::IoReq *req = (vp::IoReq *)_this->req_queues[i]->pop();

                    req->prepare();

                    iss_reg_t addr = _this->pending_addr;

                    if (_this->reg_indexed != -1)
                    {
                        uint64_t offset = velem_get_value(&_this->ara.iss, _this->reg_indexed, _this->pending_elem,
                            _this->inst_elem_size, _this->ara.iss.vector.lmul);
                        addr += offset;
                        _this->pending_elem++;
                    }

                    iter_size += size;
                    req->set_addr(addr);
                    req->set_is_write(_this->pending_is_write);
                    req->set_size(size);

                    req->set_data(_this->pending_velem);

                    vp::IoReqStatus err = _this->ports[i].req(req);

                    // For now only synchronous requests are supported, so for snitch cluster
                    // Asynchronous will be needed when having more complex interconnects behind.
                    // Then the requests should be handled properly to model the number of on-going
                    // transactions.
                    if (err != vp::IO_REQ_OK)
                    {
                        _this->trace.fatal("Unimplemented async response");
                    }

                    // Put it back now until asynchronous responses are supported
                    _this->req_queues[i]->push_back(req);

                    // Prepare the next burst
                    if (_this->reg_indexed == -1)
                    {
                        _this->pending_addr += _this->strided ? _this->stride : size;
                    }
                    _this->pending_size -= size;
                    _this->pending_velem += size;

                    // Switch to next instruction once all burst have been sent
                    if (_this->pending_size == 0)
                    {
                        _this->nb_waiting_insn--;
                        _this->insn_first_waiting = (_this->insn_first_waiting + 1) % AraVlsu::queue_size;
                    }
                }
            }

            _this->ara.insn_commit(pending_insn, iter_size);

            _this->ara.exec_insn_chunk(insn, pending_insn, _this->vstart / _this->elem_size,
                (_this->vstart + iter_size) / _this->elem_size, iter_size / _this->elem_size);

            _this->vstart += iter_size;

            // Check if the first enqueued instruction must be removed
            if (_this->nb_pending_insn.get() > 0)
            {
                AraVlsuPendingInsn &slot = _this->insns[_this->insn_first];
                PendingInsn *pending_insn = slot.insn;
                if (_this->pending_size == 0 && slot.nb_pending_bursts == 0 &&
                    pending_insn->timestamp <= _this->ara.iss.clock.get_cycles())
                {
                    _this->event_label.dump_highz_next();
                    // Remember the timestamp of last operation to impact timing of next one
                    _this->op_timestamp = _this->ara.iss.clock.get_cycles();
                    _this->insn_first = (_this->insn_first + 1) % AraVlsu::queue_size;
                    _this->nb_pending_insn.dec(1);
                    pending_insn->timestamp = _this->ara.iss.clock.get_cycles() + 1;
                    _this->ara.insn_end(pending_insn);
                }
            }
        }
    }
}
