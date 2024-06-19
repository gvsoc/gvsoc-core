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
static iss_reg_t null_memcheck_reg = -1;

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
        return &null_memcheck_reg;
    else
        return &this->regs_memcheck[reg];
}

inline iss_reg_t *Regfile::reg_store_ref(int reg)
{
    return &this->regs[reg];
}

inline iss_reg_t *Regfile::reg_check_store_ref(int reg)
{
    return &this->regs_memcheck[reg];
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

inline iss_reg64_t Regfile::get_memcheck_reg64(int reg)
{
    if (reg == 0)
        return 0;
    else
        return (((uint64_t)this->regs_memcheck[reg + 1]) << 32) + this->regs_memcheck[reg];
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
    int64_t timestamp = latency + this->iss.exec.get_cycles();
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

inline bool Regfile::memcheck_reg(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE

    if (this->regs_memcheck[reg] != -1)
    {
        if (this->iss.top.traces.get_trace_engine()->is_memcheck_enabled())
        {
            return true;
        }
    }
#endif

    return false;
}

inline void Regfile::memcheck_branch_reg(int reg)
{
    if (this->memcheck_reg(reg))
    {
        this->memcheck_reg_fault = true;
        this->memcheck_reg_fault_message = "Conditional jump depends on uninitialised register";
    }
}

inline void Regfile::memcheck_access_reg(int reg)
{
    if (this->memcheck_reg(reg))
    {
        this->memcheck_reg_fault = true;
        this->memcheck_reg_fault_message = "Access address depends on uninitialised register";
    }
}

inline void Regfile::memcheck_fault()
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_reg_fault)
    {
        // When GDB is connected, throw a message without exiting and notify gdb
        // since this will do a break, so that user can continue
        if (this->iss.gdbserver.is_enabled())
        {
            this->trace.force_warning_no_error("%s (reg: %d)\n", this->memcheck_reg_fault_message.c_str(), this->memcheck_reg_fault_id);

            this->memcheck_reg_fault = false;

            this->iss.exec.stalled_inc();
            this->iss.exec.halted.set(true);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, vp::Gdbserver_engine::SIGNAL_BUS);
        }
        else
        {
            this->trace.force_warning("%s (reg: %d)\n", this->memcheck_reg_fault_message.c_str(), this->memcheck_reg_fault_id);
        }
    }
#endif
}

inline iss_reg_t Regfile::memcheck_get(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    return this->regs_memcheck[reg];
#else
    return 0;
#endif
}

inline void Regfile::memcheck_set(int reg, iss_reg_t value)
{
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck[reg] = value;
#endif
}

inline bool Regfile::memcheck_get_valid(int reg)
{
    return this->memcheck_get(reg) == -1;
}

inline void Regfile::memcheck_set_valid(int reg, bool valid)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (valid)
    {
        this->regs_memcheck[reg] = -1;
    }
    else
    {
        this->regs_memcheck[reg] = 0;
    }
#endif
}

inline void Regfile::memcheck_merge(int out_reg, int in_reg)
{
    this->memcheck_set_valid(out_reg, this->memcheck_get_valid(in_reg));
}

inline void Regfile::memcheck_merge64(int out_reg, int in_reg)
{
    bool valid = this->memcheck_get_valid(in_reg) && this->memcheck_get_valid(in_reg + 1);
    this->memcheck_set_valid(out_reg, valid);
    this->memcheck_set_valid(out_reg + 1, valid);
}

inline void Regfile::memcheck_copy(int out_reg, int in_reg)
{
    this->memcheck_set(out_reg, this->memcheck_get(in_reg));
}

inline void Regfile::memcheck_bitwise_and(int out_reg, int in_reg_0, int in_reg_1)
{
    iss_reg_t in_reg_0_valid = this->memcheck_get(in_reg_0);
    iss_reg_t in_reg_1_valid = this->memcheck_get(in_reg_1);
    this->memcheck_set(out_reg, in_reg_0_valid & in_reg_1_valid);
}

inline void Regfile::memcheck_shift_left(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted and introduce
    // new valid ones from the right
    iss_reg_t in_reg_valid = this->memcheck_get(in_reg);
    in_reg_valid = (in_reg_valid << shift) | ((1 << shift) - 1);
    this->memcheck_set(out_reg, in_reg_valid);
}

inline void Regfile::memcheck_shift_right(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted and introduce
    // new valid ones from the left
    iss_reg_t in_reg_valid = this->memcheck_get(in_reg);
    in_reg_valid = (in_reg_valid >> shift) |
        (((1 << shift) - 1) << (sizeof(iss_reg_t) * 8 - shift));
    this->memcheck_set(out_reg, in_reg_valid);
}

inline void Regfile::memcheck_shift_right_signed(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted
    iss_reg_t in_reg_valid = this->memcheck_get(in_reg);
    iss_reg_t new_in_reg_valid = (in_reg_valid >> shift);

    // Then introduce new valid ones from the left only if sign bit is valid
    if ((in_reg_valid >> (sizeof(iss_reg_t) * 8 - 1)) & 1)
    {
        new_in_reg_valid |= (((1 << shift) - 1) << (sizeof(iss_reg_t) * 8 - shift));
    }

    this->memcheck_set(out_reg, new_in_reg_valid);
}
