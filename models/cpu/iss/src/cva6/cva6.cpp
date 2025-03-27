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
#include <string.h>


void IssWrapper::start()
{
    vp_assert_always(this->iss.lsu.data.is_bound(), &this->trace, "Data master port is not connected\n");
    vp_assert_always(this->iss.prefetcher.fetch_itf.is_bound(), &this->trace, "Fetch master port is not connected\n");
    // vp_assert_always(this->irq_ack_itf.is_bound(), &this->trace, "IRQ ack master port is not connected\n");

    this->trace.msg("ISS start (fetch: %d, boot_addr: 0x%lx)\n",
        iss.exec.fetch_enable_reg.get(), get_js_config()->get_child_int("boot_addr"));

    this->iss.timing.background_power.leakage_power_start();
    this->iss.timing.background_power.dynamic_power_start();

    this->iss.lsu.start();
    this->iss.gdbserver.start();
}

void IssWrapper::stop()
{
    this->iss.insn_cache.stop();
    this->iss.gdbserver.stop();
}

void IssWrapper::reset(bool active)
{
    this->iss.prefetcher.reset(active);
    this->iss.csr.reset(active);
    this->iss.exec.reset(active);
    this->iss.core.reset(active);
    this->iss.mmu.reset(active);
    this->iss.pmp.reset(active);
    this->iss.irq.reset(active);
    this->iss.lsu.reset(active);
    this->iss.timing.reset(active);
    this->iss.trace.reset(active);
    this->iss.regfile.reset(active);
    this->iss.decode.reset(active);
    this->iss.gdbserver.reset(active);
    this->iss.syscalls.reset(active);
    this->iss.vector.reset(active);
    this->iss.ara.reset(active);

    this->do_flush = false;
    this->insn_first = 0;
    this->insn_last = 0;
    this->nb_pending_insn = 0;
}

IssWrapper::IssWrapper(vp::ComponentConf &config)
    : vp::Component(config), iss(*this),
    fsm_event(this, &IssWrapper::fsm_handler)
{
    this->iss.syscalls.build();
    this->iss.decode.build();
    this->iss.exec.build();
    this->iss.insn_cache.build();
    this->iss.dbgunit.build();
    this->iss.csr.build();
    this->iss.lsu.build();
    this->iss.irq.build();
    this->iss.trace.build();
    this->iss.timing.build();
    this->iss.gdbserver.build();
    this->iss.core.build();
    this->iss.mmu.build();
    this->iss.pmp.build();
    this->iss.exception.build();
    this->iss.prefetcher.build();
    this->iss.vector.build();
    this->iss.ara.build();

    this->traces.new_trace("wrapper", &this->trace, vp::DEBUG);

    this->pending_insns.resize(8);
    for (int i=0; i<this->pending_insns.size(); i++)
    {
        this->pending_insns[i].id = i;
    }

    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("all"))
        {
            insn->u.insn.stub_handler = &IssWrapper::insn_stub_handler;
        }

#ifdef CONFIG_ISS_HAS_VECTOR
        this->iss.ara.isa_init();
#endif
    }
}

void IssWrapper::insn_commit(PendingInsn *pending_insn)
{
    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "End of instruction (pc: 0x%lx)\n", pending_insn->pc);

    pending_insn->done = true;
    pending_insn->timestamp = this->clock.get_cycles() + 1;
}

void IssWrapper::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IssWrapper *_this = (IssWrapper *)__this;

    if (_this->nb_pending_insn == 0)
    {
        _this->fsm_event.disable();
        return;
    }

    PendingInsn &pending_insn = _this->pending_insns[_this->insn_first];

    // Check if the instruction at the head is done
    if (pending_insn.done &&
        pending_insn.timestamp <= _this->clock.get_cycles())
    {
        _this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "Commit instruction (pc: 0x%lx)\n", pending_insn.pc);

        iss_insn_t *insn = pending_insn.insn;
        _this->insn_first++;
        if (_this->insn_first == _this->max_pending_insn)
        {
            _this->insn_first = 0;
        }
        _this->nb_pending_insn--;

        // Only execute the handler if we haven't offloaded the instruction to ara since it will
        // take care of executing the handler
        if ((insn->flags & ISS_INSN_FLAGS_VECTOR) == 0)
        {
            iss_addr_t next_pc = insn->stub_handler(&_this->iss, insn, pending_insn.pc);

            for (int i=0; i<insn->nb_out_reg; i++)
            {
                if (!insn->out_regs_fp[i])
                {
                    _this->iss.regfile.scoreboard_reg_timestamp[insn->out_regs[i]] = _this->clock.get_cycles();
                }
                else
                {
                    _this->iss.regfile.scoreboard_freg_timestamp[insn->out_regs[i]] = _this->clock.get_cycles();
                }
            }

            if (_this->do_flush && _this->nb_pending_insn == 0)
            {
                _this->do_flush = false;
                _this->iss.exec.current_insn = next_pc;
            }
        }
    }
}

PendingInsn &IssWrapper::pending_insn_alloc()
{
    this->nb_pending_insn++;
    int insn_id = this->insn_last;
    this->insn_last++;
    if (this->insn_last == this->max_pending_insn)
    {
        this->insn_last = 0;
    }
    return this->pending_insns[insn_id];
}

PendingInsn &IssWrapper::pending_insn_enqueue(iss_insn_t *insn, iss_reg_t pc)
{
    PendingInsn &pending_insn = this->pending_insn_alloc();

    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pc);

    pending_insn.insn = insn;
    pending_insn.done = false;
    pending_insn.timestamp = this->clock.get_cycles() + 1;
    pending_insn.pc = pc;

    this->fsm_event.enable();

    return pending_insn;
}

iss_reg_t IssWrapper::insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We stall the instruction if we hit a branch and we are flushing the queue or if the queue
    // is full
    if (iss->top.do_flush || iss->top.nb_pending_insn == iss->top.max_pending_insn)
    {
        return pc;
    }

    // Invalidate the output registers to block any instruction needing one of these instructions
    for (int i=0; i<insn->nb_out_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_FREG) == 0)
        {
            iss->regfile.scoreboard_reg_invalidate(insn->out_regs[i]);
        }
        else
        {
            iss->regfile.scoreboard_freg_invalidate(insn->out_regs[i]);
        }
    }

    // Push the instruction to the queue
    PendingInsn &pending_insn = iss->top.pending_insn_enqueue(insn, pc);
    // Mark it as done as we have no need to delay it. The handler will b eexecuted when the FSM
    // decides to commit it
    iss->top.insn_commit(&pending_insn);

    // In case the instruction is a branch, we can't know now what is the next instruction.
    // Justr wait until the queue is empty.
    // This will need proper branch prediction at some point.
    if (insn->decoder_item->u.insn.tags[ISA_TAG_BRANCH_ID])
    {
        iss->top.do_flush = true;
        return pc;
    }

    return iss_insn_next(iss, insn, pc);
}

iss_reg_t IssWrapper::vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We stall the instruction if cva6 queue or ara queue is full
    if (iss->top.nb_pending_insn == iss->top.max_pending_insn || iss->ara.queue_is_full())
    {
        iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "%s queue is full (pc: 0x%lx)\n",
            iss->ara.queue_is_full() ? "Ara" : "Core", pc);
        return pc;
    }

    // Only offload the instruction once all input registers are ready
    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) == 0)
        {
            if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_FREG) == 0)
            {
                if (iss->regfile.scoreboard_reg_timestamp[insn->in_regs[i]] == -1)
                {
                    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Blocked due to int reg dependency (pc: 0x%lx, reg: %d)\n",
                        pc, insn->in_regs[i]);
                    return pc;
                }
            }
            else
            {
                if (iss->regfile.scoreboard_freg_timestamp[insn->in_regs[i]] == -1)
                {
                    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Blocked due to float reg dependency (pc: 0x%lx, reg: %d)\n",
                        pc, insn->in_regs[i]);
                    return pc;
                }
            }
        }
    }

    // Allocate a slot in cva6 queue and offload the instruction
    PendingInsn &pending_insn = iss->top.pending_insn_enqueue(insn, pc);

    iss->ara.insn_enqueue(&pending_insn);

    return iss_insn_next(iss, insn, pc);
}
