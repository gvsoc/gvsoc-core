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
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t mulw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_MULW, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t divw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_sim_t divider = get_signed_value(REG_GET(1), 32);
    iss_sim_t dividend = get_signed_value(REG_GET(0), 32);
    iss_sim_t result;
    if (divider == 0)
        result = -1;
    else if (divider == (1ULL << (ISS_REG_WIDTH - 1)) && dividend == -1)
        result = 0;
    else if (dividend == (1ULL << (ISS_REG_WIDTH - 1)) && divider == -1)
        result = (1ULL << (ISS_REG_WIDTH - 1));
    else
        result = dividend / divider;
    REG_SET(0, get_signed_value(result, 32));

    int64_t cycles;

    if (divider == 0)
    {
        cycles = 1;
    }
    else if (divider > 0)
    {
        cycles = __builtin_clz(divider) + 3;
    }
    else
    {
        cycles = __builtin_clz((~divider) + 1) + 2;
    }

    iss->timing.stall_insn_dependency_account(cycles);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t divuw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_uim_t divider = getField(REG_GET(1), 0, 32);
    iss_uim_t dividend = getField(REG_GET(0), 0, 32);
    iss_uim_t result;
    if (divider == 0)
        result = -1;
    else
        result = dividend / divider;
    REG_SET(0, get_signed_value(result, 32));

    iss->timing.stall_insn_dependency_account(__builtin_clz(divider) + 3);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t remw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    int32_t divider = get_signed_value(REG_GET(1), 32);
    int32_t dividend = get_signed_value(REG_GET(0), 32);
    int32_t result;
    if (divider == 0)
        result = dividend;
    else if (divider == (1 << 31) && dividend == -1)
        result = -1;
    else if (dividend == (1 << 31) && divider == -1)
        result = 0;
    else
        result = dividend % divider;
    REG_SET(0, get_signed_value(result, 32));

    int cycles;

    if (divider == 0)
    {
        cycles = 1;
    }
    else if (divider > 0)
    {
        cycles = __builtin_clz(divider) + 3;
    }
    else
    {
        cycles = __builtin_clz((~divider) + 1) + 2;
    }

    iss->timing.stall_insn_dependency_account(cycles);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t remuw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    uint32_t divider = getField(REG_GET(1), 0, 32);
    uint32_t dividend = getField(REG_GET(0), 0, 32);
    uint32_t result;
    if (divider == 0)
        result = dividend;
    else
        result = dividend % divider;

    REG_SET(0, get_signed_value(result, 32));

    iss->timing.stall_insn_dependency_account(__builtin_clz(divider) + 3);

    return iss_insn_next(iss, insn, pc);
}
