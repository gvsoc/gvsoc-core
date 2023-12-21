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

#ifndef __CPU_ISS_RV32XFVEC_HPP
#define __CPU_ISS_RV32XFVEC_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

//
// The definitions herein only work for FLEN=32 -> i.e. no D extension!!
//

//
// with Xf16
//
static inline iss_reg_t vfadd_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsqrt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, REG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, REG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, REG_GET(0), 2, 16, 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 5, 10)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpka_h_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0), 8, 23, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(1), 8, 23, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// Unless RV32D supported
static inline iss_reg_t vfmv_x_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

//
// with Xf16alt
//
static inline iss_reg_t vfadd_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsqrt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, REG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, REG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) & 0xffff, REG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, REG_GET(0), 2, 16, 8, 7));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xffff, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpka_ah_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0), 8, 23, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(1), 8, 23, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// Unless RV32D supported
static inline iss_reg_t vfmv_x_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_ah_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xffff, 8, 7, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) >> 16, 8, 7, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xffff, 5, 10, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) >> 16, 5, 10, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

//
// with Xf8
//
static inline iss_reg_t vfadd_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsqrt_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, REG_GET(2) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 8, REG_GET(1) >> 8, REG_GET(2) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 24, REG_GET(1) >> 24, REG_GET(2) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, REG_GET(2) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, REG_GET(2) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, REG_GET(2) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, REG_GET(2) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, REG_GET(2) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 8, REG_GET(1) >> 8, REG_GET(2) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) >> 16, REG_GET(2) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 24, REG_GET(1) >> 24, REG_GET(2) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) & 0xff, REG_GET(1) & 0xff, REG_GET(2) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 8, REG_GET(1) & 0xff, REG_GET(2) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 16, REG_GET(1) & 0xff, REG_GET(2) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, REG_GET(0) >> 24, REG_GET(1) & 0xff, REG_GET(2) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 8, REG_GET(1) >> 8, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) >> 16, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 24, REG_GET(1) >> 24, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_r_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) & 0xff, REG_GET(1) & 0xff, 5, 2)) & 0xff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 8, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 8) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 16, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 16) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, REG_GET(0) >> 24, REG_GET(1) & 0xff, 5, 2)) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// Unless RV32D supported
static inline iss_reg_t vfmv_x_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, REG_GET(0) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_b_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, REG_GET(0), 4, 8, 5, 2));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// If F extension also supported
static inline iss_reg_t vfcpka_b_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0), 8, 23, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(1), 8, 23, 5, 2, 0) & 0xff) << 8));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpkb_b_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0), 8, 23, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(1), 8, 23, 5, 2, 0) & 0xff) << 24) |
                   (REG_GET(2) & ~(0xffff << 16)));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xff, 5, 2, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xffff, 5, 10, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) >> 16, 5, 10, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16alt extension also supported
static inline iss_reg_t vfcvt_ah_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xff, 5, 2, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) & 0xffff, 8, 7, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, REG_GET(0) >> 16, 8, 7, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

#endif
