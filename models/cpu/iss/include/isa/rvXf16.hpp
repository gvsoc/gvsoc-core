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
 * Authors: Stefan Mach, ETH (smach@iis.ee.ethz.ch)
 *          Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#ifndef __CPU_ISS_RVXF16_HPP
#define __CPU_ISS_RVXF16_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/float.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t flh_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    iss->lsu.load_float<uint16_t>(insn, REG_GET(0) + SIM_GET(0), 2, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsh_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    iss->lsu.store_float<uint16_t>(insn, REG_GET(0) + SIM_GET(0), 2, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmadd_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, float_madd_16(iss, FREG_GET(0), FREG_GET(1), FREG_GET(2), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmsub_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, float_msub_16(iss, FREG_GET(0), FREG_GET(1), FREG_GET(2), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmsub_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, float_nmsub_16(iss, FREG_GET(0), FREG_GET(1), FREG_GET(2), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmadd_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, float_nmadd_16(iss, FREG_GET(0), FREG_GET(1), FREG_GET(2), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fadd_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0), FREG_GET(1), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsub_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0), FREG_GET(1), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmul_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0), FREG_GET(1), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fdiv_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0), FREG_GET(1), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsqrt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnj_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjn_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjx_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmin_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmax_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_w_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_w_ff_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_wu_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_wu_ff_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_x_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t feq_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    uint32_t a = FREG_GET(0);
    uint32_t b = FREG_GET(1);

    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fle_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0), FREG_GET(1), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fclass_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL1(lib_flexfloat_class, FREG_GET(0), 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_h_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_w_round, REG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_h_wu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_wu_round, REG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 5, 10, 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_h_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, 5, 10, UIM_GET(0)));

    return iss_insn_next(iss, insn, pc);
}

//
// RV64Xf16
//
static inline iss_reg_t fcvt_l_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_l_ff_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_lu_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_lu_ff_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_h_l_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_l_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_h_lu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_lu_round, FREG_GET(0), 5, 10, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

#endif
