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


#include "iss_core.hpp"

inline void Lsu::load(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_set_reg(&this->iss, reg, 0);
    if (!this->data_req(addr, (uint8_t *)iss_reg_ref(&this->iss, reg), size, false))
    {
        // We don't need to do anything as the target will write directly to the register
        // and we the zero extension is already managed by the initial value
    }
    else
    {
        this->iss.state.stall_callback = &Lsu::load_resume;
        this->iss.state.stall_reg = reg;
    }
}

inline void Lsu::elw(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_set_reg(&this->iss, reg, 0);
    if (!this->data_req(addr, (uint8_t *)iss_reg_ref(&this->iss, reg), size, false))
    {
        // We don't need to do anything as the target will write directly to the register
        // and we the zero extension is already managed by the initial value
    }
    else
    {
        this->iss.state.stall_callback = &Lsu::elw_resume;
        this->iss.state.stall_reg = reg;
    }
}

inline void Lsu::load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (!this->data_req(addr, (uint8_t *)iss_reg_ref(&this->iss, reg), size, false))
    {
        iss_set_reg(&this->iss, reg, iss_get_signed_value(iss_get_reg_untimed(&this->iss, reg), size * 8));
    }
    else
    {
        this->iss.state.stall_callback = &Lsu::load_signed_resume;
        this->iss.state.stall_reg = reg;
        this->iss.state.stall_size = size;
    }
}

inline void Lsu::store(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (!this->data_req(addr, (uint8_t *)iss_reg_store_ref(&this->iss, reg), size, true))
    {
        // For now we don't have to do anything as the register was written directly
        // by the request but we cold support sign-extended loads here;
    }
    else
    {
        this->iss.state.stall_callback = &Lsu::store_resume;
        this->iss.state.stall_reg = reg;
    }
}

inline void Lsu::load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.timing.event_load_account(1);
    this->load(insn, addr, size, reg);
}

inline void Lsu::elw_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.timing.event_load_account(1);
    this->elw(insn, addr, size, reg);
}

inline void Lsu::load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.timing.event_load_account(1);
    this->load_signed(insn, addr, size, reg);
}

inline void Lsu::store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    this->iss.timing.event_store_account(1);
    this->store(insn, addr, size, reg);
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
            this->iss.trace.fatal("SP-based access outside stack (addr: 0x%x, stack_start: 0x%x, stack_end: 0x%x)\n",
                addr, this->iss.csr.stack_start, this->iss.csr.stack_end);
        }
    }
}
