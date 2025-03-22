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
    this->traces.new_trace_event("active_box", &this->trace_active_box, 1);
    this->traces.new_trace_event("active", &this->trace_active, 1);

    int nb_lanes = top.get_js_config()->get_int("ara/nb_lanes");
    this->iss.vector.VLEN = nb_lanes * 64;
    this->blocks.resize(Ara::nb_blocks);
    this->blocks[Ara::vlsu_id] = new AraVlsu(*this, nb_lanes);
}

void Ara::reset(bool active)
{
}

void Ara::build()
{
    this->queue_size = 8;
}

void Ara::insn_enqueue(PendingInsn *insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", insn->pc);
    uint8_t one = 1;
    this->trace_active_box.event(&one);
    this->trace_active.event(&one);
    this->nb_pending_insn.inc(1);
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }

    this->pending_insns.push(insn);
    insn->timestamp = this->iss.top.clock.get_cycles() + 1;
    int reg = insn->insn->in_regs[0];
    uint64_t reg_value;
    if (insn->insn->in_regs_fp[0])
    {
        reg_value = this->iss.regfile.get_freg_untimed(reg);
    }
    else
    {
        reg_value = this->iss.regfile.get_reg_untimed(reg);
    }
    insn->reg = reg_value;
    this->base_addr_queue.push(reg_value);

    this->fsm_event.enable();
}

void Ara::insn_end(PendingInsn *pending_insn)
{
    iss_insn_t *insn = pending_insn->insn;
    this->nb_pending_insn.dec(1);
    if (this->nb_pending_insn.get() == 0)
    {
        this->trace_active_box.event_highz();
        uint8_t zero = 0;
        this->trace_active.event(&zero);
    }
    this->queue_full.set(false);

    this->pending_insn = pending_insn;
    insn->stub_handler(&this->iss, insn, pending_insn->pc);

    this->iss.top.insn_commit(pending_insn);

    for (int i=0; i<insn->nb_out_reg; i++)
    {
        this->iss.regfile.scoreboard_reg_set_timestamp(insn->out_regs[i], 0, 0);
    }

    this->base_addr_queue.pop();
}

void Ara::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Ara *_this = (Ara *)__this;

    if (_this->pending_insns.size() > 0 &&
        _this->pending_insns.front()->timestamp <= _this->iss.top.clock.get_cycles())
    {
        PendingInsn *pending_insn = _this->pending_insns.front();
        iss_insn_t *insn = pending_insn->insn;
        iss_addr_t pc = pending_insn->pc;

        int block_id = insn->decoder_item->u.insn.block_id;
        if (block_id != -1)
        {
            AraBlock *block = _this->blocks[block_id];
            if (!block->is_full())
            {
                _this->pending_insns.pop();

                block->enqueue_insn(pending_insn);
            }
        }
        else
        {
            _this->pending_insns.pop();

            _this->insn_end(pending_insn);
        }
    }

    if (_this->pending_insns.size() == 0)
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
}
