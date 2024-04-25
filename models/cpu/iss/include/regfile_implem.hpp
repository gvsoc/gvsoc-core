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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
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

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
inline void Regfile::scoreboard_reg_check(int reg)
{
#ifdef CONFIG_GVSOC_ISS_TIMED
    int64_t diff = this->scoreboard_reg_timestamp[reg] - this->engine->get_cycles() - this->iss.exec.stall_cycles;

    if (unlikely(diff > 0))
    {
        int stall_reason = this->scoreboard_reg_stall_reason[reg];
        this->iss.timing.stall_cycles_account(diff);
        if (stall_reason != -1)
        {
            this->iss.timing.event_account(stall_reason, diff);
        }
    }
#endif
}

inline void Regfile::scoreboard_freg_check(int reg)
{
#ifdef CONFIG_GVSOC_ISS_TIMED
#if defined(ISS_SINGLE_REGFILE)
    scoreboard_reg_check(reg);
#else
    int64_t diff = this->scoreboard_freg_timestamp[reg] - this->engine->get_cycles() - this->iss.exec.stall_cycles;

    if (unlikely(diff > 0))
    {
        int stall_reason = this->scoreboard_freg_stall_reason[reg];
        this->iss.timing.stall_cycles_account(diff);
        if (stall_reason != -1)
        {
            this->iss.timing.event_account(stall_reason, diff);
        }
    }
#endif
#endif
}
#endif

inline iss_reg_t Regfile::get_reg(int reg)
{
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    this->scoreboard_reg_check(reg);
#endif
    return this->regs[reg];
}

inline iss_reg_t Regfile::get_reg_untimed(int reg)
{
    return this->regs[reg];
}

inline iss_reg64_t Regfile::get_reg64_untimed(int reg)
{
    if (reg == 0)
        return 0;
    else
        return (((uint64_t)this->regs[reg + 1]) << 32) + this->regs[reg];
}

inline iss_reg64_t Regfile::get_reg64(int reg)
{
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    this->scoreboard_reg_check(reg);
    this->scoreboard_reg_check(reg + 1);
#endif

    return this->get_reg64_untimed(reg);
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
#ifndef CONFIG_GVSOC_ISS_SNITCH
#ifdef ISS_SINGLE_REGFILE
    this->regs[reg] = value;
#else
    this->fregs[reg] = value;
#endif
#endif

#ifdef CONFIG_GVSOC_ISS_SNITCH
    // Conditionally choose operands from regfile or external streams.
    // Choose operands from streamer when SSR is enabled and fp register index is 0/1/2.
    if (!this->iss.ssr.ssr_enable)
    {
    #ifdef ISS_SINGLE_REGFILE
        this->regs[reg] = value;
    #else
        this->fregs[reg] = value;
    #endif
    }
    else
    {
    #ifdef ISS_SINGLE_REGFILE
        this->regs[reg] = value;
    #else
        if (reg >= 0 & reg <= 2)
        {
            this->iss.ssr.dm_write(value, reg);
        }
        else
        {
            this->fregs[reg] = value;
        }
    #endif  
    }
#endif
}

inline iss_freg_t Regfile::get_freg_untimed(int reg)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    if (!this->iss.ssr.ssr_enable)
    {
        return this->fregs[reg];
    }
    else
    {
        if (reg >= 0 & reg <= 2)
        {
            if (reg == 0)
            {
                return this->iss.ssr.ssr_fregs[0];
            }
            if (reg == 1)
            {
                return this->iss.ssr.ssr_fregs[1];
            }
            if (reg == 2)
            {
                return this->iss.ssr.ssr_fregs[2];
            }
        }

        return this->fregs[reg];
    }
#else
#ifdef ISS_SINGLE_REGFILE
    return this->regs[reg];
#else
    return this->fregs[reg];
#endif
#endif
}

inline iss_freg_t Regfile::get_freg(int reg)
{
#ifndef CONFIG_GVSOC_ISS_SNITCH
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    this->scoreboard_freg_check(reg);
#endif

    return this->get_freg_untimed(reg);
#endif

#ifdef CONFIG_GVSOC_ISS_SNITCH
    // Conditionally choose operands from regfile or external streams.
    if (!this->iss.ssr.ssr_enable)
    {
    #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
        this->scoreboard_freg_check(reg);
    #endif

        return this->get_freg_untimed(reg);
    }
    else
    {
    // There's no need to check scoreboard if SSR is enabled.
    #ifdef ISS_SINGLE_REGFILE
        return this->regs[reg];
    #else
        if (reg >= 0 & reg <= 2)
        {
            if (reg == 0 & this->iss.ssr.dm0.dm_read)
            {
                return this->iss.ssr.ssr_fregs[0];
            }
            if (reg == 1 & this->iss.ssr.dm1.dm_read)
            {
                return this->iss.ssr.ssr_fregs[1];
            }
            if (reg == 2 & this->iss.ssr.dm2.dm_read)
            {
                return this->iss.ssr.ssr_fregs[2];
            }
            // Only need to fetch from memory once at the first time
            // and the memory access from/to the same address would be finished by fetching from ssr_fregs[i].
            return this->iss.ssr.dm_read(reg);
        }
        else
        {
            // There's no need to check scoreboard if SSR is enabled.
        #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
            this->scoreboard_freg_check(reg);
        #endif

            return this->get_freg_untimed(reg);
        }
    #endif
    }
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
inline void Regfile::scoreboard_reg_set_timestamp(int reg, int64_t latency, int stall_reason)
{
    int64_t timestamp = this->iss.exec.get_cycles() + latency;
    this->scoreboard_reg_timestamp[reg] = timestamp;
    this->scoreboard_reg_stall_reason[reg] = stall_reason;
}

inline void Regfile::scoreboard_freg_set_timestamp(int reg, int64_t latency, int stall_reason)
{
    int64_t timestamp = this->iss.exec.get_cycles() + latency;
#ifdef ISS_SINGLE_REGFILE
    this->scoreboard_reg_timestamp[reg] = timestamp;
    this->scoreboard_reg_stall_reason[reg] = stall_reason;
#else
    this->scoreboard_freg_timestamp[reg] = timestamp;
    this->scoreboard_freg_stall_reason[reg] = stall_reason;
#endif
}

inline void Regfile::scoreboard_freg_invalidate(int reg)
{
#ifdef ISS_SINGLE_REGFILE
    this->scoreboard_reg_timestamp[reg] = -1;
#else
    this->scoreboard_freg_timestamp[reg] = -1;
#endif
}
#endif