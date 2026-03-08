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
: VuBlock(&iss, "vlsu"), vu(vu),
nb_pending_insn(*this, "nb_pending_insn", 8, true),
fsm_event(this, &VuLsu::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("addr", &this->event_addr, 64);
    this->traces.new_trace_event("size", &this->event_size, 64);
    this->traces.new_trace_event("is_write", &this->event_is_write, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);
    this->traces.new_trace_event_string("label", &this->event_label);

    this->insns.resize(VuLsu::queue_size);

    for (int i=0; i<VuLsu::nb_burst; i++)
    {
        this->free_bursts.push(new vp::IoReq());
    }

    this->io_itf.set_resp_meth(&VuLsu::data_response);
    this->io_itf.set_grant_meth(&VuLsu::data_grant);
    iss.new_master_port("vlsu", &this->io_itf, (vp::Block *)this);
}

void VuLsu::reset(bool active)
{
    if (active)
    {
        this->pending_size = 0;
        uint8_t zero = 0;
        this->event_active.event(&zero);
        this->event_addr.event(&zero);
        this->event_size.event(&zero);
        this->event_is_write.event(&zero);
        this->stalled = false;
        this->insn_first = 0;
        this->insn_first_waiting = 0;
        this->insn_last = 0;
        this->nb_waiting_insn = 0;
    }
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
}

void VuLsu::enqueue_insn(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->vu.iss.exec.get_insn(pending_insn->entry);
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx, id: %d)\n",
        insn->addr, pending_insn->id);
    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&insn->addr);

    // Just push the instruction and let the FSM handle it if needed.
    // A delay is added to take into account the time needed on RTL to start the instruction
    pending_insn->timestamp = this->vu.iss.clock.get_cycles() + 5;
    VuLsuPendingInsn &slot = this->insns[this->insn_last];
    this->insn_last = (this->insn_last + 1) % VuLsu::queue_size;
    slot.insn = pending_insn;
    slot.nb_pending_bursts = 0;
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn++;

    this->fsm_event.enable();
}


void VuLsu::handle_insn_load(VuLsu *_this, iss_insn_t *insn)
{
    // A load instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = _this->vu.iss.vector.sewb;
    unsigned int lmul = _this->vu.iss.vector.lmul;
    _this->pending_velem = velem_get(&_this->vu.iss, insn->out_regs[0], 0, sewb, lmul);
    _this->pending_addr = _this->insns[_this->insn_first_waiting].insn->reg;
    _this->pending_is_write = false;
    int elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    _this->elem_size = elem_size;
    _this->pending_size = (_this->vu.iss.csr.vl.value - _this->vu.iss.csr.vstart.value) * elem_size;
    _this->vstart = _this->vu.iss.csr.vstart.value;
}

void VuLsu::handle_insn_store(VuLsu *_this, iss_insn_t *insn)
{
    // A store instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = _this->vu.iss.vector.sewb;
    unsigned int lmul = _this->vu.iss.vector.lmul;
    _this->pending_velem = velem_get(&_this->vu.iss, insn->in_regs[1], 0, sewb, lmul);
    _this->pending_addr = _this->insns[_this->insn_first_waiting].insn->reg;
    _this->pending_is_write = true;
    int elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    _this->elem_size = elem_size;
    _this->pending_size = (_this->vu.iss.csr.vl.value - _this->vu.iss.csr.vstart.value) * elem_size;
    _this->vstart = _this->vu.iss.csr.vstart.value;
}

void VuLsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
    VuLsu *_this = (VuLsu *)__this;
    // The last burst which stalled the block has just been granted. Resume the bursts.
    _this->stalled = false;
}

void VuLsu::handle_burst_end(vp::IoReq *req)
{
    // The slot is stored in the req stack
    VuLsuPendingInsn *slot = (VuLsuPendingInsn *)req->arg_pop();
    slot->nb_pending_bursts--;
    this->free_bursts.push(req);
    if (slot->nb_pending_bursts == 0)
    {
        // Trigger the instruction end with some delay to reproduce RTL behavior
        slot->insn->timestamp = this->vu.iss.clock.get_cycles() + 4;
    }
}

void VuLsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    VuLsu *_this = (VuLsu *)__this;
    // We just received the response to one of the burst. Account it to allow terminating the
    // instruction.
    _this->handle_burst_end(req);
}

void VuLsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    VuLsu *_this = (VuLsu *)__this;

    // Check if any synchronous delayed burst need to be terminated
    if (_this->delayed_bursts.size() != 0 &&
        _this->delayed_bursts_timestamps.front() <= _this->vu.iss.clock.get_cycles())
    {
        vp::IoReq *req = _this->delayed_bursts.front();
        _this->handle_burst_end(req);
        _this->delayed_bursts.pop();
        _this->delayed_bursts_timestamps.pop();
    }

    // In case nothing is on-going, disable the FSM
    if (_this->nb_pending_insn.get() == 0)
    {
        uint8_t zero = 0;
        _this->event_queue.event_highz();
        _this->event_pc.event_highz();
        _this->event_active.event(&zero);
        _this->event_addr.event_highz();
        _this->event_size.event_highz();
        _this->event_is_write.event(&zero);
        _this->event_label.event_string((char *)1, false);

        _this->fsm_event.disable();
    }

    // Check if the first waiting instruction can be started
    if (_this->nb_waiting_insn > 0)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;

        if (pending_insn->timestamp <=_this->vu.iss.clock.get_cycles() &&
            _this->pending_size == 0)
        {
            iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);
            _this->event_label.event_string(insn->desc->label, false);
            _this->event_pc.event((uint8_t *)&insn->addr);

            ((void (*)(VuLsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
        }
    }

    // Check if a new burst must be sent
    if (_this->pending_size && _this->free_bursts.size() != 0 && !_this->stalled)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;
        iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);

        if (_this->vu.insn_ready(pending_insn))
        {
            uint64_t size = std::min((iss_addr_t)_this->vu.nb_lanes / 2 * 8, _this->pending_size);
            _this->trace.msg(vp::Trace::LEVEL_TRACE,
                "Sending request (addr: 0x%lx, pending_size: 0x%lx, is_write: %d)\n",
                _this->pending_addr, _this->pending_size, _this->pending_is_write);

            _this->event_addr.event((uint8_t *)&_this->pending_addr);
            _this->event_size.event((uint8_t *)&_this->pending_size);
            _this->event_is_write.event((uint8_t *)&_this->pending_is_write);

            vp::IoReq *req = _this->free_bursts.front();
            _this->free_bursts.pop();
            req->init();
            req->set_addr(_this->pending_addr);
            req->set_size(size);
            req->set_is_write(_this->pending_is_write);
            req->set_data(_this->pending_velem);
            req->arg_push((void *)&slot);
            slot.nb_pending_bursts++;

            int err = _this->vu.iss.lsu.data.req(req);
            if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
            {
                int64_t latency = req->get_full_latency();
                if (latency == 0)
                {
                    _this->handle_burst_end(req);
                }
                else
                {
                    _this->delayed_bursts.push(req);
                    _this->delayed_bursts_timestamps.push(_this->vu.iss.clock.get_cycles() + latency);
                }
            }
            else if (err == vp::IO_REQ_DENIED)
            {
                // Any denied burst must prevent this block from sending any other burst.
                // It will resume only once the burst is granted
                _this->stalled = true;
            }
            else
            {
                // Nothing to for asynchronous requests, they are handled in the callback
            }

            // Prepare the next burst
            _this->pending_addr += size;
            _this->pending_size -= size;
            _this->pending_velem += size;

            // Switch to next instruction once all burst have been sent
            if (_this->pending_size == 0)
            {
                _this->nb_waiting_insn--;
                _this->insn_first_waiting = (_this->insn_first_waiting + 1) % VuLsu::queue_size;
            }

            _this->vu.insn_commit(pending_insn, size);

            _this->vu.exec_insn_chunk(insn, pending_insn, _this->vstart / _this->elem_size,
                (_this->vstart + size) / _this->elem_size, size / _this->elem_size);

            _this->vstart += size;
        }
    }

    // Check if the first enqueued instruction must be removed
    if (_this->nb_pending_insn.get() > 0)
    {
        VuLsuPendingInsn &slot = _this->insns[_this->insn_first];
        PendingInsn *pending_insn = slot.insn;
        if (slot.nb_pending_bursts == 0 && _this->pending_size == 0 &&
            pending_insn->timestamp <= _this->vu.iss.clock.get_cycles())
        {
            _this->insn_first = (_this->insn_first + 1) % VuLsu::queue_size;
            _this->nb_pending_insn.dec(1);
            _this->vu.insn_end(pending_insn);
        }
    }
}
