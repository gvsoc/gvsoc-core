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
#ifdef VP_MEMCHECK_ACTIVE
    // Re-arm memcheck: all registers uninitialized again. See regfile.cpp.
    void memcheck_reset();
#endif

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

    // Memcheck register shadow. Each register carries a per-bit validity mask
    // (uninitialized-value tracking, -1 means fully initialized) and a buffer ID
    // (pointer provenance, 0 means none). The validity masks follow the v1 ISS
    // semantics; the IDs are propagated so that a pointer keeps naming its
    // allocation through moves, pointer arithmetic and alignment masking, and get
    // dropped by operations that do not produce a pointer.
    // The checks (memcheck_reg and its callers) are runtime-gated, the shadow
    // updates are only compiled in debug builds (VP_MEMCHECK_ACTIVE).
    inline bool memcheck_reg(int reg);
    inline void memcheck_branch_reg(int reg);
    inline void memcheck_access_reg(int reg);
    inline void memcheck_fault();
    inline iss_reg_t memcheck_get(int reg);
    inline void memcheck_set(int reg, iss_reg_t value);
    inline bool memcheck_get_valid(int reg);
    inline void memcheck_set_valid(int reg, bool valid);
    inline uint32_t memcheck_get_id(int reg);
    inline void memcheck_set_id(int reg, uint32_t id);
    inline void memcheck_merge(int out_reg, int in_reg);
    inline void memcheck_merge2(int out_reg, int in_reg_0, int in_reg_1);
    inline void memcheck_merge3(int out_reg, int in_reg_0, int in_reg_1, int in_reg_2);
    inline void memcheck_merge64(int out_reg, int in_reg);
    inline void memcheck_copy(int out_reg, int in_reg);
    inline void memcheck_bitwise_and(int out_reg, int in_reg_0, int in_reg_1);
    inline void memcheck_shift_left(int out_reg, int in_reg, int shift);
    inline void memcheck_shift_right(int out_reg, int in_reg, int shift);
    inline void memcheck_shift_right_signed(int out_reg, int in_reg, int shift);

#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
    // Stack checker: the runtime declares the stack the core is executing on
    // via semihosting (SEMIHOSTING_GV_STACK_SET). Every SP write is then
    // checked against the declared range, and the stack usage is dumped as a
    // real trace event, which the GUI displays as an analog signal.
    void stack_set(iss_reg_t base, iss_reg_t size);
    void stack_sp_update(iss_reg_t sp);
    void stack_fault_report(iss_reg_t sp);
#endif

    inline bool scoreboard_insn_check(iss_insn_t *insn);
    inline void scoreboard_insn_clear(iss_insn_t *insn);
    inline void scoreboard_insn_start(iss_insn_t *insn);
    inline void scoreboard_insn_end(iss_insn_t *insn);
    // True when no register is in flight (always true without a
    // scoreboard). Used by the DBT dispatch to enter translated code
    // only from a clean state.
    inline bool scoreboard_is_clean();

    // Host address of the raw register array (64-bit slots, x0 writes
    // land in the trailing dummy slot). Used by the DBT backend to
    // access registers directly from translated code.
    inline uint64_t *dbt_regs() { return this->regs; }

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    inline void sb_reg_invalid_set(int reg);
    inline void sb_reg_invalid_clear(int reg);
    inline void sb_reg_invalid_clear_mask(uint64_t mask);
    // True when register `reg` has an in-flight producer (e.g. a deferred
    // load) that has not yet written back. Lets a core with implicit
    // register operands (Zdinx pair reads, whose odd sibling is not in the
    // instruction's decoded scoreboard mask) re-check a register the generic
    // scoreboard_insn_check would miss.
    inline bool sb_reg_is_invalid(int reg) { return (this->sb_reg_invalid >> reg) & 1; }
    // Caller pushes an opaque per-register stall-reason tag at
    // invalidation time. The scoreboard stores the bytes verbatim and
    // hands them back via Events::event_scoreboard_stall when a
    // dependent insn stalls. The scoreboard never interprets the
    // value — only the per-core events class does. See IssStallReason
    // in event/event.hpp for the shared enumerators.
    inline void sb_set_reason(uint64_t mask, uint8_t reason);
#endif

private:
    // Combine the buffer IDs of a 2-input operation: an ID is kept only when it is
    // unambiguous (exactly one input carries one). Two equal IDs cancel out, which
    // covers the pointer-difference case.
    inline uint32_t memcheck_id_combine(int in_reg_0, int in_reg_1);

    Iss &iss;
    vp::Trace trace;
    uint64_t regs[ISS_NB_REGS+ISS_NB_FREGS+1];

#ifdef VP_MEMCHECK_ACTIVE
    // Per-bit validity mask of each register, -1 means fully initialized
    iss_reg_t regs_memcheck[ISS_NB_REGS+ISS_NB_FREGS+1];
    // Buffer provenance of each register, 0 means no associated buffer
    uint32_t regs_memcheck_id[ISS_NB_REGS+ISS_NB_FREGS+1];
    // Fault flagged by the ISA handlers, consumed once per instruction by
    // memcheck_fault()
    bool memcheck_reg_fault;
    int memcheck_reg_fault_id;
    const char *memcheck_reg_fault_kind;
    std::string memcheck_reg_fault_message;
#endif

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

#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
    // Bounds of the stack declared by the runtime, top is the first address
    // after the stack
    iss_reg_t stack_base;
    iss_reg_t stack_top;
    // True when a stack has been declared
    bool stack_enabled;
    // The declaration can precede the actual stack switch (context-switch
    // case, where SP still points to the previous thread stack). The checks
    // only arm once SP first lands inside the declared range.
    bool stack_active;
    vp::Trace stack_usage_event;
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
#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
    if (__builtin_expect(reg == 2, 0))
    {
        this->stack_sp_update((iss_reg_t)value);
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

inline bool Regfile::scoreboard_is_clean()
{
    return this->sb_reg_invalid == 0;
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

inline bool Regfile::scoreboard_is_clean()
{
    return true;
}

#endif

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
    // Setting the mask directly means the value does not derive from a pointer
    // (constants, PC-derived values), so the provenance is dropped
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck[reg] = value;
    this->regs_memcheck_id[reg] = 0;
#endif
}

inline bool Regfile::memcheck_get_valid(int reg)
{
    return this->memcheck_get(reg) == (iss_reg_t)-1;
}

inline void Regfile::memcheck_set_valid(int reg, bool valid)
{
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck[reg] = valid ? (iss_reg_t)-1 : 0;
    this->regs_memcheck_id[reg] = 0;
#endif
}

inline uint32_t Regfile::memcheck_get_id(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    return this->regs_memcheck_id[reg];
#else
    return 0;
#endif
}

inline void Regfile::memcheck_set_id(int reg, uint32_t id)
{
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck_id[reg] = id;
#endif
}

inline uint32_t Regfile::memcheck_id_combine(int in_reg_0, int in_reg_1)
{
#ifdef VP_MEMCHECK_ACTIVE
    uint32_t id_0 = this->regs_memcheck_id[in_reg_0];
    uint32_t id_1 = this->regs_memcheck_id[in_reg_1];

    if (id_1 == 0)
    {
        return id_0;
    }
    if (id_0 == 0)
    {
        return id_1;
    }
    // Two pointers to the same buffer combine into a non-pointer (typically a
    // pointer difference). Two different buffers is undefined behavior in C,
    // arbitrarily keep the first one.
    return id_0 == id_1 ? 0 : id_0;
#else
    return 0;
#endif
}

inline void Regfile::memcheck_merge(int out_reg, int in_reg)
{
    // Single-input propagation (mv, addi, ...): this is pointer arithmetic when the
    // input is a pointer, so the provenance follows. As soon as one bit is invalid,
    // the whole output is considered invalid.
#ifdef VP_MEMCHECK_ACTIVE
    uint32_t id = this->regs_memcheck_id[in_reg];
    this->memcheck_set_valid(out_reg, this->memcheck_get_valid(in_reg));
    this->regs_memcheck_id[out_reg] = id;
#endif
}

inline void Regfile::memcheck_merge2(int out_reg, int in_reg_0, int in_reg_1)
{
    // 2-input propagation (add, sub, ...): the output is valid only if both inputs
    // are fully valid, and keeps the provenance when it is unambiguous
#ifdef VP_MEMCHECK_ACTIVE
    uint32_t id = this->memcheck_id_combine(in_reg_0, in_reg_1);
    this->memcheck_set_valid(out_reg,
        this->memcheck_get_valid(in_reg_0) && this->memcheck_get_valid(in_reg_1));
    this->regs_memcheck_id[out_reg] = id;
#endif
}

inline void Regfile::memcheck_merge3(int out_reg, int in_reg_0, int in_reg_1, int in_reg_2)
{
    // 3-input propagation (MACs, ...): these do not produce pointers, only the
    // validity is combined
#ifdef VP_MEMCHECK_ACTIVE
    this->memcheck_set_valid(out_reg,
        this->memcheck_get_valid(in_reg_0) && this->memcheck_get_valid(in_reg_1) &&
        this->memcheck_get_valid(in_reg_2));
#endif
}

inline void Regfile::memcheck_merge64(int out_reg, int in_reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    bool valid = this->memcheck_get_valid(in_reg) && this->memcheck_get_valid(in_reg + 1);
    this->memcheck_set_valid(out_reg, valid);
    this->memcheck_set_valid(out_reg + 1, valid);
#endif
}

inline void Regfile::memcheck_copy(int out_reg, int in_reg)
{
    // Bit-exact copy of the shadow (immediate logical ops): alignment masks like
    // "p & ~0xf" must keep the provenance
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck[out_reg] = this->regs_memcheck[in_reg];
    this->regs_memcheck_id[out_reg] = this->regs_memcheck_id[in_reg];
#endif
}

inline void Regfile::memcheck_bitwise_and(int out_reg, int in_reg_0, int in_reg_1)
{
    // Register logical ops: masking a pointer with a register mask keeps the
    // provenance when unambiguous
#ifdef VP_MEMCHECK_ACTIVE
    this->regs_memcheck[out_reg] =
        this->regs_memcheck[in_reg_0] & this->regs_memcheck[in_reg_1];
    this->regs_memcheck_id[out_reg] = this->memcheck_id_combine(in_reg_0, in_reg_1);
#endif
}

inline void Regfile::memcheck_shift_left(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted and introduce
    // new valid ones from the right. A shifted pointer is not a pointer anymore.
#ifdef VP_MEMCHECK_ACTIVE
    iss_reg_t in_reg_valid = this->regs_memcheck[in_reg];
    in_reg_valid = (in_reg_valid << shift) | (((iss_reg_t)1 << shift) - 1);
    this->regs_memcheck[out_reg] = in_reg_valid;
    this->regs_memcheck_id[out_reg] = 0;
#endif
}

inline void Regfile::memcheck_shift_right(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted and introduce
    // new valid ones from the left
#ifdef VP_MEMCHECK_ACTIVE
    iss_reg_t in_reg_valid = this->regs_memcheck[in_reg];
    in_reg_valid = (in_reg_valid >> shift) |
        ((((iss_reg_t)1 << shift) - 1) << (sizeof(iss_reg_t) * 8 - shift));
    this->regs_memcheck[out_reg] = in_reg_valid;
    this->regs_memcheck_id[out_reg] = 0;
#endif
}

inline void Regfile::memcheck_shift_right_signed(int out_reg, int in_reg, int shift)
{
    // When shifting, keep the valid bit for bits being shifted
#ifdef VP_MEMCHECK_ACTIVE
    iss_reg_t in_reg_valid = this->regs_memcheck[in_reg];
    iss_reg_t new_in_reg_valid = (in_reg_valid >> shift);

    // Then introduce new valid ones from the left only if sign bit is valid
    if ((in_reg_valid >> (sizeof(iss_reg_t) * 8 - 1)) & 1)
    {
        new_in_reg_valid |= ((((iss_reg_t)1 << shift) - 1) << (sizeof(iss_reg_t) * 8 - shift));
    }

    this->regs_memcheck[out_reg] = new_in_reg_valid;
    this->regs_memcheck_id[out_reg] = 0;
#endif
}
