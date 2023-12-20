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

#include "cpu/iss/include/iss_core.hpp"

template<typename T>
inline bool Lsu::load(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        this->iss.regfile.set_reg(reg, *(T *)&this->mem_array[phys_addr - this->memory_start]);

        return false;
    }
#endif

    // First set register to zero for zero-extension
    this->iss.regfile.set_reg(reg, 0);

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.reg_ref(reg), size, false, latency)) == 0)
    {
        this->iss.regfile.scoreboard_reg_set_timestamp(reg, this->iss.top.clock.get_cycles() + latency + 1);
        // We don't need to do anything as the target will write directly to the register
        // and we the zero extension is already managed by the initial
    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &Lsu::load_resume;
            this->stall_reg = reg;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

inline void Lsu::elw(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.regfile.set_reg(reg, 0);
    int64_t latency;
    if (!this->data_req(addr, (uint8_t *)this->iss.regfile.reg_ref(reg), size, false, latency))
    {
        this->iss.regfile.scoreboard_reg_set_timestamp(reg, this->iss.top.clock.get_cycles() + latency + 1);
        // We don't need to do anything as the target will write directly to the register
        // and we the zero extension is already managed by the initial value
    }
    else
    {
        this->stall_callback = &Lsu::elw_resume;
        this->stall_reg = reg;
    }
}

template<typename T>
inline bool Lsu::load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        this->iss.regfile.set_reg(reg, *(T *)&this->mem_array[phys_addr - this->memory_start]);

        return false;
    }
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.reg_ref(reg), size, false, latency)) == 0)
    {
        this->iss.regfile.set_reg(reg, iss_get_signed_value(this->iss.regfile.get_reg(reg), size * 8));
        this->iss.regfile.scoreboard_reg_set_timestamp(reg, this->iss.top.clock.get_cycles() + latency + 1);
    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &Lsu::load_signed_resume;
            this->stall_reg = reg;
            this->stall_size = size;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
inline bool Lsu::store(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        *(T *)&this->mem_array[phys_addr - this->memory_start] = this->iss.regfile.get_reg(reg);

        return false;
    }
#endif

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    // Since the input register is passed through its index and accessed through a pointer,
    // we need to take care of the scoreboard
    this->iss.regfile.scoreboard_reg_check(reg);
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.reg_store_ref(reg), size, true, latency)) == 0)
    {
        // For now we don't have to do anything as the register was written directly
        // by the request but we cold support sign-extended loads here;
    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &Lsu::store_resume;
            this->stall_reg = reg;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
inline bool Lsu::load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->load<T>(insn, addr, size, reg);
}

inline void Lsu::elw_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.timing.event_load_account(1);
    this->elw(insn, addr, size, reg);
}

template<typename T>
inline bool Lsu::load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->load_signed<T>(insn, addr, size, reg);
}

template<typename T>
inline bool Lsu::store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    {
        return true;
    }
    this->iss.timing.event_store_account(1);
    return this->store<T>(insn, addr, size, reg);
}

inline void Lsu::stack_access_check(int reg, iss_addr_t addr)
{
    // Could be optimized when decoding instruction by pointing to different instruction
    // handlers when register 2 is seen but the gain is small compared to the cost of the full
    // load.
    if (this->iss.csr.stack_conf && reg == 2)
    {
        if (addr < this->iss.csr.stack_start || addr >= this->iss.csr.stack_end)
        {
            this->trace.fatal("SP-based access outside stack (addr: 0x%x, stack_start: 0x%x, stack_end: 0x%x)\n",
                              addr, this->iss.csr.stack_start, this->iss.csr.stack_end);
        }
    }
}

template<typename T>
inline bool Lsu::load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        this->iss.regfile.set_freg(reg, iss_get_float_value(*(T *)&this->mem_array[phys_addr - this->memory_start], size * 8));

        return false;
    }
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.freg_ref(reg), size, false, latency)) == 0)
    {
        this->iss.regfile.set_freg(reg, iss_get_float_value(this->iss.regfile.get_freg(reg), size * 8));
        this->iss.regfile.scoreboard_freg_set_timestamp(reg, this->iss.top.clock.get_cycles() + latency + 1);
    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &Lsu::load_float_resume;
            this->stall_reg = reg;
            this->stall_size = size;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
inline bool Lsu::store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        *(T *)&this->mem_array[phys_addr - this->memory_start] = this->iss.regfile.get_freg(reg);

        return false;
    }
#endif

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    // Since the input register is passed through its index and accessed through a pointer,
    // we need to take care of the scoreboard
    this->iss.regfile.scoreboard_freg_check(reg);
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.freg_store_ref(reg), size, true, latency)) == 0)
    {
        // For now we don't have to do anything as the register was written directly
        // by the request but we cold support sign-extended loads here;
    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &Lsu::store_resume;
            this->stall_reg = reg;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
inline bool Lsu::load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->load_float<T>(insn, addr, size, reg);
}

template<typename T>
inline bool Lsu::store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    {
        return true;
    }
    this->iss.timing.event_store_account(1);
    return this->store_float<T>(insn, addr, size, reg);
}
