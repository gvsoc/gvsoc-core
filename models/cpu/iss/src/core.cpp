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
}

void Core::reset(bool active)
{
}


iss_insn_t *Core::mret_handle()
{
    this->iss.exec.switch_to_full_mode();
    this->iss.irq.irq_enable = this->iss.irq.saved_irq_enable;
    this->iss.csr.mcause = 0;

    return insn_cache_get(&this->iss, this->iss.csr.mepc);
}

iss_insn_t *Core::dret_handle()
{
    this->iss.exec.switch_to_full_mode();
    this->iss.irq.irq_enable = this->iss.irq.debug_saved_irq_enable;
    this->iss.exec.debug_mode = 0;

    return insn_cache_get(&this->iss, this->iss.csr.depc);
}
