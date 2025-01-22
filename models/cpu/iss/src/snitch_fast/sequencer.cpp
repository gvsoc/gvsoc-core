/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

 #include <cpu/iss/include/cores/snitch_fast/sequencer.hpp>
 #include "cpu/iss/include/iss.hpp"
 #include ISS_CORE_INC(class.hpp)


Sequencer::Sequencer(IssWrapper &top, Iss &iss)
: vp::Block(&top, "sequencer"), iss(iss), fsm_event(this, &Sequencer::fsm_handler)
{
    iss.top.traces.new_trace("sequencer", &trace, vp::DEBUG);

    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("all"))
        {
            insn->u.insn.stub_handler = &Sequencer::non_float_handler;
        }

        // Redirect all float instructions to sequencer buffer
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("fp_op"))
        {
            insn->u.insn.stub_handler = &Sequencer::sequence_buffer_handler;
        }

        // Except those who should bypass the sequencer
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("nseq"))
        {
            insn->u.insn.stub_handler = &Sequencer::direct_branch_handler;
        }
    }
}


void Sequencer::reset(bool active)
{
    if (active)
    {
        this->stall_reg = -1;
        this->input_insn = NULL;
        this->stalled_insn = false;
        this->max_rpt = 0;
    }
}


void Sequencer::build()
{
}


iss_reg_t Sequencer::non_float_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Checking non-float instructions register dependencies\n");

    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if (iss->regfile.scoreboard_reg_timestamp[insn->in_regs[i]] == -1)
        {
            iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to register dependency (reg: %d)\n", insn->in_regs[i]);
            iss->sequencer.stall_reg = insn->in_regs[i];
            iss->exec.insn_stall();
            return pc;
        }
    }

    return insn->stub_handler(iss, insn, pc);
}

iss_reg_t Sequencer::sequence_buffer_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    Sequencer *_this = &iss->sequencer;

    iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Received sequencable instruction (pc: 0x%lx)\n", pc);

    if (_this->input_insn)
    {
        // Input queue is full,  stall the core
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Input queue is full, stalling core\n");

        _this->stalled_insn = true;
        iss->exec.insn_stall();
        return pc;
    }
    else
    {
        if (_this->max_rpt == 0)
        {
            return insn->stub_handler(iss, insn, pc);
        }
        else
        {
            iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Pushing instruction to sequencer buffer (pc: 0x%lx)\n", pc);
            iss->sequencer.buffer.push_back(insn);
            return iss_insn_next(iss, insn, pc);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

iss_reg_t Sequencer::direct_branch_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // THis is called for each instruction which should bypass the sequence buffer.
    Sequencer *_this = &iss->sequencer;

    iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Received non-sequencable instruction (pc: 0x%lx)\n", pc);

    if (_this->input_insn)
    {
        // Input queue is full,  stall the core
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Input queue is full, stalling core\n");

        _this->stalled_insn = true;
        iss->exec.insn_stall();
        return pc;
    }
    else
    {
        // Input queue is empty, store the input instruction and continue with next one
        if (_this->buffer.size() > 0)
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Buffer non empty, stalling direct branch instruction\n");

            // Since this instruction is synchronizing with snitch core, we need to invalidate output
            // registers so that any core instruction using one of these registers is stalled
            for (int i=0; i<insn->nb_out_reg; i++)
            {
                int reg = insn->out_regs[i];
                if (!insn->out_regs_fp[i])
                {
                    iss->regfile.scoreboard_reg_invalidate(reg);
                }
            }

            _this->input_insn = insn;
            return iss_insn_next(iss, insn, pc);
        }
        else
        {
            return insn->stub_handler(iss, insn, pc);
        }
    }
}

iss_reg_t Sequencer::frep_handle(iss_insn_t *insn, iss_reg_t pc, bool is_inner)
{
    this->is_inner = is_inner;
    this->max_inst = insn->uim[2];
    this->max_rpt = this->iss.regfile.get_reg(insn->in_regs[0]);
    this->stagger_max = insn->uim[1];
    this->stagger_mask = insn->uim[0];
    this->insn_count = 0;
    this->rpt_count = 0;

    this->fsm_event.enable();

    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Frep config (is_inner: %d, max_inst: %d, max_rpt: %d, stagger_max: %d, stagger_mask: 0x%llx)\n",
        is_inner, this->max_inst, this->max_rpt, this->stagger_max, this->stagger_mask);

    return iss_insn_next(&this->iss, insn, pc);
}

void Sequencer::fsm_handler(vp::Block *block, vp::ClockEvent *event)
{
    Sequencer *_this = (Sequencer *)block;

    if (_this->buffer.size() > 0)
    {
        iss_insn_t *insn = _this->buffer[_this->insn_count];

        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Executing instruction from sequence buffer (pc: 0x%lx)\n", insn->addr);
        bool force_trace_dump = _this->iss.trace.force_trace_dump;
        _this->iss.trace.force_trace_dump = true;
        insn->stub_handler(&_this->iss, insn, insn->addr);
        _this->iss.trace.force_trace_dump = force_trace_dump;

        if (_this->is_inner)
        {
        }
        else
        {
            if (_this->insn_count == _this->max_inst)
            {
                if (_this->rpt_count == _this->max_rpt)
                {
                    _this->buffer.clear();
                    _this->max_rpt = 0;
                }
                else
                {
                    _this->rpt_count++;
                }
            }
            else
            {
                _this->insn_count++;
            }
        }
    }
    else if (_this->input_insn)
    {
        iss_insn_t *insn = _this->input_insn;
        _this->input_insn = NULL;
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Executing instruction from direct branch (pc: 0x%lx)\n", insn->addr);

        for (int i=0; i<insn->nb_out_reg; i++)
        {
            _this->iss.regfile.scoreboard_reg_set_timestamp(insn->out_regs[i], 0, 0);
        }

        bool force_trace_dump = _this->iss.trace.force_trace_dump;
        _this->iss.trace.force_trace_dump = true;
        insn->stub_handler(&_this->iss, insn, insn->addr);
        _this->iss.trace.force_trace_dump = force_trace_dump;

        if (_this->stalled_insn)
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Unstalling instruction\n");
            _this->iss.trace.dump_trace_enabled = true;
            _this->iss.exec.current_insn = _this->iss.exec.stall_insn;
            _this->iss.exec.insn_resume();
            _this->iss.exec.stalled_dec();
            _this->stalled_insn = true;
        }
        else if (_this->stall_reg != -1)
        {
            for (int i=0; i<insn->nb_out_reg; i++)
            {
                if (_this->stall_reg == insn->out_regs[i])
                {
                    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Unstalling instruction (reg: %d)\n", _this->stall_reg);
                    _this->stall_reg = -1;
                    _this->iss.trace.dump_trace_enabled = true;
                    _this->iss.exec.current_insn = _this->iss.exec.stall_insn;
                    _this->iss.exec.insn_resume();
                    _this->iss.exec.stalled_dec();
                    break;
                }
            }
        }
    }
    else if (_this->max_rpt == 0)
    {
        _this->fsm_event.disable();
    }
}
