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

#include "iss.hpp"

Core::Core(Iss &iss)
: iss(iss)
{
}

void Core::build()
{
    this->iss.top.traces.new_trace("core", &this->trace, vp::DEBUG);

    this->iss.csr.mstatus.register_callback(std::bind(&Core::mstatus_update, this, std::placeholders::_1));
}

void Core::reset(bool active)
{
    this->mode = PRIV_M;
}


iss_insn_t *Core::mret_handle()
{
    this->iss.exec.switch_to_full_mode();

    this->mode = this->iss.csr.mstatus.mpp;
#ifdef CONFIG_GVSOC_ISS_USER_MODE
    this->iss.csr.mstatus.mpp = PRIV_U;
#else
    this->iss.csr.mstatus.mpp = PRIV_M;
#endif
    this->iss.irq.irq_enable.set(this->iss.csr.mstatus.mpie);
    this->iss.csr.mstatus.mie = this->iss.csr.mstatus.mpie;
    this->iss.csr.mstatus.mpie = 1;
    this->iss.csr.mcause.value = 0;

    return insn_cache_get(&this->iss, this->iss.csr.mepc.value);
}

iss_insn_t *Core::sret_handle()
{
    this->iss.exec.switch_to_full_mode();

    this->mode = this->iss.csr.mstatus.spp;
    this->iss.csr.mstatus.spp = PRIV_U;
    this->iss.irq.irq_enable.set(this->iss.csr.mstatus.spie);
    this->iss.csr.mstatus.sie = this->iss.csr.mstatus.spie;
    this->iss.csr.mstatus.spie = 1;
    this->iss.csr.scause.value = 0;

    return insn_cache_get(&this->iss, this->iss.csr.sepc.value);
}

iss_insn_t *Core::dret_handle()
{
    this->iss.exec.switch_to_full_mode();
    this->iss.irq.irq_enable.set(this->iss.irq.debug_saved_irq_enable);
    this->iss.exec.debug_mode = 0;

    return insn_cache_get(&this->iss, this->iss.csr.depc);
}


bool Core::mstatus_update(iss_reg_t value)
{
    this->iss.csr.mstatus.value = value;

#ifdef CONFIG_GVSOC_ISS_RI5KY
    this->iss.csr.mstatus.value &= 0x21899;
#endif

    this->iss.timing.stall_insn_dependency_account(4);
    this->iss.irq.global_enable(this->iss.csr.mstatus.mie);

    return false;
}