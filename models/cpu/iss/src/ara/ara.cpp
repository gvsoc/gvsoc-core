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

 #include <cstdint>
#include "cpu/iss/include/iss.hpp"
#include "cpu/iss/include/cores/ara/ara.hpp"


Ara::Ara(IssWrapper &top, Iss &iss)
    : Block(&top, "ara"), iss(iss),
    fsm_event(this, &Ara::fsm_handler),
    nb_pending_insn(*this, "nb_pending_insn", 8, true),
    nb_waiting_insn(*this, "nb_waiting_insn", 8, true),
    queue_full(*this, "queue_full", 1, true)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event_string("label", &this->event_label);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);

    this->nb_lanes = top.get_js_config()->get_int("ara/nb_lanes");
    this->iss.vector.VLEN = CONFIG_ISS_VLEN;
    this->blocks.resize(Ara::nb_blocks);
    this->blocks[Ara::vlsu_id] = new AraVlsu(*this, top);
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
            this->scoreboard_out_use[i] = 0;
            this->scoreboard_chained[i] = NULL;
        }
        this->insn_first_waiting = 0;
        this->insn_first = 0;
        this->insn_last = 0;
#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
        this->nb_pending_vaccess = 0;
        this->nb_pending_vstore = 0;
#endif
    }
}

void Ara::build()
{
    this->pending_insns.resize(this->queue_size);
}

PendingInsn *Ara::pending_insn_alloc(PendingInsn *cva6_pending_insn)
{
    // The new instruction is marked as pending and also waiting for dependecy resolution
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn.inc(1);
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }
    int insn_id = this->insn_last;
    this->insn_last = (this->insn_last + 1) % this->queue_size;

    // Copy the pending instruction coming from CVA6 since the commit will free it.
    this->pending_insns[insn_id] = *cva6_pending_insn;

    this->pending_insns[insn_id].chained = false;

#if !defined(CONFIG_GVSOC_ISS_USE_SPATZ)
    // We commit now the instruction to CVA6 since there is nothing to commit in CVA6.
    // FIXME probably there is something wrong with fpu registers dependency management.
    // Align it on Spatz to fix it
    this->iss.top.insn_commit(cva6_pending_insn);
#endif

    return &this->pending_insns[insn_id];
}

void Ara::insn_enqueue(PendingInsn *cva6_pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", cva6_pending_insn->pc);
    PendingInsn *pending_insn = this->pending_insn_alloc(cva6_pending_insn);

	iss_insn_t *insn = pending_insn->insn;
    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&pending_insn->pc);
    this->event_label.event_string(pending_insn->insn->desc->label, false);

    // Mark the instruction to be handled in the next cycle in case the FSM is already active
    // to prevent it from handling it in the next cycle
    pending_insn->timestamp = this->iss.top.clock.get_cycles() + 1;

    // Copy the CVA6 register since we will use it later and it will probably change before that
    int reg = insn->in_regs[0];
    int reg_2 = insn->in_regs[1];
    uint64_t reg_value, reg_value_2;

    if (insn->nb_in_reg > 0)
    {
        if (insn->in_regs_fp[0])
        {
            reg_value = this->iss.regfile.get_freg_untimed(reg);
        }
        else
        {
            reg_value = this->iss.regfile.get_reg_untimed(reg);
        }
        pending_insn->reg = reg_value;
    }

    if (insn->nb_in_reg > 1)
    {
        if (insn->in_regs_fp[1])
        {
            reg_value_2 = this->iss.regfile.get_freg_untimed(reg_2);
        }
        else
        {
            reg_value_2 = this->iss.regfile.get_reg_untimed(reg_2);
        }
        pending_insn->reg_2 = reg_value_2;
    }

    // Enable the FSM to let it handle the pending instructions
    this->fsm_event.enable();
}

void Ara::insn_end(PendingInsn *pending_insn)
{
    iss_insn_t *insn = pending_insn->insn;

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
    // If the ended instruction is a load or store, decrement associated counters used for
    // for synchronizing snitch and spatz memory accesses
    if (insn->decoder_item->u.insn.tags[ISA_TAG_VLOAD_ID])
    {
        this->nb_pending_vaccess--;
    }

    if (insn->decoder_item->u.insn.tags[ISA_TAG_VSTORE_ID])
    {
        this->nb_pending_vaccess--;
        this->nb_pending_vstore--;
    }
#endif

    // Mark the instruction as done. THe FSM will remove it when it is at the head of the queue
    pending_insn->done = true;

    // Now that the instruction is over, execute the handler to functionally model it. This will
    // write the output register.
    // Store the instruction register as it will be used by the handler.
    this->current_insn_reg = pending_insn->reg;
    this->current_insn_reg_2 = pending_insn->reg_2;
    // Force trace dump since the core may be stalled which would skip trace
    this->iss.trace.force_trace_dump = true;
    insn->stub_handler(&this->iss, insn, pending_insn->pc);
    this->iss.trace.force_trace_dump = false;

    // Mark output registers as ready in next cycle
    for (int i=0; i<insn->nb_out_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
        {
            // Since the instruction is done, we can disable chaining, this will trigger end
            // of chained instructions
            this->scoreboard_committed[insn->out_regs[i]] = 0;

            // Be careful to make the output register valid only if no instruction is already
            // writing it, this can happen when instructions are chained
            this->scoreboard_out_use[insn->out_regs[i]]--;
            if (this->scoreboard_out_use[insn->out_regs[i]] == 0)
            {
                this->scoreboard_valid_ts[insn->out_regs[i]] = this->iss.top.clock.get_cycles() + 1;
            }
        }
    }

    // Mark intput registers as not in use anymore
    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
        {
            this->scoreboard_in_use[insn->in_regs[i]]--;
            // Cancel the register chaining if it was involved in this instruction, this will
            // allow other instructions to use this register for chaining
            if (this->scoreboard_chained[insn->in_regs[i]] == pending_insn)
            {
                this->scoreboard_chained[insn->in_regs[i]] = NULL;
            }
        }
    }

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
    // Commit the instruction. This may unblock other instructions waiting for float registers
    this->iss.top.insn_commit(pending_insn);
#endif
}

void Ara::insn_commit(int reg, int size)
{
    // If it is the first time an element is committed, we may need to start an instruction.
    if (this->scoreboard_committed[reg] == 0)
    {
        this->fsm_event.enqueue();
    }
    this->scoreboard_committed[reg] += size;
}

void Ara::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Ara *_this = (Ara *)__this;

    // Check if the pending instruction at the head must be terminated
    if (_this->nb_pending_insn.get() > 0)
    {
        PendingInsn *pending_insn = &_this->pending_insns[_this->insn_first];
        if (pending_insn->done)
        {
            _this->nb_pending_insn.dec(1);
            if (_this->nb_pending_insn.get() == 0)
            {
                uint8_t zero = 0;
                _this->event_active.event(&zero);
                _this->event_label.event_string((char *)1, false);
            }
            _this->queue_full.set(false);

            _this->insn_first = (_this->insn_first + 1) % _this->queue_size;
        }
    }

    // Check if the dependencies of the pending instruction at the head of the waiting instruction
    // are resolved.
    if (_this->nb_waiting_insn.get() > 0)
    {
        PendingInsn *pending_insn = &_this->pending_insns[_this->insn_first_waiting];

        if (pending_insn->timestamp <= _this->iss.top.clock.get_cycles())
        {
            iss_insn_t *insn = pending_insn->insn;
            iss_addr_t pc = pending_insn->pc;
            bool stalled = false;

            #ifdef CONFIG_GVSOC_ISS_VECTOR_CHAINING
            // First check if the instruction can be chained
            for (int i=0; i<insn->nb_in_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    // The instruction can be chained if the register has already elements and is
                    // not already chained with another instructions
                    if (_this->scoreboard_committed[insn->in_regs[i]] != 0 && !_this->scoreboard_chained[insn->in_regs[i]])
                    {
                        pending_insn->chained = true;
                        _this->scoreboard_chained[insn->in_regs[i]] = pending_insn;
                    }
                }
            }
            #endif

            // Don't start if an input register is being written unless vector chaining is enabled
            // and some elements have alreay been committed
            for (int i=0; i<insn->nb_in_reg; i++)
            {
                if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                {
                    // This register can stall the instruction only if it not chained to it
                    if (_this->scoreboard_chained[insn->in_regs[i]] != pending_insn)
                    {
                        if (_this->scoreboard_valid_ts[insn->in_regs[i]] > _this->iss.top.clock.get_cycles())
                        {
                            stalled = true;
                            break;
                        }
                    }
                }
            }

            // Don't start if an output register is being written unless vector chaining is enabled
            // and some elements have alreay been committed. In the HW, the instruction will write
            // after the previous instruction only the part which is already committed
            if (!stalled)
            {
                for (int i=0; i<insn->nb_out_reg; i++)
                {
                    if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                    {
                        // This register can stall the instruction only if it not chained to it
                        if (_this->scoreboard_chained[insn->in_regs[i]] != pending_insn)
                        {
                            if (_this->scoreboard_valid_ts[insn->out_regs[i]] > _this->iss.top.clock.get_cycles())
                            {
                                stalled = true;
                                break;
                            }
                        }
                    }
                }
            }

            // Don't start if an output register is being read
            if (!stalled)
            {
                for (int i=0; i<insn->nb_out_reg; i++)
                {
                    if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                    {
                        if (_this->scoreboard_in_use[insn->out_regs[i]])
                        {
          		            stalled = true;
                            break;
                        }
                    }
                }
            }

            if (!stalled)
            {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx)\n", pending_insn->pc);

                int block_id = insn->decoder_item->u.insn.block_id;

                // Some instructions like vsetvli have no associated block and must be execute by
                // the core
                if (block_id == -1)
                {
                    // Make sure there is no on-going instruction before executing vsetvli
                    if (_this->nb_pending_insn.get() == _this->nb_waiting_insn.get())
                    {
                        _this->event_pc.event((uint8_t *)&pending_insn->pc);

                        _this->insn_first_waiting = (_this->insn_first_waiting + 1) % _this->queue_size;
                        _this->nb_waiting_insn.dec(1);
                        _this->insn_end(pending_insn);
                    }
                }
                else
                {
                    _this->event_pc.event((uint8_t *)&pending_insn->pc);

                    AraBlock *block = _this->blocks[block_id];
                    if (!block->is_full())
                    {
                        // Mark output registers as being written
                        for (int i=0; i<insn->nb_out_reg; i++)
                        {
                            if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                            {
                                _this->scoreboard_valid_ts[insn->out_regs[i]] = INT64_MAX;
                                _this->scoreboard_committed[insn->out_regs[i]] = 0;
                                _this->scoreboard_out_use[insn->out_regs[i]]++;
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

                        // Enqueue the instruction to the processing block
                        block->enqueue_insn(pending_insn);

                        _this->insn_first_waiting = (_this->insn_first_waiting + 1) % _this->queue_size;
                        _this->nb_waiting_insn.dec(1);
                    }
                }
            }
        }
    }

    if (_this->nb_pending_insn.get() == 0)
    {
        _this->event_queue.event_highz();
        _this->event_pc.event_highz();
        _this->fsm_event.disable();
    }
}

void Ara::isa_init()
{
    // Make sure all vector instructions are handled with dedicated handler so that they can be
    // offloaded to ara
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_isa("v"))
    {
        insn->u.insn.stub_handler = &IssWrapper::vector_insn_stub_handler;
        // Vector instructions need to be handled differently in cva6
        insn->u.insn.flags |= ISS_INSN_FLAGS_VECTOR;
    }
    // Associate instruction to processing blocks
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_id = Ara::vlsu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_id = Ara::vlsu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vothers"))
    {
        insn->u.insn.block_id = Ara::vfpu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vslide"))
    {
        insn->u.insn.block_id = Ara::vslide_id;
    }

    for (AraBlock *block: this->blocks)
    {
        block->isa_init();
    }
}
