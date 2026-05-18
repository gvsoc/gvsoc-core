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

#include <vp/signal.hpp>
#include "cpu/iss/include/types.hpp"
#include "types.hpp"

class IssWrapper;

class Regfile
{
public:

    Regfile(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    inline void set_reg(int reg, uint64_t value);
    inline void set_reg_pair(int reg, uint64_t value);
    inline uint64_t get_reg(int reg);
    inline uint64_t get_reg_untimed(int reg);
    inline uint64_t get_reg_pair(int reg);
    inline uint64_t get_reg_pair_untimed(int reg);

    inline void set_freg(int reg, uint64_t value);
    inline uint64_t get_freg(int reg);
    inline uint64_t get_freg_untimed(int reg);

    inline int get_reg_gid(int reg);
    inline int get_reg_lid(int reg);
    inline bool is_freg(int reg);

    // These methods are just kepts for compatibility with ISA files. They will be replaced
    // by the new memcheck implementation at some point.
    inline bool memcheck_reg(int reg) { return false; }
    inline void memcheck_branch_reg(int reg) {}
    inline void memcheck_propagate_1(int out_reg, int in_reg) {}
    inline void memcheck_access_reg(int reg) {}
    inline void memcheck_fault() {}
    inline iss_reg_t memcheck_get(int reg) { return 0; }
    inline void memcheck_set(int reg, iss_reg_t value) {}
    inline bool memcheck_get_valid(int reg) { return true; }
    inline void memcheck_set_valid(int reg, bool valid) {}
    inline void memcheck_merge(int out_reg, int in_reg) {}
    inline void memcheck_merge64(int out_reg, int in_reg) {}
    inline void memcheck_copy(int out_reg, int in_reg) {}
    inline void memcheck_bitwise_and(int out_reg, int in_reg_0, int in_reg_1) {}
    inline void memcheck_shift_left(int out_reg, int in_reg, int shift) {}
    inline void memcheck_shift_right(int out_reg, int in_reg, int shift) {}
    inline void memcheck_shift_right_signed(int out_reg, int in_reg, int shift) {}

    inline bool scoreboard_insn_check(iss_insn_t *insn);
    inline void scoreboard_insn_clear(iss_insn_t *insn);
    inline void scoreboard_insn_start(iss_insn_t *insn);
    inline void scoreboard_insn_end(iss_insn_t *insn);

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    inline void sb_reg_invalid_set(int reg);
    inline void sb_reg_invalid_clear(int reg);
    inline void sb_reg_invalid_clear_mask(uint64_t mask);
    // Caller pushes an opaque per-register stall-reason tag at
    // invalidation time. The scoreboard stores the bytes verbatim and
    // hands them back via Events::event_scoreboard_stall when a
    // dependent insn stalls. The scoreboard never interprets the
    // value — only the per-core events class does. See IssStallReason
    // in event/event.hpp for the shared enumerators.
    inline void sb_set_reason(uint64_t mask, uint8_t reason);
#endif

private:
    Iss &iss;
    vp::Trace trace;
    uint64_t regs[ISS_NB_REGS+ISS_NB_FREGS+1];

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    uint64_t sb_reg_invalid;
    // 64 entries (one per bit of sb_reg_invalid / sb_out_reg_mask) so
    // ctzll-based indexing is always in range — sb_out_reg_mask gets
    // sign-extended bits set in some decode paths (`1 << reg` with
    // reg=31 widens to 0xFFFFFFFF80000000 when OR-assigned into a
    // uint64_t), so the iteration can walk bit positions up to 63.
    uint8_t  sb_reason[64];
    // Bitmap of which sb_reason[bit] entries are currently non-zero —
    // mirrors sb_set_reason calls. The release path skips the per-bit
    // clear loop when the released registers were never tagged, so a
    // typical retire (whose destination wasn't tagged by a producer)
    // costs a single AND + branch instead of walking the mask bits.
    uint64_t sb_reason_set_mask;
    // Track the (PC, cycle) of the last scoreboard stall so we only
    // fire event_scoreboard_stall on the *first* cycle of each hazard —
    // matches RTL's id_valid_q gate that nulls PCCR_in[2..3] on the
    // retry cycles of a held insn (riscv_cs_registers.sv:1099-1100).
    iss_addr_t sb_last_stall_pc;
    int64_t    sb_last_stall_cycle;
#endif

#if defined(CONFIG_GVSOC_EVENT_ACTIVE)
    std::vector<vp::Signal<iss_reg_t>> reg_signals;
#endif
};

inline bool Regfile::is_freg(int reg)
{
#if defined(CONFIG_GVSOC_ISS_ZFINX)
    return false;
#else
    return reg >= ISS_NB_REGS;
#endif
}

inline int Regfile::get_reg_gid(int reg)
{
#if defined(CONFIG_GVSOC_ISS_ZFINX)
    return reg;
#else
    return reg + ISS_NB_REGS;
#endif
}

inline int Regfile::get_reg_lid(int reg)
{
#if defined(CONFIG_GVSOC_ISS_ZFINX)
    return reg;
#else
    return reg - ISS_NB_REGS;
#endif
}

inline void Regfile::set_reg(int reg, uint64_t value)
{
    // Since the register file is always 64bits to simplify, be careful to cast to real width
    // when writing, otherwise some instructions could read back a larger value
    if (!this->is_freg(reg)) {
        value = (iss_reg_t)value;
    }
    this->regs[reg] = value;
#if defined(CONFIG_GVSOC_EVENT_ACTIVE)
    if (reg < ISS_DUMMY_REG)
    {
        this->reg_signals[reg] = value;
    }
#endif
}

inline void Regfile::set_reg_pair(int reg, uint64_t value)
{
    if (reg == 0)
        return;

    this->set_reg(reg, value & 0xFFFFFFFF);
    this->set_reg(reg | 1, (value >> 32) & 0xFFFFFFFF);
}

inline uint64_t Regfile::get_reg(int reg)
{
    return this->regs[reg];
}

inline uint64_t Regfile::get_reg_untimed(int reg)
{
    return this->regs[reg];
}

inline uint64_t Regfile::get_reg_pair(int reg)
{
    return this->get_reg_pair_untimed(reg);
}

inline uint64_t Regfile::get_reg_pair_untimed(int reg)
{
    if (reg == 0)
        return 0;
    else
        return (((uint64_t)this->get_reg(reg | 1)) << 32) + this->get_reg(reg);
}

inline uint64_t Regfile::get_freg_untimed(int reg)
{
#if defined(CONFIG_GVSOC_ISS_ZDINX)
    return this->get_reg_pair_untimed(reg);
#else
    return this->get_reg(reg);
#endif
}

inline uint64_t Regfile::get_freg(int reg)
{
    return this->get_freg_untimed(reg);
}

inline void Regfile::set_freg(int reg, uint64_t value)
{
    this->set_reg(reg, value);
}

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
inline void Regfile::sb_reg_invalid_set(int reg)
{
    this->sb_reg_invalid |= 1 << reg;
}

inline void Regfile::sb_reg_invalid_clear(int reg)
{
    this->sb_reg_invalid &= ~(1 << reg);
}

inline void Regfile::sb_reg_invalid_clear_mask(uint64_t mask)
{
    this->sb_reg_invalid &= ~mask;
    // Fast skip when the released registers were never tagged with a
    // non-zero stall reason — the common case for non-load retires.
    uint64_t m = mask & this->sb_reason_set_mask;
    if (m == 0) return;
    this->sb_reason_set_mask &= ~m;
    while (m)
    {
        this->sb_reason[__builtin_ctzll(m)] = 0;
        m &= m - 1;
    }
}

inline void Regfile::sb_set_reason(uint64_t mask, uint8_t reason)
{
    this->sb_reason_set_mask |= mask;
    while (mask)
    {
        this->sb_reason[__builtin_ctzll(mask)] = reason;
        mask &= mask - 1;
    }
}

// scoreboard_insn_check's body is in regfile_implem.hpp because it
// references iss.timing.event_scoreboard_stall, which needs Iss to be
// complete. That implem header is included after iss.hpp via the ISA
// implem-include list (see riscv.py: add_implem_include).

inline void Regfile::scoreboard_insn_clear(iss_insn_t *insn)
{
    this->sb_reg_invalid_clear_mask(insn->sb_out_reg_mask);
}

inline void Regfile::scoreboard_insn_start(iss_insn_t *insn)
{
    this->sb_reg_invalid |= insn->sb_out_reg_mask;
}

inline void Regfile::scoreboard_insn_end(iss_insn_t *insn)
{
    this->sb_reg_invalid_clear_mask(insn->sb_out_reg_mask);
}
#else
inline bool Regfile::scoreboard_insn_check(iss_insn_t *insn)
{
    return false;
}

inline void Regfile::scoreboard_insn_start(iss_insn_t *insn)
{
}

inline void Regfile::scoreboard_insn_end(iss_insn_t *insn)
{
}

inline void Regfile::scoreboard_insn_clear(iss_insn_t *insn)
{
}

#endif
