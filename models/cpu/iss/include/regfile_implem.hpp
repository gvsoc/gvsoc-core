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
static iss_reg_t null_check_reg = -1;

inline iss_reg_t *Regfile::reg_ref(int reg)
{
    if (reg == 0)
        return &null_reg;
    else
        return &this->regs[reg];
}

inline iss_reg_t *Regfile::reg_check_ref(int reg)
{
    if (reg == 0)
        return &null_check_reg;
    else
        return &this->regs_check[reg];
}

inline iss_reg_t *Regfile::reg_store_ref(int reg)
{
    return &this->regs[reg];
}

inline iss_reg_t *Regfile::reg_check_store_ref(int reg)
{
    return &this->regs_check[reg];
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

inline iss_reg64_t Regfile::get_check_reg64(int reg)
{
    if (reg == 0)
        return 0;
    else
        return (((uint64_t)this->regs_check[reg + 1]) << 32) + this->regs_check[reg];
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
#ifdef ISS_SINGLE_REGFILE
    this->regs[reg] = value;
#else
    this->fregs[reg] = value;
#endif
}

inline iss_freg_t Regfile::get_freg_untimed(int reg)
{
#ifdef ISS_SINGLE_REGFILE
    return this->regs[reg];
#else
    return this->fregs[reg];
#endif
}

inline iss_freg_t Regfile::get_freg(int reg)
{
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    this->scoreboard_freg_check(reg);
#endif

    return this->get_freg_untimed(reg);
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

inline bool Regfile::check_reg(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE

    if (this->regs_check[reg] != -1)
    {
        if (this->iss.top.traces.get_trace_engine()->is_memcheck_enabled())
        {
            return true;
        }
    }
#endif

    return false;
}

inline void Regfile::check_branch_reg(int reg)
{
    if (this->check_reg(reg))
    {
        this->check_reg_fault = true;
        this->check_reg_fault_message = "Conditional jump depends on uninitialised register";
    }
}

inline void Regfile::check_load_reg(int reg)
{
    if (this->check_reg(reg))
    {
        this->check_reg_fault = true;
        this->check_reg_fault_message = "Load address depends on uninitialised register";
    }
}

inline void Regfile::check_fault()
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->check_reg_fault)
    {
        // When GDB is connected, throw a message without exiting and notify gdb
        // since this will do a break, so that user can continue
        if (this->iss.gdbserver.is_enabled())
        {
            this->trace.force_warning_no_error("%s (reg: %d)\n", this->check_reg_fault_message.c_str(), this->check_reg_fault_id);

            this->check_reg_fault = false;

            this->iss.exec.stalled_inc();
            this->iss.exec.halted.set(true);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, vp::Gdbserver_engine::SIGNAL_BUS);
        }
        else
        {
            this->trace.force_warning("%s (reg: %d)\n", this->check_reg_fault_message.c_str(), this->check_reg_fault_id);
        }
    }
#endif
}

inline iss_reg_t Regfile::check_get(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    return this->regs_check[reg];
#else
    return 0;
#endif
}

inline void Regfile::check_set(int reg, iss_reg_t value)
{
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_check[reg] = value;
#endif
}