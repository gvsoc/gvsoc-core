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

static inline iss_reg_t lwu_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lwu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_perf<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t ld_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load_signed<int64_t>(insn, REG_GET(0) + SIM_GET(0), 8, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t ld_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_signed_perf<int64_t>(insn, REG_GET(0) + SIM_GET(0), 8, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sd_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.store<uint64_t>(insn, REG_GET(0) + SIM_GET(0), 8, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sd_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_perf<uint64_t>(insn, REG_GET(0) + SIM_GET(0), 8, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t addiw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_ADDW, REG_GET(0), SIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slliw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SLLW, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srliw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRLW, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sraiw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRAW, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t addw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_ADDW, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t subw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SUBW, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sllw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SLLW, REG_GET(0), REG_GET(1) & ((1 << 5) - 1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srlw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRLW, REG_GET(0), REG_GET(1) & ((1 << 5) - 1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sraw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRAW, REG_GET(0), REG_GET(1) & ((1 << 5) - 1)));
    return iss_insn_next(iss, insn, pc);
}
