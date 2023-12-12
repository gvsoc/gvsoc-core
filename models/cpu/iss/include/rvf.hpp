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

#ifndef __CPU_ISS_RVF_HPP
#define __CPU_ISS_RVF_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t fp_offload_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    if (iss->snitch & !iss->fp_ss)
    {
        // if (iss->send_acc_req(insn, pc, false)) 
        // {
        //     return pc;
        // }
        insn->reg_addr = &iss->regfile.regs[0];
        iss->send_acc_req(insn, pc, false);
    }
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_float(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->csr.mstatus.fs == 0)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_float(insn, REG_GET(0) + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_nmadd_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmul_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fdiv_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsqrt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnj_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjn_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjx_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmin_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmax_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_w_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_w_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_wu_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_wu_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_x_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_s_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t feq_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fle_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fclass_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL1(lib_flexfloat_class, FREG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_w_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_wu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_wu_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

//
// RV64F
//
static inline iss_reg_t fcvt_l_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_l_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_lu_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_lu_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_l_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_l_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_lu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_lu_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

//
// COMPRESSED INSTRUCTIONS
//
static inline iss_reg_t c_fsw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return fsw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fswsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return fsw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fsdsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return fsd_exec(iss, insn, pc);
}

static inline iss_reg_t c_flw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return flw_exec(iss, insn, pc);
}

static inline iss_reg_t c_flwsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return flw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fldsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // perfAccountRvc(cpu);
    return fld_exec(iss, insn, pc);
}

#endif
