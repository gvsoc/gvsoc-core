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

#pragma once

#include "cpu/iss/include/regfile.hpp"

static iss_reg_t null_reg = 0;

inline iss_reg_t *Regfile::reg_ref(int reg)
{
    if (reg == 0)
        return &null_reg;
    else
        return &this->regs[reg];
}

inline iss_reg_t *Regfile::reg_store_ref(int reg)
{
    return &this->regs[reg];
}

inline void Regfile::set_reg(int reg, iss_reg_t value)
{
    this->regs[reg] = value;
}

inline iss_reg_t Regfile::get_reg(int reg)
{
#ifdef CONFIG_GVSOC_ISS_TIMED
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    int64_t diff = this->scoreboard_reg_timestamp[reg] - this->engine->get_cycles() - this->iss.exec.instr_event->stall_cycle_get();
    if (unlikely(diff > 0))
    {
        this->iss.timing.stall_load_dependency_account(diff);
    }
#endif
#endif

    return this->regs[reg];
}

inline iss_reg_t Regfile::get_reg_untimed(int reg)
{
    return this->regs[reg];
}

inline iss_reg64_t Regfile::get_reg64(int reg)
{
    if (reg == 0)
        return 0;
    else
        return (((uint64_t)this->regs[reg + 1]) << 32) + this->regs[reg];
}

inline void Regfile::set_reg64(int reg, iss_reg64_t value)
{
    if (reg != 0)
    {
        this->regs[reg] = value & 0xFFFFFFFF;
        this->regs[reg + 1] = value >> 32;
    }
}

inline void Regfile::set_freg(int reg, iss_freg_t value)
{
#ifdef ISS_SINGLE_REGFILE
    if (reg != 0)
        this->fregs[reg] = value;
#else
    this->fregs[reg] = value;
#endif
}

inline iss_freg_t Regfile::get_freg(int reg)
{
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    if (this->iss.top.clock.get_cycles() < this->scoreboard_freg_timestamp[reg])
    {
        this->iss.timing.stall_load_dependency_account(
            this->scoreboard_freg_timestamp[reg] - this->iss.top.clock.get_cycles()
        );
    }
#endif

#ifdef ISS_SINGLE_REGFILE
    return this->regs[reg];
#else
    return this->fregs[reg];
#endif
}

inline iss_freg_t *Regfile::freg_ref(int reg)
{
#ifdef ISS_SINGLE_REGFILE
    return (iss_freg_t *)this->reg_ref(reg);
#else
    return &this->fregs[reg];
#endif
}

inline iss_freg_t *Regfile::freg_store_ref(int reg)
{
#ifdef ISS_SINGLE_REGFILE
    return (iss_freg_t *)this->reg_store_ref(reg);
#else
    return &this->fregs[reg];
#endif
}

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
inline void Regfile::scoreboard_reg_set_timestamp(int reg, int64_t timestamp)
{
    this->scoreboard_reg_timestamp[reg] = timestamp;
}

inline void Regfile::scoreboard_freg_set_timestamp(int reg, int64_t timestamp)
{
    this->scoreboard_reg_timestamp[reg] = timestamp;
}

inline void Regfile::scoreboard_freg_invalidate(int reg)
{
    this->scoreboard_freg_timestamp[reg] = -1;
}
#endif