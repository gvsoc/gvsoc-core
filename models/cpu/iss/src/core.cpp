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
    this->reset_value = false;

    this->iss.top.traces.new_trace("core", &this->trace, vp::DEBUG);

    // Initialize the mstatus write mask so that WPRI fields are preserved
#if ISS_REG_WIDTH == 64
    this->mstatus_write_mask = 0x8000003F007FFFEA;
#else
    this->mstatus_write_mask = 0x807FFFEA;
#endif

    // For now preserve floating point state to keep it dirty
    // this->mstatus_write_mask &= ~(0x3ULL << 13);
    this->iss.csr.mstatus.reset_val |= 0x3ULL << 13;
    // Remove vector state since we do not support yet vector extension
    this->mstatus_write_mask &= ~(0x3ULL << 9);
    // Remove user state
    this->mstatus_write_mask &= ~(0x3ULL << 15);

    // Always put state dirty to 1 since floats state is always dirty
    this->mstatus_write_mask &= ~(0x1ULL << 63);
    this->iss.csr.mstatus.reset_val |= 0x1ULL << 63;
#if ISS_REG_WIDTH == 64
    // Remove sxl and uxl since they are not supported
    this->mstatus_write_mask &= ~(0x3ULL << 32);
    this->iss.csr.mstatus.reset_val |= 2ULL << 32;
    this->mstatus_write_mask &= ~(0x3ULL << 34);
    this->iss.csr.mstatus.reset_val |= 2ULL << 34;
#endif

#if ISS_REG_WIDTH == 64
    this->sstatus_write_mask = this->mstatus_write_mask & 0x80000003000DE762;
#else
    this->sstatus_write_mask = this->mstatus_write_mask & 0x800DE762;
#endif

    this->iss.csr.mstatus.register_callback(std::bind(&Core::mstatus_update, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.sstatus.register_callback(std::bind(&Core::sstatus_update, this, std::placeholders::_1, std::placeholders::_2));
}

void Core::reset(bool active)
{
    this->mode_set(PRIV_M);

    // Remember the current reset value so that other aspects willing to stall the core
    // can flag it if the core is under reset
    this->reset_value = active;

    // Everytime the core is reset, the stall counter is automatically set to 0, so
    // we need to set it again to stall the core until the reset is deasserted
    if (active)
    {
        this->iss.exec.stalled_inc();
    }
    else
    {
        // In case gdb got connected while we were under reset, we need to stall the core
        // before releasing the reset
        if (this->iss.exec.halted.get())
        {
            this->iss.exec.stalled_inc();
        }

        this->iss.exec.stalled_dec();
    }
}


iss_reg_t Core::mret_handle()
{
    this->iss.exec.switch_to_full_mode();

    this->mode_set(this->iss.csr.mstatus.mpp);
#ifdef CONFIG_GVSOC_ISS_USER_MODE
    this->iss.csr.mstatus.mpp = PRIV_U;
#else
    this->iss.csr.mstatus.mpp = PRIV_M;
#endif
    this->iss.irq.irq_enable.set(this->iss.csr.mstatus.mpie);
    this->iss.csr.mstatus.mie = this->iss.csr.mstatus.mpie;
    this->iss.csr.mstatus.mpie = 1;
    this->iss.csr.mcause.value = 0;

    return this->iss.csr.mepc.value;
}

iss_reg_t Core::sret_handle()
{
    if (this->mode_get() == PRIV_S && this->iss.csr.mstatus.tsr)
    {
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_ILLEGAL);
        return 0;
    }

    this->iss.exec.switch_to_full_mode();

    this->mode_set(this->iss.csr.mstatus.spp);
    this->iss.csr.mstatus.spp = PRIV_U;
    this->iss.irq.irq_enable.set(this->iss.csr.mstatus.spie);
    this->iss.csr.mstatus.sie = this->iss.csr.mstatus.spie;
    this->iss.csr.mstatus.spie = 1;
    this->iss.csr.scause.value = 0;

    return this->iss.csr.sepc.value;
}

iss_reg_t Core::dret_handle()
{
    this->iss.exec.switch_to_full_mode();
    this->iss.irq.irq_enable.set(this->iss.irq.debug_saved_irq_enable);
    this->iss.exec.debug_mode = 0;

    return this->iss.csr.depc;
}


bool Core::mstatus_update(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
    #ifdef CONFIG_GVSOC_ISS_RI5KY
        this->iss.csr.mstatus.value = value;

    #ifdef CONFIG_GVSOC_ISS_SUPERVISOR_MODE
        this->iss.csr.mstatus.value &= 0x21899;
    #else
        this->iss.csr.mstatus.value = (this->iss.csr.mstatus.value & 0x88) | 0x1800;
    #endif
    #else
        this->iss.csr.mstatus.value = (this->iss.csr.mstatus.value & ~this->mstatus_write_mask) |
            (value & this->mstatus_write_mask);
    #endif

        this->iss.timing.stall_insn_dependency_account(4);
        this->iss.irq.global_enable(this->iss.csr.mstatus.mie);

    }
    else
    {
        value = this->iss.csr.mstatus.value;
    }

    return false;
}


bool Core::sstatus_update(bool is_write, iss_reg_t &value)
{
    this->iss.timing.stall_insn_dependency_account(4);

    if (is_write)
    {
        this->iss.csr.mstatus.value = (this->iss.csr.mstatus.value & ~this->sstatus_write_mask) |
            (value & this->sstatus_write_mask);
    }
    else
    {
        value = this->iss.csr.mstatus.value;
    }

    return false;
}

void Core::mode_set(int mode)
{
    this->mode = mode;
    iss_cache_vflush(&this->iss);
}