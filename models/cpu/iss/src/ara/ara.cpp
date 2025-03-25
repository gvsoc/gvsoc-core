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
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->trace_active, 1);
    this->traces.new_trace_event_string("label", &this->trace_label);

    this->nb_lanes = top.get_js_config()->get_int("ara/nb_lanes");
    this->iss.vector.VLEN = CONFIG_ISS_VLEN;
    this->blocks.resize(Ara::nb_blocks);
    this->blocks[Ara::vlsu_id] = new AraVlsu(*this, this->nb_lanes);
    this->blocks[Ara::vfpu_id] = new AraVcompute(*this, "vfpu");
    this->blocks[Ara::vslide_id] = new AraVcompute(*this, "vslide");
}

void Ara::reset(bool active)
{
    if (active)
    {
        for (int i=0; i<ISS_NB_VREGS; i++)
        {
            this->scoreboard_valid_ts[i] = 0;
            this->scoreboard_in_use[i] = 0;
        }
    }
    this->insn_first = 0;
    this->insn_first_enqueued = 0;
    this->insn_last = 0;
    this->nb_waiting_insn = 0;
}

void Ara::build()
{
    this->queue_size = 8;
    this->pending_insns.resize(this->queue_size);
}

PendingInsn *Ara::pending_insn_alloc(PendingInsn *cva6_pending_insn)
{
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn++;
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }
    int insn_id = this->insn_last;
    this->insn_last++;
    if (this->insn_last == this->queue_size)
    {
        this->insn_last = 0;
    }

    this->pending_insns[insn_id] = *cva6_pending_insn;

    return &this->pending_insns[insn_id];
}

void Ara::insn_enqueue(PendingInsn *cva6_pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", cva6_pending_insn->pc);
    PendingInsn *pending_insn = this->pending_insn_alloc(cva6_pending_insn);

    this->iss.top.insn_commit(cva6_pending_insn);

	iss_insn_t *insn = pending_insn->insn;
    uint8_t one = 1;
    this->trace_active.event(&one);
    this->trace_label.event_string(cva6_pending_insn->insn->desc->label, true);

    pending_insn->timestamp = this->iss.top.clock.get_cycles() + 1;
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
    pending_insn->reg = reg_value;

    this->fsm_event.enable();
}

void Ara::insn_end(PendingInsn *pending_insn)
{
    iss_insn_t *insn = pending_insn->insn;

    pending_insn->done = true;
    this->pending_insn = pending_insn;
    insn->stub_handler(&this->iss, insn, pending_insn->pc);

    // Mark output registers as ready in next cycle
    for (int i=0; i<insn->nb_out_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
        {
            this->scoreboard_valid_ts[insn->out_regs[i]] = this->iss.top.clock.get_cycles() + 1;
        }
        else
        {
           	this->iss.regfile.scoreboard_reg_set_timestamp(insn->out_regs[i], 0, 0);
        }
    }

    // Mark intput registers as not in use anymore
    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
        {
            this->scoreboard_in_use[insn->in_regs[i]]--;
        }
    }
}

void Ara::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Ara *_this = (Ara *)__this;

    if (_this->nb_pending_insn.get() > 0)
    {
        PendingInsn *pending_insn = &_this->pending_insns[_this->insn_first_enqueued];
        if (pending_insn->done)
        {
            _this->nb_pending_insn.dec(1);
            if (_this->nb_pending_insn.get() == 0)
            {
                uint8_t zero = 0;
                _this->trace_active.event(&zero);
                _this->trace_label.event_string((char *)1, false);
            }
            _this->queue_full.set(false);

            _this->insn_first_enqueued++;
            if (_this->insn_first_enqueued == _this->queue_size)
            {
                _this->insn_first_enqueued = 0;
            }
        }
    }

    if (_this->nb_waiting_insn > 0)
    {
        PendingInsn *pending_insn = &_this->pending_insns[_this->insn_first];

        if (pending_insn->timestamp <= _this->iss.top.clock.get_cycles())
        {
            iss_insn_t *insn = pending_insn->insn;
            iss_addr_t pc = pending_insn->pc;
            bool stalled = false;

            // Don't start if an input register is being written
            for (int i=0; i<insn->nb_in_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    if (_this->scoreboard_valid_ts[insn->in_regs[i]] > _this->iss.top.clock.get_cycles())
                    {
                        stalled = true;
                    }
                }
            }

            // Don't start if an output register is being written
            for (int i=0; i<insn->nb_out_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    if (_this->scoreboard_valid_ts[insn->out_regs[i]] > _this->iss.top.clock.get_cycles())
                    {
                        stalled = true;
                    }
                }
            }

            // Don't start if an output register is being read
            for (int i=0; i<insn->nb_out_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    if (_this->scoreboard_in_use[insn->out_regs[i]])
                    {
    		            stalled = true;
                    }
                }
            }

            if (!stalled)
            {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx)\n", pending_insn->pc);

                int block_id = insn->decoder_item->u.insn.block_id;
                AraBlock *block = _this->blocks[block_id];
                if (!block->is_full())
                {
                    // Mark output registers as being written
                    for (int i=0; i<insn->nb_out_reg; i++)
                    {
                        if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                        {
                            _this->scoreboard_valid_ts[insn->out_regs[i]] = INT64_MAX;
                        }
                    }

                    // Mark input registers as being read
                    for (int i=0; i<insn->nb_in_reg; i++)
                    {
                        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                        {
                            _this->scoreboard_in_use[insn->in_regs[i]]++;
                        }
                    }

                    block->enqueue_insn(pending_insn);

                    _this->insn_first++;
                    if (_this->insn_first == _this->queue_size)
                    {
                        _this->insn_first = 0;
                    }
                    _this->nb_waiting_insn--;
                }
            }
        }
    }

    if (_this->nb_pending_insn.get() == 0)
    {
        _this->fsm_event.disable();
    }
}

void Ara::isa_init()
{
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_isa("v"))
    {
        insn->u.insn.stub_handler = &IssWrapper::vector_insn_stub_handler;
        insn->u.insn.flags |= ISS_INSN_FLAGS_VECTOR;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_id = Ara::vlsu_id;
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_id = Ara::vlsu_id;
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("valu"))
    {
        insn->u.insn.block_id = Ara::vfpu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vslide"))
    {
        insn->u.insn.block_id = Ara::vslide_id;
    }
}
