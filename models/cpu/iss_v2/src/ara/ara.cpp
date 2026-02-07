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
#include "cpu/iss_v2/include/cores/ara/ara.hpp"


Ara::Ara(Iss &iss)
    : Block(&iss, "ara"), iss(iss),
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

    this->nb_lanes = iss.get_js_config()->get_int("ara/nb_lanes");
    this->iss.vector.VLEN = CONFIG_ISS_VLEN;
    this->blocks.resize(Ara::nb_blocks);
    this->blocks[Ara::vlsu_id] = new AraVlsu(*this, iss);
    this->blocks[Ara::vfpu_id] = new AraVcompute(*this, "vfpu");
    this->blocks[Ara::vslide_id] = new AraVcompute(*this, "vslide");

    this->pending_insns.resize(this->queue_size);


    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        // Make sure we track snitch load and stores to synchronize with spatz VLSU
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("load"))
        {
            insn->u.insn.stub_handler = &Ara::load_store_handler;
        }

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("store"))
        {
            insn->u.insn.stub_handler = &Ara::load_store_handler;
        }

        this->isa_init();
    }
}

iss_reg_t Ara::load_store_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We need to stall snitch if:
    // - snitch wants to do a store while spatz is having pending memory accesses
    // - snitch wants to do a load while spatz is having pending loads
    if (insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID] && iss->arch.ara.nb_pending_vaccess ||
        insn->decoder_item->u.insn.tags[ISA_TAG_LOAD_ID] && iss->arch.ara.nb_pending_vstore)
    {
        iss->arch.ara.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to on-going vector access (is_store: %d, pending_vaccess: %d, pending_vstore: %d\n",
            insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID], iss->arch.ara.nb_pending_vaccess, iss->arch.ara.nb_pending_vstore);

        return pc;
    }
    return insn->stub_handler(iss, insn, pc);
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
#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
        this->nb_pending_vaccess = 0;
        this->nb_pending_vstore = 0;
#endif

        this->insn_id_table = (1 << this->queue_size) - 1;

        for (PendingInsn &pending_insn: this->pending_insns)
        {
            pending_insn.valid = false;
        }
    }
}

PendingInsn *Ara::pending_insn_alloc(InsnEntry *entry)
{
    // The new instruction is marked as pending and also waiting for dependecy resolution
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn.inc(1);
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }
    int insn_id = this->alloc_id();

    // Copy the pending instruction coming from CVA6 since the commit will free it.
    PendingInsn *pending_insn = &this->pending_insns[insn_id];

    pending_insn->valid = true;
    pending_insn->entry = entry;
    pending_insn->chained = false;
    pending_insn->waiting = true;

    printf("ALLOC %p %p %p %d %x\n", pending_insn, pending_insn->entry, entry, insn_id, entry->addr);

    return pending_insn;
}

void Ara::insn_enqueue(InsnEntry *entry)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", entry->addr);
    PendingInsn *pending_insn = this->pending_insn_alloc(entry);

	iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&insn->addr);
    this->event_label.event_string(insn->desc->label, false);

    // Mark the instruction to be handled in the next cycle in case the FSM is already active
    // to prevent it from handling it in the next cycle
    pending_insn->timestamp = this->iss.clock.get_cycles() + 1;

    // Copy the CVA6 register since we will use it later and it will probably change before that
    int reg = insn->in_regs[0];
    int reg_2 = insn->in_regs[1];
    int reg_3 = insn->in_regs[2];
    uint64_t reg_value, reg_value_2, reg_value_3;

    if (insn->nb_in_reg > 0)
    {
        reg_value = this->iss.regfile.get_reg_untimed(reg);
        pending_insn->reg = reg_value;
    }

    if (insn->nb_in_reg > 1)
    {
        reg_value_2 = this->iss.regfile.get_reg_untimed(reg_2);
    }

    if (insn->nb_in_reg > 2)
    {
        reg_value_3 = this->iss.regfile.get_reg_untimed(reg_3);
        pending_insn->reg_3 = reg_value_3;
    }

    // Enable the FSM to let it handle the pending instructions
    this->fsm_event.enable();
}

void Ara::insn_end(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    printf("INSN END %x\n", insn->addr);

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
    insn->stub_handler(&this->iss, insn, insn->addr);

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
                this->scoreboard_valid_ts[insn->out_regs[i]] = this->iss.clock.get_cycles() + 1;
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
    this->iss.arch.insn_commit(pending_insn);
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
        for (PendingInsn &pending_insn: _this->pending_insns)
        {
            if (pending_insn.valid && pending_insn.done)
            {
                pending_insn.valid = false;
                pending_insn.done = false;
                _this->nb_pending_insn.dec(1);
                _this->free_id(pending_insn.id);
                if (_this->nb_pending_insn.get() == 0)
                {
                    uint8_t zero = 0;
                    _this->event_active.event(&zero);
                    _this->event_label.event_string((char *)1, false);
                }
                _this->queue_full.set(false);
            }
        }
    }

    // Check if the dependencies of the pending instruction at the head of the waiting instruction
    // are resolved.
    if (_this->nb_waiting_insn.get() > 0)
    {
        printf("START CHECKING\n");
        for (int i=0; i<_this->pending_insns.size(); i++)
        {
            PendingInsn *pending_insn = &_this->pending_insns[i];
            if (pending_insn->valid && pending_insn->waiting)
            {
                iss_insn_t *insn = _this->iss.exec.get_insn(pending_insn->entry);
                printf("  %d %x %p waiting %d done %d valid %d in mask %x out mask %x\n",
                    i, insn->addr, pending_insn, pending_insn->waiting, pending_insn->done, pending_insn->valid,
                    insn->sb_in_vreg_mask, insn->sb_out_vreg_mask);
                if (pending_insn->timestamp <= _this->iss.clock.get_cycles())
                {
                    iss_insn_t *insn = _this->iss.exec.get_insn(pending_insn->entry);
                    iss_addr_t pc = insn->addr;
                    bool stalled = false;

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

                    // Don't start if an input register is being written unless vector chaining is enabled
                    // and some elements have alreay been committed
                    for (int i=0; i<insn->nb_in_reg; i++)
                    {
                        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) != 0)
                        {
                            // This register can stall the instruction only if it not chained to it
                            if (_this->scoreboard_chained[insn->in_regs[i]] != pending_insn)
                            {
                                if (_this->scoreboard_valid_ts[insn->in_regs[i]] > _this->iss.clock.get_cycles())
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
                                    if (_this->scoreboard_valid_ts[insn->out_regs[i]] > _this->iss.clock.get_cycles())
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
                        int block_id = insn->decoder_item->u.insn.block_id;

                        // Some instructions like vsetvli have no associated block and must be execute by
                        // the core
                        if (block_id == -1)
                        {
                            // Make sure there is no on-going instruction before executing vsetvli
                            if (_this->nb_pending_insn.get() == _this->nb_waiting_insn.get())
                            {
                                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx)\n", insn->addr);

                                _this->event_pc.event((uint8_t *)&insn->addr);

                                pending_insn->waiting = false;
                                _this->nb_waiting_insn.dec(1);
                                _this->insn_end(pending_insn);
                            }
                        }
                        else
                        {
                            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx)\n", insn->addr);

                            _this->event_pc.event((uint8_t *)&insn->addr);

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

                                printf("ENQUEUE %p %x %d\n", pending_insn, insn->addr, pending_insn->waiting );
                                pending_insn->waiting = false;

                                // Enqueue the instruction to the processing block
                                block->enqueue_insn(pending_insn);

                                _this->nb_waiting_insn.dec(1);
                            }
                        }
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
        insn->u.insn.stub_handler = &Spatz::vector_insn_stub_handler;
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
