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

#define VF_OP(op, ff_name, fmt, fmt_size, exp, mant)                                                                             \
                                                                                                                        \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                                 \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    for (int i=0; i<8; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op2 = FREG_GET(1) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL2(lib_flexfloat_##ff_name, op1, op2, exp, mant) & ((1ULL << fmt_size) - 1)) << (i*fmt_size);   \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}                                                                                                                       \
                                                                                                                        \
static inline iss_reg_t vf##op##_r_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                               \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    iss_freg_t value = FREG_GET(1);                                                                                     \
    for (int i=0; i<8; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL2(lib_flexfloat_##ff_name, op1, value, exp, mant) & ((1ULL << fmt_size) - 1)) << (i*fmt_size); \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}

#define VF_OP3(op, ff_name, fmt, fmt_size, exp, mant)                                                                             \
                                                                                                                        \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                                 \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    for (int i=0; i<8; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op2 = FREG_GET(1) >> (i*fmt_size);                                                                   \
        iss_freg_t op3 = FREG_GET(2) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL4(lib_flexfloat_##ff_name, op1, op2, op3, exp, mant, 0) & ((1ULL << fmt_size) - 1)) << (i*fmt_size);   \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}                                                                                                                       \
                                                                                                                        \
static inline iss_reg_t vf##op##_r_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                               \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    iss_freg_t value = FREG_GET(1);                                                                                     \
    for (int i=0; i<8; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op3 = FREG_GET(2) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL4(lib_flexfloat_##ff_name, op1, value, op3, exp, mant, 0) & ((1ULL << fmt_size) - 1)) << (i*fmt_size); \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}

#define VF_CMP(op,fmt,fmt_size,exp,mant)                                                                                 \
                                                                                                                \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                         \
{                                                                                                               \
    iss_freg_t result = 0;                                                                                      \
    for (int i=0; i<8; i++)                                                                                     \
    {                                                                                                           \
        result |= LIB_FF_CALL2(lib_flexfloat_##op, FREG_GET(0) >> (i*fmt_size), FREG_GET(1) >> (i*fmt_size), exp, mant) << i; \
    }                                                                                                           \
                                                                                                                \
    REG_SET(0, result);                                                                                         \
                                                                                                                \
    return iss_insn_next(iss, insn, pc);                                                                        \
}                                                                                                               \
                                                                                                                \
static inline iss_reg_t vf##op##_r_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                       \
{                                                                                                               \
    iss_freg_t result = 0;                                                                                      \
    iss_freg_t value = FREG_GET(1);                                                                             \
                                                                                                                \
    for (int i=0; i<8; i++)                                                                                     \
    {                                                                                                           \
        result |= LIB_FF_CALL2(lib_flexfloat_##op, FREG_GET(0) >> (i*fmt_size), value, exp, mant) << i;                \
    }                                                                                                           \
                                                                                                                \
    REG_SET(0, result);                                                                                         \
                                                                                                                \
    return iss_insn_next(iss, insn, pc);                                                                        \
}

#define VF_CPK(pos, dst_format, index, width, dst_exp, dst_mant)                                                                                     \
                                                                                                        \
static inline iss_reg_t vfcpk##pos##_##dst_format##_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                 \
{                                                                                                       \
    iss_freg_t result = FREG_OUT_GET(0);                                                                \
                                                                                                        \
    result &= ~(((1ULL << (2*width)) - 1) << index);                                                                        \
                                                                                                        \
    result |= (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, dst_exp, dst_mant, 0) & ((1ULL << width) - 1)) << index;   \
    result |= (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(1), 8, 23, dst_exp, dst_mant, 0) & ((1ULL << width) - 1)) << (index + width);   \
                                                                                                        \
    FREG_SET(0, result);                                                                                \
                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                \
}

//
// with Xf16
//
static inline iss_reg_t vfadd_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23) & 0xffffffff) << 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) << 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23) & 0xffffffff) << 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) << 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsqrt_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, FREG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, FREG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, FREG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, FREG_GET(2) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 2, 16, 5, 10));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

VF_CMP(eq, h, 16, 5, 10)
VF_CMP(ne, h, 16, 5, 10)
VF_CMP(lt, h, 16, 5, 10)
VF_CMP(ge, h, 16, 5, 10)
VF_CMP(le, h, 16, 5, 10)
VF_CMP(gt, h, 16, 5, 10)

VF_CPK(a, h, 0,  16, 5, 10)
VF_CPK(b, h, 32, 16, 5, 10)

// Unless RV32D supported
static inline iss_reg_t vfmv_x_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xffff, 5, 10) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 5, 10) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

//
// with Xf16alt
//
static inline iss_reg_t vfadd_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfdiv_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsqrt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, FREG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, FREG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) >> 16, FREG_GET(1) >> 16, FREG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, FREG_GET(2) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, FREG_GET(2) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 2, 16, 8, 7));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) >> 16, FREG_GET(1) >> 16, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_r_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) & 0xffff, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) >> 16, FREG_GET(1) & 0xffff, 8, 7)) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpka_ah_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(1), 8, 23, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// Unless RV32D supported
static inline iss_reg_t vfmv_x_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_ah_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xffff, 8, 7) & 0xffff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 8, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 8, 7, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 8, 7, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 5, 10, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 5, 10, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

//
// with Xf8
//
VF_OP(add, add, b, 8, 5, 2)
VF_OP(sub, sub, b, 8, 5, 2)
VF_OP(mul, mul, b, 8, 5, 2)
VF_OP(div, div, b, 8, 5, 2)
VF_OP(min, min, b, 8, 5, 2)
VF_OP(max, max, b, 8, 5, 2)

static inline iss_reg_t vfsqrt_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

VF_OP3(mac, madd_round, b, 8, 5, 2)
VF_OP3(mre, nmsub_round, b, 8, 5, 2)
VF_OP(sgnj, sgnj, b, 8, 5, 2)
VF_OP(sgnjn, sgnjn, b, 8, 5, 2)
VF_OP(sgnjx, sgnjx, b, 8, 5, 2)

VF_CMP(eq, b, 8, 5, 2)
VF_CMP(ne, b, 8, 5, 2)
VF_CMP(lt, b, 8, 5, 2)
VF_CMP(ge, b, 8, 5, 2)
VF_CMP(le, b, 8, 5, 2)
VF_CMP(gt, b, 8, 5, 2)


// Unless RV32D supported
static inline iss_reg_t vfmv_x_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_b_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) & 0xff, 5, 2) & 0xff) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 8, 5, 2) & 0xff) << 8) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 16, 5, 2) & 0xff) << 16) |
                   ((LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0) >> 24, 5, 2) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfclass_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 4, 8, 5, 2));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xff, 5, 2, 0) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 8, 5, 2, 0) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 24, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// If F extension also supported
static inline iss_reg_t vfadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, FREG_GET(2) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 32, FREG_GET(1) >> 32, FREG_GET(2) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmac_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, FREG_GET(2) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, FREG_GET(2) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, FREG_GET(2) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0) >> 32, FREG_GET(1) >> 32, FREG_GET(2) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmre_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, FREG_GET(2) & 0xffffffff, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, FREG_GET(2) >> 32, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsum_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(1) & 0xffffffff, 
        (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 32, FREG_GET(0), 8, 23, 0) & 0xffffffff), 8, 23, 0) & 0xffffffff));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfeq_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfne_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ne, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vflt_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfle_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfgt_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_gt, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfge_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, ((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) |
                   (((LIB_FF_CALL2(lib_flexfloat_ge, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23)) & 0xffffffff) << 1));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 32, FREG_GET(1) >> 32, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_r_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) & 0xffffffff, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0) >> 32, FREG_GET(1) & 0xffffffff, 8, 23) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpka_s_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, 8, 23, 0) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(1), 8, 23, 8, 23, 0) & 0xffffffff) << 32));
#endif
    return iss_insn_next(iss, insn, pc);
}



VF_CPK(a, b, 0,  8, 5, 2)
VF_CPK(b, b, 16, 8, 5, 2)
VF_CPK(c, b, 32, 8, 5, 2)
VF_CPK(d, b, 48, 8, 5, 2)


// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xff, 5, 2, 5, 10, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 5, 10, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 5, 10, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 5, 10, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16alt extension also supported
static inline iss_reg_t vfcvt_ah_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xff, 5, 2, 8, 7, 0) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 8, 7, 0) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 8, 7, 5, 2, 0) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 8, 7, 5, 2, 0) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

#endif
