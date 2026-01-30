/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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

Spatz::Spatz(Iss &iss)
: iss(iss), ara(iss)
{
    this->pending_insns.resize(8);
    for (int i=0; i<this->pending_insns.size(); i++)
    {
        this->pending_insns[i].id = i;
    }
}

void Spatz::reset(bool active)
{
    if (active)
    {
        this->do_flush = false;
        this->insn_first = 0;
        this->insn_last = 0;
        this->nb_pending_insn = 0;
    }

    this->ara.reset(active);
}


PendingInsn &Spatz::pending_insn_alloc()
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

PendingInsn &Spatz::pending_insn_enqueue(iss_insn_t *insn, iss_reg_t pc)
{
    PendingInsn &pending_insn = this->pending_insn_alloc();

    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pc);

    pending_insn.insn = insn;
    pending_insn.done = false;
    pending_insn.timestamp = this->iss.clock.get_cycles() + 1;
    pending_insn.pc = pc;

    // this->fsm_event.enable();

    return pending_insn;
}

void Spatz::insn_commit(PendingInsn *pending_insn)
{
    iss_insn_t *insn = pending_insn->insn;

    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "End of instruction (pc: 0x%lx)\n", pending_insn->pc);

    pending_insn->done = true;
    pending_insn->timestamp = this->iss.clock.get_cycles() + 1;

    this->iss.exec.insn_terminate(pending_insn->entry);
}

iss_reg_t Spatz::vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We stall the instruction if ara queue is full
    if (iss->arch.ara.queue_is_full())
    {
        iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "%s queue is full (pc: 0x%lx)\n",
            iss->arch.ara.queue_is_full() ? "Ara" : "Core", pc);

        iss->exec.insn_stall();
        return pc;
    }

    InsnEntry *entry = iss->exec.insn_hold(insn);

    // Account vector loads and stores to synchronize with snitch
    if (insn->decoder_item->u.insn.tags[ISA_TAG_VLOAD_ID])
    {
        iss->arch.ara.nb_pending_vaccess++;
    }

    if (insn->decoder_item->u.insn.tags[ISA_TAG_VSTORE_ID])
    {
        iss->arch.ara.nb_pending_vaccess++;
        iss->arch.ara.nb_pending_vstore++;
    }

    // Allocate a slot in cva6 queue and offload the instruction
    PendingInsn &pending_insn = iss->arch.pending_insn_enqueue(insn, pc);

    pending_insn.entry = entry;

    iss->arch.ara.insn_enqueue(&pending_insn);

    return iss_insn_next(iss, insn, pc);
}
