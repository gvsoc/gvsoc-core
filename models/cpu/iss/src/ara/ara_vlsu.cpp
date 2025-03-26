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

#include "cpu/iss/include/iss.hpp"
#include "cpu/iss/include/cores/ara/ara.hpp"

AraVlsu::AraVlsu(Ara &ara, IssWrapper &top)
: AraBlock(&ara, "vlsu"), ara(ara),
fsm_event(this, &AraVlsu::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("addr", &this->event_addr, 64);
    this->traces.new_trace_event("size", &this->event_size, 64);
    this->traces.new_trace_event("is_write", &this->event_is_write, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event_string("label", &this->event_label);

    for (int i=0; i<AraVlsu::nb_burst; i++)
    {
        this->free_bursts.push(new vp::IoReq());
    }

    this->io_itf.set_resp_meth(&AraVlsu::data_response);
    this->io_itf.set_grant_meth(&AraVlsu::data_grant);
    top.new_master_port("vlsu", &this->io_itf, (vp::Block *)this);
}

void AraVlsu::reset(bool active)
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
        this->nb_pending_bursts = 0;
        this->pending_insn = NULL;
    }
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
}

void AraVlsu::enqueue_insn(PendingInsn *pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pending_insn->pc);
    uint8_t one = 1;
    this->event_active.event(&one);

    // Just push the instruction and let the FSM handle it if needed.
    // It is marked for execution in the next cycle so that the FSM does not handle it in this
    // cycle in case the FSM is already active
    pending_insn->timestamp = this->ara.iss.top.clock.get_cycles() + 1;
    this->insns.push(pending_insn);
    this->fsm_event.enable();
}


void AraVlsu::handle_insn_load(AraVlsu *_this, iss_insn_t *insn)
{
    // A load instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = _this->ara.iss.vector.sewb;
    unsigned int lmul = _this->ara.iss.vector.LMUL_t;
    _this->pending_velem = velem_get(&_this->ara.iss, insn->out_regs[0], 0, sewb, lmul);
    _this->pending_addr = _this->insns.front()->reg;
    _this->pending_is_write = false;
    _this->pending_elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    int elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    _this->pending_size = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) * elem_size;
}

void AraVlsu::handle_insn_store(AraVlsu *_this, iss_insn_t *insn)
{
    // A store instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    unsigned int sewb = _this->ara.iss.vector.sewb;
    unsigned int lmul = _this->ara.iss.vector.LMUL_t;
    _this->pending_velem = velem_get(&_this->ara.iss, insn->in_regs[1], 0, sewb, lmul);
    _this->pending_addr = _this->insns.front()->reg;
    _this->pending_is_write = true;
    int elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    _this->pending_size = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) * elem_size;
}

void AraVlsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
    AraVlsu *_this = (AraVlsu *)__this;
    // The last burst which stalled the block has just been granted. Resume the bursts.
    _this->stalled = false;
}

void AraVlsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    AraVlsu *_this = (AraVlsu *)__this;
    // We just received the response to one of the burst. Account it to allow terminating the
    // instruction.
    _this->nb_pending_bursts--;
    _this->free_bursts.push(req);

    // Check if current instruction can now be terminated
    _this->check_current_insn();
}

void AraVlsu::check_current_insn()
{
    // Current instruction can terminate only once there is no more burst to send and there is no
    // pending burst
    if (this->pending_size == 0 && this->nb_pending_bursts == 0)
    {
        this->insns.pop();
        this->ara.insn_end(this->pending_insn);
        this->pending_insn = NULL;
    }
}

void AraVlsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVlsu *_this = (AraVlsu *)__this;

    // In case nothing is on-going, disable the FSM
    if (_this->insns.size() == 0 && _this->pending_size == 0)
    {
        uint8_t zero = 0;
        _this->event_pc.event(&zero);
        _this->event_active.event(&zero);
        _this->event_addr.event(&zero);
        _this->event_size.event(&zero);
        _this->event_is_write.event(&zero);
        _this->event_label.event_string((char *)1, false);

        _this->fsm_event.disable();
    }

    if (_this->insns.size() > 0)
    {
        // If nothing is on-going, check if we can start a new instruction
        if (_this->insns.front()->timestamp <=_this->ara.iss.top.clock.get_cycles() &&
            _this->pending_insn == NULL)
        {
            _this->pending_insn = _this->insns.front();
            _this->event_label.event_string(_this->pending_insn->insn->desc->label, true);
            _this->event_pc.event((uint8_t *)&_this->pending_insn->pc);

            iss_insn_t *insn = _this->pending_insn->insn;
            ((void (*)(AraVlsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
        }

        if (_this->pending_size && _this->free_bursts.size() != 0 && !_this->stalled)
        {
            uint64_t size = std::min((iss_addr_t)_this->ara.nb_lanes * 8, _this->pending_size);
            _this->trace.msg(vp::Trace::LEVEL_TRACE,
                "Sending request (addr: 0x%lx, size: 0x%lx, is_write: %d)\n",
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

            int err = _this->ara.iss.lsu.data.req(req);
            if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
            {
                _this->free_bursts.push(req);
            }
            else if (err == vp::IO_REQ_DENIED)
            {
                // Any denied burst must prevent this block from sending any other burst.
                // It will resume only once the burst is granted
                _this->stalled = true;
            }
            else
            {
                // In case the burst is asynchronous, account it so that we terminate the
                // instruction only once all pending bursts are done.
                _this->nb_pending_bursts++;
            }

            // Prepare the next burst
            _this->pending_addr += size;
            _this->pending_size -= size;
            _this->pending_velem += size;

            // And check if we are done with this instruction
            _this->check_current_insn();
        }

    }
}
