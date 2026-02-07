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
}

void Spatz::reset(bool active)
{
    this->ara.reset(active);
}


void Spatz::insn_commit(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "End of instruction (pc: 0x%lx)\n", insn->addr);

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

    iss->arch.ara.insn_enqueue(entry);

    return iss_insn_next(iss, insn, pc);
}
