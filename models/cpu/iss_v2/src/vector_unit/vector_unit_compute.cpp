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

#include "cpu/iss_v2/include/iss.hpp"
#include "cpu/iss_v2/include/cores/vector_unit/vector_unit.hpp"

VuCompute::VuCompute(Vu &vu, std::string name)
: VuBlock(&vu, name), vu(vu),
fsm_event(this, &VuCompute::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event_string("label", &this->event_label);
}

void VuCompute::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->pending_insn = NULL;
        this->event_active.event(&zero);
    }
}

void VuCompute::enqueue_insn(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->vu.iss.exec.get_insn(pending_insn->entry);
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx, id: %d)\n",
        insn->addr, pending_insn->id);
    uint8_t one = 1;
    this->event_active.event(&one);

    // Just push the instruction and let the FSM handle it if needed.
    // It is marked for execution in the next cycle so that the FSM does not handle it in this
    // cycle in case the FSM is already active
    pending_insn->timestamp++;
    this->insns.push(pending_insn);
    this->fsm_event.enable();
}

void VuCompute::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    VuCompute *_this = (VuCompute *)__this;

    // Check if the pending instruction has finished. Its timestamp was set when instruction
    // started annd indicates when it must be terminated
    if (_this->pending_insn)
    {
        if (_this->pending_insn->timestamp <= _this->vu.iss.clock.get_cycles())
        {
            // Notify vu so that it removes it from the pending instruction and update scoreboard
            _this->vu.insn_end(_this->pending_insn);
            // Clear it to take a new one
            _this->pending_insn = NULL;
            _this->event_pc.event_highz();
            _this->event_label.event_string((char *)1, false);
        }
    }

    // Check if a new instruction can starts.
    // Take the first one from the queue and see if its timestamp has passed. This was set during
    // enqueue to make sure the instruction starts at best the cycle after.
    if (_this->pending_insn == NULL && _this->insns.size() != 0 &&
        _this->insns.front()->timestamp <= _this->vu.iss.clock.get_cycles())
    {
        PendingInsn *pending_insn = _this->insns.front();
        iss_insn_t *insn = _this->vu.iss.exec.get_insn(pending_insn->entry);
        bool ready= _this->vu.insn_ready(pending_insn);

        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Check ready (pc: 0x%lx, id: %d, ready: %d)\n",
            insn->addr, pending_insn->id, ready);

        if (ready)
        {
            int nb_elem_per_cycle = _this->vu.nb_lanes * _this->vu.lane_width /
                _this->vu.iss.vector.sewb;

            if (pending_insn->nb_bytes_done == 0)
            {
                _this->event_pc.event((uint8_t *)&insn->addr);
                _this->event_label.event_string(insn->desc->label, false);

                unsigned int nb_elems = _this->vu.iss.csr.vl.value - _this->vu.iss.csr.vstart.value;
                _this->total_size = nb_elems * _this->vu.iss.vector.sewb;
                _this->vstart = _this->vu.iss.csr.vstart.value;
                _this->vend = std::min((int)_this->vu.iss.csr.vl.value,
                    _this->vstart + nb_elem_per_cycle);
            }

            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Exec chunk (pc: 0x%lx, id: %d, start: %d, end: %d)\n",
                insn->addr, pending_insn->id, _this->vstart, _this->vend);

            // Now that the instruction is over, execute the handler to functionally model it. This will
            // write the output register.
            // Store the instruction register as it will be used by the handler.
            _this->vu.current_insn_reg = pending_insn->reg;
            _this->vu.current_insn_reg_2 = pending_insn->reg_2;

            _this->vu.exec_insn_chunk(insn, pending_insn, _this->vstart, _this->vend, nb_elem_per_cycle);

            _this->vstart += nb_elem_per_cycle;
            _this->vend = std::min((int)_this->vu.iss.csr.vl.value,
                _this->vstart + nb_elem_per_cycle);

            _this->vu.insn_commit(pending_insn, nb_elem_per_cycle * _this->vu.iss.vector.sewb);

            if (pending_insn->nb_bytes_done >= _this->total_size)
            {
                _this->pending_insn = pending_insn;
               	_this->insns.pop();
                _this->pending_insn->timestamp = _this->vu.iss.clock.get_cycles() + insn->latency + 1;
            }

        }
    }

    // In case nothing is on-going, disable the FSM
    if (_this->insns.size() == 0 && _this->pending_insn == NULL)
    {
        uint8_t zero = 0;
        _this->event_active.event(&zero);
        _this->fsm_event.disable();
    }
}
