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

#include "regfile.hpp"

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
    if (reg != 0)
        this->regs[reg] = value;
}

inline iss_reg_t Regfile::get_reg(int reg)
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
