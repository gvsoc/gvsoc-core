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
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event_string("label", &this->event_label);
}

void AraVcompute::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->pending_insn = NULL;
        this->event_active.event(&zero);
    }
}

void AraVcompute::enqueue_insn(PendingInsn *pending_insn)
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

void AraVcompute::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVcompute *_this = (AraVcompute *)__this;

    // Check if the pending instruction has finished. Its timestamp was set when instruction
    // started annd indicates when it must be terminated
    if (_this->pending_insn)
    {
        if (_this->pending_insn->chained)
        {
            // If the instruction is chained, we can finish it once each vector chaining is done
            // which can be detected once the number of committed elements become 0
            bool done = true;
            iss_insn_t *insn = _this->pending_insn->insn;
            for (int i=0; i<insn->nb_in_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    if (_this->ara.scoreboard_committed[insn->in_regs[i]] != 0)
                    {
                        done = false;
                        break;
                    }
                }
            }

            // If done, unchain it and assign end timestamp, this make it finish at the indidcated timestamp
            if (done)
            {
                _this->pending_insn->chained = false;
                // the instruction ends the cycle after chaining stopped unless it is earlier than
                // that start plus the stimation duration.
                _this->pending_insn->timestamp = std::max(_this->end_cyclestamp, _this->ara.iss.top.clock.get_cycles() + 1);
            }
        }
        else
        {
            if (_this->pending_insn->timestamp <= _this->ara.iss.top.clock.get_cycles())
            {
                // Notify ara so that it removes it from the pending instruction and update scoreboard
                _this->ara.insn_end(_this->pending_insn);
                // Clear it to take a new one
                _this->pending_insn = NULL;
                _this->event_pc.event_highz();
                _this->event_label.event_string((char *)1, false);
            }
        }
    }

    // Check if a new instruction can starts.
    // Take the first one from the queue and see if its timestamp has passed. This was set during
    // enqueue to make sure the instruction starts at best the cycle after.
    if (_this->pending_insn == NULL && _this->insns.size() != 0 &&
        _this->insns.front()->timestamp <= _this->ara.iss.top.clock.get_cycles())
    {
        _this->pending_insn = _this->insns.front();
       	_this->insns.pop();

        // Computes the duration of the instruction. The compute unit will process in each cycle
        // a number of elements equal to the numbe of lanes multiplied by the number of elements each FPU can
        // handle.
        // Note that the number of elements already take into account lmul
        unsigned int nb_elems = (_this->ara.iss.csr.vl.value - _this->ara.iss.csr.vstart.value) /
            _this->ara.nb_lanes;
        int64_t end_cyclestamp = _this->ara.iss.top.clock.get_cycles() +
            nb_elems * _this->ara.iss.vector.sewb / 8 + _this->pending_insn->insn->latency;

        // Only assign the end timestamp if the instruction is not chained
        if (!_this->pending_insn->chained)
        {
            _this->pending_insn->timestamp = end_cyclestamp;
        }
        else
        {
            // Otherwise just remember it to make sure it lasts at east the operation duration
            _this->end_cyclestamp = end_cyclestamp;
        }

        _this->event_pc.event((uint8_t *)&_this->pending_insn->pc);
        _this->event_label.event_string(_this->pending_insn->insn->desc->label, false);
    }

    // In case nothing is on-going, disable the FSM
    if (_this->insns.size() == 0 && _this->pending_insn == NULL)
    {
        uint8_t zero = 0;
        _this->event_active.event(&zero);
        _this->fsm_event.disable();
    }
}
