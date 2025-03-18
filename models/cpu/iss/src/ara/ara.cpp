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
#include <cstdint>


Ara::Ara(IssWrapper &top, Iss &iss)
    : Block(&top, "ara"), iss(iss),
    fsm_event(this, &Ara::fsm_handler),
    nb_pending_insn(*this, "nb_pending_insn", 8, true),
    queue_full(*this, "queue_full", 1, true)
{
}

void Ara::reset(bool active)
{
    if (active)
    {
        this->stall_reg = -1;
    }
}

void Ara::build()
{
    this->queue_size = 8;
}

void Ara::insn_enqueue(iss_insn_t *insn, iss_reg_t pc)
{
    this->nb_pending_insn.inc(1);
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }

    this->pending_insns.push(insn);
    this->pending_insns_timestamp.push(this->iss.top.clock.get_cycles() + 20);
    this->pending_insns_pc.push(pc);
    int reg = insn->in_regs[0];
    uint64_t reg_value;
    if (insn->in_regs_fp[0])
    {
        reg_value = this->iss.regfile.get_freg_untimed(reg);
    }
    else
    {
        reg_value = this->iss.regfile.get_reg_untimed(reg);
    }
    this->base_addr_queue.push(reg_value);

    this->fsm_event.enable();
}

void Ara::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Ara *_this = (Ara *)__this;

    if (_this->pending_insns.size() > 0 &&
        _this->pending_insns_timestamp.front() <= _this->iss.top.clock.get_cycles())
    {
        iss_insn_t *insn = _this->pending_insns.front();
        iss_addr_t pc = _this->pending_insns_pc.front();
        _this->pending_insns.pop();
        _this->pending_insns_timestamp.pop();
        _this->pending_insns_pc.pop();

        _this->nb_pending_insn.dec(1);
        _this->queue_full.set(false);

        insn->stub_handler(&_this->iss, insn, pc);

        for (int i=0; i<insn->nb_out_reg; i++)
        {
            _this->iss.regfile.scoreboard_reg_set_timestamp(insn->out_regs[i], 0, 0);
        }

        if (_this->stall_reg != -1)
        {
            _this->stall_reg = -1;
            _this->iss.trace.dump_trace_enabled = true;
            _this->iss.exec.current_insn = _this->iss.exec.stall_insn;
            _this->iss.exec.insn_resume();
            _this->iss.exec.stalled_dec();
        }

        _this->base_addr_queue.pop();
    }

    if (_this->pending_insns.size() == 0)
    {
        _this->fsm_event.disable();
    }
}
