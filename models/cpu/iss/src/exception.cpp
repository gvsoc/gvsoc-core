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
    this->debug_handler_addr = this->iss.get_js_config()->get_int("debug_handler");
}


iss_insn_t *Exception::raise(int id)
{
#if defined(PRIV_1_10)
    if (id == ISS_EXCEPT_DEBUG)
    {
        this->iss.csr.depc = this->iss.exec.current_insn->addr;
        this->iss.irq.debug_saved_irq_enable = this->iss.irq.irq_enable;
        this->iss.irq.irq_enable = 0;
        return this->iss.irq.debug_handler;
    }
    else
    {
        this->iss.csr.epc = this->iss.exec.current_insn->addr;
        this->iss.irq.saved_irq_enable = this->iss.irq.irq_enable;
        this->iss.irq.irq_enable = 0;
        this->iss.csr.mcause = 0xb;
        iss_insn_t *insn = this->iss.irq.vectors[0];
        if (insn == NULL)
            insn = insn_cache_get(&this->iss, 0);
        return insn;
    }
#else
    this->iss.csr.epc = this->iss.exec.current_insn->addr;
    this->iss.irq.saved_irq_enable = this->iss.irq.irq_enable;
    this->iss.irq.irq_enable = 0;
    this->iss.csr.mcause = 0xb;
    iss_insn_t *insn = this->iss.irq.vectors[32 + id];
    if (insn == NULL)
        insn = insn_cache_get(iss, 0);
    return insn;
#endif
}