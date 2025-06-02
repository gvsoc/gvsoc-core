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

 #include <cpu/iss/include/cores/spatz/fpu_sequencer.hpp>
#include <cstdint>
 #include "cpu/iss/include/iss.hpp"
 #include ISS_CORE_INC(class.hpp)


Sequencer::Sequencer(IssWrapper &top, Iss &iss)
: vp::Block(&top, "sequencer"), iss(iss)
{
    iss.top.traces.new_trace("sequencer", &trace, vp::DEBUG);

    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        // Make sure we track snitch load and stores to synchronize with spatz VLSU
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("load"))
        {
            insn->u.insn.stub_handler = &Sequencer::load_store_handler;
        }

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("store"))
        {
            insn->u.insn.stub_handler = &Sequencer::load_store_handler;
        }

        // Redirect all float instructions to sequencer buffer
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("fp_op"))
        {
            insn->u.insn.stub_handler = &Sequencer::float_handler;
        }

#ifdef CONFIG_GVSOC_ISS_USE_SPATZ
        this->iss.ara.isa_init();
#endif
    }
}


void Sequencer::reset(bool active)
{
    if (active)
    {
        for (int i=0; i<ISS_NB_FREGS; i++)
        {
            this->scoreboard_freg_timestamp[i] = 0;
        }
    }
}


void Sequencer::build()
{
}


iss_reg_t Sequencer::load_store_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We need to stall snitch if:
    // - snitch wants to do a store while spatz is having pending memory accesses
    // - snitch wants to do a load while spatz is having pending loads
    if (insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID] && iss->ara.nb_pending_vaccess ||
        insn->decoder_item->u.insn.tags[ISA_TAG_LOAD_ID] && iss->ara.nb_pending_vstore)
    {
        iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to on-going vector access (is_store: %d, pending_vaccess: %d, pending_vstore: %d\n",
            insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID], iss->ara.nb_pending_vaccess, iss->ara.nb_pending_vstore);

        return pc;
    }
    return insn->stub_handler(iss, insn, pc);
}

iss_reg_t Sequencer::float_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Checking float instructions register dependencies\n");

    int64_t cycles = iss->top.clock.get_cycles();
    // Block the instruction until all float input registers are available, since some vector
    // instructions having them as output may still be pending
    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if (insn->in_regs_fp[i] && iss->sequencer.scoreboard_freg_timestamp[insn->in_regs[i]] > cycles)
        {
            iss->sequencer.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to register dependency (reg: %d)\n", insn->in_regs[i]);
            return pc;
        }
    }

    return insn->stub_handler(iss, insn, pc);
}
