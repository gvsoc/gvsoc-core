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


#include "cpu/iss/include/regfile.hpp"
#include "cpu/iss/include/iss.hpp"
#include ISS_CORE_INC(class.hpp)

Regfile::Regfile(IssWrapper &top, Iss &iss)
: iss(iss)
{
    top.traces.new_trace("regfile", &this->trace, vp::DEBUG);
}


void Regfile::reset(bool active)
{
    if (active)
    {
        this->engine = this->iss.top.clock.get_engine();
        this->regs[0] = 0;
        for (int i = 0; i < ISS_NB_REGS; i++)
        {
            this->regs[i] = i == 0 ? 0 : 0x57575757;
        }
#if !defined(ISS_SINGLE_REGFILE)
        for (int i = 0; i < ISS_NB_FREGS; i++)
        {
            this->fregs[i] = 0x5757575757575757;
        }
#endif

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
        // Initialize the scoreboard so that registers can be read by default.
        for (int i = 0; i < ISS_NB_REGS; i++)
        {
            this->scoreboard_reg_timestamp[i] = 0;
        }
#if !defined(ISS_SINGLE_REGFILE)
        for (int i = 0; i < ISS_NB_FREGS; i++)
        {
            this->scoreboard_freg_timestamp[i] = 0;
        }
#endif
#endif

        this->memcheck_reg_fault = false;

        // Mark all registers as invalid after reset
        for (int i = 1; i < ISS_NB_REGS; i++)
        {
            this->regs_memcheck[i] = 0;
        }

        // Except x0 which is always valid
        this->regs_memcheck[0] = -1;
        this->regs_memcheck[ISS_NB_REGS] = -1;
    }
}
