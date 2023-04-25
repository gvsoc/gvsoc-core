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
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <vp/vp.hpp>
#include <exception.hpp>
#include "iss.hpp"


Exception::Exception(Iss &iss)
: iss(iss)
{

}

void Exception::build()
{
    this->iss.top.traces.new_trace("exception", &this->trace, vp::DEBUG);

    this->debug_handler_addr = this->iss.top.get_js_config()->get_int("debug_handler");
}


void Exception::raise(iss_insn_t *insn, int id)
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Raising exception (id: %d)\n", id);

    this->iss.exec.switch_to_full_mode();

    if (id == ISS_EXCEPT_DEBUG)
    {
        this->iss.csr.depc = insn->addr;
        this->iss.irq.debug_saved_irq_enable = this->iss.irq.irq_enable.get();
        this->iss.irq.irq_enable.set(0);
        insn = this->iss.irq.debug_handler;
    }
    else
    {
        int next_mode = PRIV_M;
        if ((this->iss.csr.medeleg.value >> id) & 1)
        {
            next_mode = PRIV_S;
        }

        if (next_mode == PRIV_M)
        {
            this->iss.csr.mepc.value = insn->addr;
            this->iss.csr.mstatus.mie = 0;
            this->iss.csr.mstatus.mpie = this->iss.irq.irq_enable.get();
            this->iss.csr.mstatus.mpp = this->iss.core.mode_get();
        }
        else
        {
            this->iss.csr.sepc.value = insn->addr;
            this->iss.csr.mstatus.sie = 0;
            this->iss.csr.mstatus.spie = this->iss.irq.irq_enable.get();
            this->iss.csr.mstatus.spp = this->iss.core.mode_get();
        }
        this->iss.core.mode_set(next_mode);

        this->iss.irq.irq_enable.set(0);

#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
        if (next_mode == PRIV_M)
        {
            insn = this->iss.irq.mtvec_insn;
            this->iss.csr.mcause.value = id;
        }
        else
        {
            insn = this->iss.irq.stvec_insn;
            this->iss.csr.scause.value = id;
        }
#else
        this->iss.csr.mcause.value = 0xb;
        insn = this->iss.irq.vectors[0];
#endif
        if (insn == NULL)
            insn = insn_cache_get(&this->iss, 0);
    }

    this->iss.exec.exception_insn = insn;
}