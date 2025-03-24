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

AraVlsu::AraVlsu(Ara &ara, int nb_lanes)
: AraBlock(&ara, "vlsu"), ara(ara), nb_lanes(nb_lanes),
fsm_event(this, &AraVlsu::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active_box", &this->trace_active_box, 1);
    this->traces.new_trace_event("active", &this->trace_active, 1);
    this->traces.new_trace_event("addr", &this->trace_addr, 64);
    this->traces.new_trace_event("size", &this->trace_size, 64);
    this->traces.new_trace_event("is_write", &this->trace_is_write, 1);
    this->traces.new_trace_event("pc", &this->trace_pc, 64);
}

void AraVlsu::reset(bool active)
{
    if (active)
    {
        this->pending_size = 0;
        uint8_t zero = 0;
        this->trace_active.event(&zero);
        this->trace_addr.event(&zero);
        this->trace_size.event(&zero);
        this->trace_is_write.event(&zero);
    }
}

void AraVlsu::enqueue_insn(PendingInsn *pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pending_insn->pc);
    uint8_t one = 1;
    this->trace_active_box.event(&one);
    this->trace_active.event(&one);
    this->insns.push(pending_insn);
    pending_insn->timestamp = this->ara.iss.top.clock.get_cycles() + 1;
    this->fsm_event.enable();
}


void AraVlsu::handle_insn_load(AraVlsu *_this, iss_insn_t *insn)
{
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
    unsigned int sewb = _this->ara.iss.vector.sewb;
    unsigned int lmul = _this->ara.iss.vector.LMUL_t;
    _this->pending_velem = velem_get(&_this->ara.iss, insn->in_regs[1], 0, sewb, lmul);
    _this->pending_addr = _this->insns.front()->reg;
    _this->pending_is_write = true;
    int elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    _this->pending_size = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) * elem_size;
}

void AraVlsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVlsu *_this = (AraVlsu *)__this;

    if (_this->insns.size() == 0 && _this->pending_size == 0)
    {
        _this->trace_active_box.event_highz();
        uint8_t zero = 0;
        _this->trace_pc.event(&zero);
        _this->trace_active.event(&zero);
        _this->trace_addr.event(&zero);
        _this->trace_size.event(&zero);
        _this->trace_is_write.event(&zero);

        _this->fsm_event.disable();
    }

    if (_this->insns.size() > 0)
    {
        if (_this->insns.front()->timestamp <=_this->ara.iss.top.clock.get_cycles() &&
            _this->pending_size == 0)
        {
            PendingInsn *pending_insn = _this->insns.front();
            _this->trace_pc.event((uint8_t *)&pending_insn->pc);
            iss_insn_t *insn = pending_insn->insn;
            ((void (*)(AraVlsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
        }

        if (_this->pending_size)
        {
            uint64_t size = std::min((iss_addr_t)_this->nb_lanes * 8, _this->pending_size);
            _this->trace.msg(vp::Trace::LEVEL_TRACE,
                "Sending request (addr: 0x%lx, size: 0x%lx, is_write: %d)\n",
                _this->pending_addr, _this->pending_size, _this->pending_is_write);

            _this->trace_addr.event((uint8_t *)&_this->pending_addr);
            _this->trace_size.event((uint8_t *)&_this->pending_size);
            _this->trace_is_write.event((uint8_t *)&_this->pending_is_write);

            vp::IoReq req;
            req.init();
            req.set_addr(_this->pending_addr);
            req.set_size(size);
            req.set_is_write(_this->pending_is_write);
            req.set_data(_this->pending_velem);
            int err = _this->ara.iss.lsu.data.req(&req);

            _this->pending_addr += size;
            _this->pending_size -= size;
            _this->pending_velem += size;

            if (_this->pending_size == 0)
            {
                PendingInsn *pending_insn = _this->insns.front();
                _this->insns.pop();
                _this->ara.insn_end(pending_insn);
            }
        }

    }
}
