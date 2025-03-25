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

AraVcompute::AraVcompute(Ara &ara, std::string name)
: AraBlock(&ara, name), ara(ara),
fsm_event(this, &AraVcompute::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->trace_active, 1);
    this->traces.new_trace_event("pc", &this->trace_pc, 64);
    this->traces.new_trace_event_string("label", &this->trace_label);
}

void AraVcompute::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->pending_insn = NULL;
        this->trace_active.event(&zero);
    }
}

void AraVcompute::enqueue_insn(PendingInsn *pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pending_insn->pc);
    uint8_t one = 1;
    pending_insn->timestamp = this->ara.iss.top.clock.get_cycles() + 1;
    this->trace_active.event(&one);
    this->insns.push(pending_insn);
    this->fsm_event.enable();
}

void AraVcompute::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVcompute *_this = (AraVcompute *)__this;

    if (_this->pending_insn &&
        _this->pending_insn->timestamp <= _this->ara.iss.top.clock.get_cycles())
    {
        _this->ara.insn_end(_this->pending_insn);
        _this->pending_insn = NULL;
        _this->trace_pc.event_highz();
        _this->trace_label.event_string((char *)1, false);
    }

    if (_this->pending_insn == NULL && _this->insns.size() != 0 &&
        _this->insns.front()->timestamp <= _this->ara.iss.top.clock.get_cycles())
    {
        _this->pending_insn = _this->insns.front();
       	_this->insns.pop();
        unsigned int lmul = _this->ara.iss.vector.LMUL_t;
        unsigned int nb_elems = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) /
            _this->ara.nb_lanes;
        _this->pending_insn->timestamp = _this->ara.iss.top.clock.get_cycles() + nb_elems;
        _this->trace_pc.event((uint8_t *)&_this->pending_insn->pc);
        _this->trace_label.event_string(_this->pending_insn->insn->desc->label, true);
    }

    if (_this->insns.size() == 0 && _this->pending_insn == NULL)
    {
        uint8_t zero = 0;
        _this->trace_active.event(&zero);
        _this->fsm_event.disable();
    }
}
