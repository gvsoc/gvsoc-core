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

AraValu::AraValu(Ara &ara)
: AraBlock(&ara, "valu"), ara(ara),
fsm_event(this, &AraValu::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active_box", &this->trace_active_box, 1);
    this->traces.new_trace_event("active", &this->trace_active, 1);
    this->traces.new_trace_event("pc", &this->trace_pc, 64);
}

void AraValu::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->pending_insn = NULL;
        this->trace_active.event(&zero);
    }
}

void AraValu::enqueue_insn(PendingInsn *pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pending_insn->pc);
    uint8_t one = 1;
    this->trace_active_box.event(&one);
    this->trace_active.event(&one);
    this->insns.push(pending_insn);
    this->fsm_event.enable();
}

void AraValu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraValu *_this = (AraValu *)__this;

    if (_this->pending_insn == NULL && _this->insns.size() != 0)
    {
        _this->pending_insn = _this->insns.front();
       	_this->insns.pop();
        unsigned int lmul = _this->ara.iss.vector.LMUL_t;
        unsigned int nb_elems = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) /
            _this->ara.nb_lanes;
        _this->pending_insn->timestamp = _this->ara.iss.top.clock.get_cycles() + nb_elems + 1;
        _this->trace_pc.event((uint8_t *)&_this->pending_insn->pc);
    }

    if (_this->pending_insn &&
        _this->pending_insn->timestamp <= _this->ara.iss.top.clock.get_cycles())
    {
        _this->ara.insn_end(_this->pending_insn);
        _this->pending_insn = NULL;
        _this->trace_pc.event_highz();
    }

    if (_this->insns.size() == 0)
    {
        _this->trace_active_box.event_highz();
        uint8_t zero = 0;
        _this->trace_active.event(&zero);
    }
}
