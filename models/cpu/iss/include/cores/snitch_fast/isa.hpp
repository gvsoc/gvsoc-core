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
#include "cpu/iss/include/isa_lib/float.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t flb_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_IN(0), base + SIM_GET(0));
    iss->lsu.load_float<uint8_t>(insn, base + SIM_GET(0), 1, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsb_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_OUT(0), base + SIM_GET(0));
    iss->lsu.store_float<uint8_t>(insn, base + SIM_GET(0), 1, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flh_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_IN(0), base + SIM_GET(0));
    iss->lsu.load<uint16_t>(insn, base + SIM_GET(0), 2, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsh_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_OUT(0), base + SIM_GET(0));
    iss->lsu.store<uint16_t>(insn, base + SIM_GET(0), 2, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flw_snitch_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    if (iss->lsu.load_float<uint32_t>(insn, base + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flw_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_IN(0), base + SIM_GET(0));
    if (iss->lsu.load_float_perf<uint32_t>(insn, base + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsw_snitch_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    if (iss->csr.mstatus.fs == 0)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    if (iss->lsu.store_float<uint32_t>(insn, base + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsw_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    if (iss->csr.mstatus.fs == 0)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    iss->lsu.stack_access_check(REG_OUT(0), base + SIM_GET(0));
    if (iss->lsu.store_float_perf<uint32_t>(insn, base + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fld_snitch_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.load_float<uint64_t>(insn, base + SIM_GET(0), 8, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fld_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_IN(0), base + SIM_GET(0));
    iss->lsu.load_float_perf<uint64_t>(insn, base + SIM_GET(0), 8, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsd_snitch_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.store_float<uint64_t>(insn, base + SIM_GET(0), 8, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsd_snitch_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t base = iss->sequencer.lsu_pop_base();
    iss->lsu.stack_access_check(REG_OUT(0), base + SIM_GET(0));
    iss->lsu.store_float_perf<uint64_t>(insn, base + SIM_GET(0), 8, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}
