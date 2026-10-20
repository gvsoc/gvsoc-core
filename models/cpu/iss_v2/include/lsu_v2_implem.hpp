// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#pragma once

#include <cpu/iss_v2/include/utils.hpp>

inline uint64_t LsuV2::load_value(int reg, uint64_t data, int size, bool is_signed)
{
    if (is_signed)
    {
        data = iss_get_signed_value(data, size * 8);
    }

    // A load narrower than the FP registers NaN-boxes its value, as the F and D
    // extensions require
    if (this->iss.regfile.is_freg(reg) && size * 8 < CONFIG_GVSOC_ISS_FP_WIDTH)
    {
        data = iss_get_float_value(data, size * 8);
#if CONFIG_GVSOC_ISS_FP_WIDTH < 64
        data &= (1ULL << CONFIG_GVSOC_ISS_FP_WIDTH) - 1;
#endif
    }

    return data;
}

#ifdef CONFIG_GVSOC_ISS_MEMORY
inline bool LsuV2::mem_array_contains(iss_addr_t phys_addr, iss_addr_t size)
{
#ifdef VP_MEMCHECK_ACTIVE
    // The memcheck shadow is only maintained by the data port accesses
    if (this->iss.traces.get_trace_engine()->is_memcheck_enabled())
    {
        return false;
    }
#endif
    iss_addr_t offset = phys_addr - this->memory_start;
    return offset < this->memory_size && size <= this->memory_size - offset;
}
#endif

inline bool LsuV2::translation_stall()
{
    if (!this->iss.exec.has_exception)
    {
        this->iss.exec.insn_stall();
    }
    return true;
}

template<typename T>
inline bool LsuV2::load(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
#ifdef CONFIG_GVSOC_ISS_MEMORY
    // Misaligned accesses, which may trap or cross a page, take the generic path
    if (likely((addr & (size - 1)) == 0))
    {
        iss_addr_t phys_addr;
        bool use_mem_array;
        if (unlikely(this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array)))
        {
            return this->translation_stall();
        }

        if (likely(use_mem_array))
        {
            this->iss.regfile.set_reg(reg, *(T *)&this->mem_array[phys_addr - this->memory_start]);
            return false;
        }

        return this->data_req_phys(insn, phys_addr, size, vp::IoReqOpcode::READ, false, reg, 0);
    }
#endif
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
#ifdef CONFIG_GVSOC_ISS_MEMORY
    // Misaligned accesses, which may trap or cross a page, take the generic path
    if (likely((addr & (size - 1)) == 0))
    {
        iss_addr_t phys_addr;
        bool use_mem_array;
        if (unlikely(this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array)))
        {
            return this->translation_stall();
        }

        if (likely(use_mem_array))
        {
            this->iss.regfile.set_reg(reg, *(T *)&this->mem_array[phys_addr - this->memory_start]);
            return false;
        }

        return this->data_req_phys(insn, phys_addr, size, vp::IoReqOpcode::READ, true, reg, 0);
    }
#endif
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, true, reg);
}

template<typename T>
inline bool LsuV2::store(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
#ifdef CONFIG_GVSOC_ISS_MEMORY
    // Misaligned accesses, which may trap or cross a page, take the generic path
    if (likely((addr & (size - 1)) == 0))
    {
        iss_addr_t phys_addr;
        bool use_mem_array;
        if (unlikely(this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array)))
        {
            return this->translation_stall();
        }

        if (likely(use_mem_array))
        {
            *(T *)&this->mem_array[phys_addr - this->memory_start] = this->iss.regfile.get_reg(reg);
            return false;
        }

        return this->data_req_phys(insn, phys_addr, size, vp::IoReqOpcode::WRITE, false, reg, 0);
    }
#endif
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::WRITE, false, reg);
}

template<typename T>
inline bool LsuV2::load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.traces.declare_access(addr, size, false);
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    // event_load_account is fired in data_req_aligned (and
    // fire_misaligned_second for beat 1) so we count once per actual
    // bus transaction, matching RI5CY's mem_load_i (each beat of a
    // misaligned access counts separately).
    return this->load<T>(insn, addr, size, reg);
}

template<typename T>
inline bool LsuV2::load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.traces.declare_access(addr, size, false);
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    return this->load_signed<T>(insn, addr, size, reg);
}

template<typename T>
inline bool LsuV2::store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.traces.declare_access(addr, size, true);
    if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    {
        return true;
    }
    // event_store_account is fired in data_req_aligned (and
    // fire_misaligned_second for beat 1).
    return this->store<T>(insn, addr, size, reg);
}

inline LsuReqEntry *LsuV2::get_req_entry()
{
    if (this->req_entry_first == NULL)
    {
        return NULL;
    }

    this->nb_pending_accesses++;
    LsuReqEntry *req = this->req_entry_first;
    this->req_entry_first = req->next;
    return req;
}

template<typename T>
inline bool LsuV2::load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
#ifdef CONFIG_GVSOC_ISS_MEMORY
    // Misaligned accesses, which may trap or cross a page, take the generic path
    if (likely((addr & (size - 1)) == 0))
    {
        iss_addr_t phys_addr;
        bool use_mem_array;
        if (unlikely(this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array)))
        {
            return this->translation_stall();
        }

        if (likely(use_mem_array))
        {
            this->iss.regfile.set_reg(reg, iss_get_float_value(
                *(T *)&this->mem_array[phys_addr - this->memory_start], size * 8));
            return false;
        }

        return this->data_req_phys(insn, phys_addr, size, vp::IoReqOpcode::READ, false, reg, 0);
    }
#endif
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->store<T>(insn, addr, size, reg);
}

template<typename T>
inline bool LsuV2::load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->load_float<T>(insn, addr, size, reg);
}

template<typename T>
inline bool LsuV2::store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->store<T>(insn, addr, size, reg);
}

inline void LsuV2::free_req_entry(LsuReqEntry *entry)
{
    this->nb_pending_accesses--;
    entry->next = this->req_entry_first;
    this->req_entry_first = entry;
}
