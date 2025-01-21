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

//
// The definitions herein only work for FLEN=32 -> i.e. no D extension!!
//

#define VF_OP(op, ff_name, fmt, fmt_size, exp, mant)                                                                             \
                                                                                                                        \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                                 \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL2(lib_flexfloat_##ff_name, op1, value, exp, mant) & ((1ULL << fmt_size) - 1)) << (i*fmt_size); \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}

#define VF_OP_ROUND(op, ff_name, fmt, fmt_size, exp, mant)                                                                             \
                                                                                                                        \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                                 \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op2 = FREG_GET(1) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL3(lib_flexfloat_##ff_name##_round, op1, op2, exp, mant, 7) & ((1ULL << fmt_size) - 1)) << (i*fmt_size);   \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL3(lib_flexfloat_##ff_name##_round, op1, value, exp, mant, 7) & ((1ULL << fmt_size) - 1)) << (i*fmt_size); \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op2 = FREG_GET(1) >> (i*fmt_size);                                                                   \
        iss_freg_t op3 = FREG_GET(2) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL4(lib_flexfloat_##ff_name, op1, op2, op3, exp, mant, 7) & ((1ULL << fmt_size) - 1)) << (i*fmt_size);   \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        iss_freg_t op3 = FREG_GET(2) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL4(lib_flexfloat_##ff_name, op1, value, op3, exp, mant, 7) & ((1ULL << fmt_size) - 1)) << (i*fmt_size); \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}

#define VF_OP1(op, ff_name, fmt, fmt_size, exp, mant)                                                                             \
                                                                                                                        \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                                 \
{                                                                                                                       \
    iss_freg_t result = 0;                                                                                              \
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                             \
    {                                                                                                                   \
        iss_freg_t op1 = FREG_GET(0) >> (i*fmt_size);                                                                   \
        result |= (LIB_FF_CALL2(lib_flexfloat_##ff_name, op1, exp, mant, 7) & ((1ULL << fmt_size) - 1)) << (i*fmt_size);   \
    }                                                                                                                   \
                                                                                                                        \
    FREG_SET(0, result);                                                                                                \
                                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                                \
}

#if defined(CONFIG_GVSOC_ISS_RI5KY)

#define VF_CMP(op,fmt,fmt_size,exp,mant)                                                                                 \
                                                                                                                \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                         \
{                                                                                                               \
    iss_freg_t result = 0;                                                                                      \
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                     \
    {                                                                                                           \
        result |= LIB_FF_CALL2(lib_flexfloat_##op, FREG_GET(0) >> (i*fmt_size), FREG_GET(1) >> (i*fmt_size), exp, mant) << (i*fmt_size); \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                     \
    {                                                                                                           \
        result |= LIB_FF_CALL2(lib_flexfloat_##op, FREG_GET(0) >> (i*fmt_size), value, exp, mant) << (i*fmt_size);                \
    }                                                                                                           \
                                                                                                                \
    REG_SET(0, result);                                                                                         \
                                                                                                                \
    return iss_insn_next(iss, insn, pc);                                                                        \
}

#else

#define VF_CMP(op,fmt,fmt_size,exp,mant)                                                                                 \
                                                                                                                \
static inline iss_reg_t vf##op##_##fmt##_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                         \
{                                                                                                               \
    iss_freg_t result = 0;                                                                                      \
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                     \
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
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/fmt_size; i++)                                                                                     \
    {                                                                                                           \
        result |= LIB_FF_CALL2(lib_flexfloat_##op, FREG_GET(0) >> (i*fmt_size), value, exp, mant) << i;                \
    }                                                                                                           \
                                                                                                                \
    REG_SET(0, result);                                                                                         \
                                                                                                                \
    return iss_insn_next(iss, insn, pc);                                                                        \
}

#endif

#define VF_CPK(pos, dst_format, index, width, dst_exp, dst_mant)                                                                                     \
                                                                                                        \
static inline iss_reg_t vfcpk##pos##_##dst_format##_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)                 \
{                                                                                                       \
    iss_freg_t result = FREG_OUT_GET(0);                                                                \
                                                                                                        \
    result &= ~(((1ULL << (2*width)) - 1) << index);                                                                        \
                                                                                                        \
    result |= (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, dst_exp, dst_mant, 7) & ((1ULL << width) - 1)) << index;   \
    result |= (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(1), 8, 23, dst_exp, dst_mant, 7) & ((1ULL << width) - 1)) << (index + width);   \
                                                                                                        \
    FREG_SET(0, result);                                                                                \
                                                                                                        \
    return iss_insn_next(iss, insn, pc);                                                                \
}

#define VF_ALT_STUB(op, fmt, alt_fmt)                                                           \
                                                                                                \
static inline iss_reg_t op##_##fmt##_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)      \
{                                                                                               \
    if (insn->fmode == 3)                                                                       \
    {                                                                                           \
        return op##_##alt_fmt##_exec(iss, insn, pc);                                            \
    }                                                                                           \
    else                                                                                        \
    {                                                                                           \
        return op##_##fmt##_exec(iss, insn, pc);                                                \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline iss_reg_t op##_r_##fmt##_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)      \
{                                                                                               \
    if (insn->fmode == 3)                                                                       \
    {                                                                                           \
        return op##_r_##alt_fmt##_exec(iss, insn, pc);                                            \
    }                                                                                           \
    else                                                                                        \
    {                                                                                           \
        return op##_r_##fmt##_exec(iss, insn, pc);                                                \
    }                                                                                           \
}

#define VF_ALT_STUB_NO_R(op, fmt, alt_fmt)                                                           \
                                                                                                \
static inline iss_reg_t op##_##fmt##_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)      \
{                                                                                               \
    if (insn->fmode == 3)                                                                       \
    {                                                                                           \
        return op##_##alt_fmt##_exec(iss, insn, pc);                                            \
    }                                                                                           \
    else                                                                                        \
    {                                                                                           \
        return op##_##fmt##_exec(iss, insn, pc);                                                \
    }                                                                                           \
}


//
// with Xf16
//
VF_OP_ROUND(add, add, h, 16, 5, 10)
VF_OP_ROUND(sub, sub, h, 16, 5, 10)
VF_OP_ROUND(mul, mul, h, 16, 5, 10)
VF_OP_ROUND(div, div, h, 16, 5, 10)
VF_OP(min, min, h, 16, 5, 10)
VF_OP(max, max, h, 16, 5, 10)
VF_OP1(sqrt, sqrt_round, h, 16, 5, 10)
VF_OP3(mac, madd_round, h, 16, 5, 10)
#if defined(CONFIG_GVSOC_ISS_RI5KY)
VF_OP3(mre, msub_round, h, 16, 5, 10)
#else
VF_OP3(mre, nmsub_round, h, 16, 5, 10)
#endif
VF_OP(sgnj, sgnj, h, 16, 5, 10)
VF_OP(sgnjn, sgnjn, h, 16, 5, 10)
VF_OP(sgnjx, sgnjx, h, 16, 5, 10)

VF_CMP(eq, h, 16, 5, 10)
VF_CMP(ne, h, 16, 5, 10)
VF_CMP(lt, h, 16, 5, 10)
VF_CMP(ge, h, 16, 5, 10)
VF_CMP(le, h, 16, 5, 10)
VF_CMP(gt, h, 16, 5, 10)

VF_CPK(a, h, 0,  16, 5, 10)
VF_CPK(b, h, 32, 16, 5, 10)

static inline iss_reg_t vfclass_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 2, 16, 5, 10));
    return iss_insn_next(iss, insn, pc);
}

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
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xffff, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xffff, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_h_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsum_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*16);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*16);
        iss_freg_t op3 = FREG_GET(1) >> (i*16);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 5, 10, 7), 5, 10, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsum_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*16);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*16);
        iss_freg_t op3 = FREG_GET(1) >> (i*16);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 5, 10, 7), 5, 10, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsumex_s_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2)*16), 5, 10, 8, 23, 0);
        iss_freg_t op2 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2+1)*16), 5, 10, 8, 23, 0);
        iss_freg_t op3 = FREG_GET(1) >> (i*32);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 23, 7), 8, 23, 7) << (i*32);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsumex_s_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2)*16), 5, 10, 8, 23, 0);
        iss_freg_t op2 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2+1)*16), 5, 10, 8, 23, 0);
        iss_freg_t op3 = FREG_GET(1) >> (i*32);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 23, 7), 8, 23, 7) << (i*32);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

//
// with Xf16alt
//

VF_OP_ROUND(add, add, ah, 16, 8, 7)
VF_OP_ROUND(sub, sub, ah, 16, 8, 7)
VF_OP_ROUND(mul, mul, ah, 16, 8, 7)
VF_OP_ROUND(div, div, ah, 16, 8, 7)
VF_OP(min, min, ah, 16, 8, 7)
VF_OP(max, max, ah, 16, 8, 7)
VF_OP1(sqrt, sqrt_round, ah, 16, 8, 7)
VF_OP3(mac, madd_round, ah, 16, 8, 7)
#if defined(CONFIG_GVSOC_ISS_RI5KY)
VF_OP3(mre, msub_round, ah, 16, 8, 7)
#else
VF_OP3(mre, nmsub_round, ah, 16, 8, 7)
#endif
VF_OP(sgnj, sgnj, ah, 16, 8, 7)
VF_OP(sgnjn, sgnjn, ah, 16, 8, 7)
VF_OP(sgnjx, sgnjx, ah, 16, 8, 7)

VF_CMP(eq, ah, 16, 8, 7)
VF_CMP(ne, ah, 16, 8, 7)
VF_CMP(lt, ah, 16, 8, 7)
VF_CMP(ge, ah, 16, 8, 7)
VF_CMP(le, ah, 16, 8, 7)
VF_CMP(gt, ah, 16, 8, 7)

VF_CPK(a, ah, 0,  16, 8, 7)
VF_CPK(b, ah, 32, 16, 8, 7)

static inline iss_reg_t vfclass_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 2, 16, 8, 7));
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
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xffff, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xffff, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xffff, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xffff, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 8, 7, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 8, 7, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_ah_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 5, 10, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 5, 10, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsum_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*16);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*16);
        iss_freg_t op3 = FREG_GET(1) >> (i*16);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 7, 7), 8, 7, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsum_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*16);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*16);
        iss_freg_t op3 = FREG_GET(1) >> (i*16);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 7, 7), 8, 7, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsumex_s_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2)*16), 8, 7, 8, 23, 0);
        iss_freg_t op2 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2+1)*16), 8, 7, 8, 23, 0);
        iss_freg_t op3 = FREG_GET(1) >> (i*32);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 23, 7), 8, 23, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsumex_s_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/16 / 2; i++)
    {
        iss_freg_t op1 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2)*16), 8, 7, 8, 23, 0);
        iss_freg_t op2 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> ((i*2+1)*16), 8, 7, 8, 23, 0);
        iss_freg_t op3 = FREG_GET(1) >> (i*32);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 8, 23, 7), 8, 23, 7) << (i*16);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

//
// with Xf8
//
VF_OP_ROUND(add, add, b, 8, 5, 2)
VF_OP_ROUND(sub, sub, b, 8, 5, 2)
VF_OP_ROUND(mul, mul, b, 8, 5, 2)
VF_OP_ROUND(div, div, b, 8, 5, 2)
VF_OP(min, min, b, 8, 5, 2)
VF_OP(max, max, b, 8, 5, 2)
VF_OP1(sqrt, sqrt_round, b, 8, 5, 2)
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

static inline iss_reg_t vfclass_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 4, 8, 5, 2));
    return iss_insn_next(iss, insn, pc);
}


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

static inline iss_reg_t vfcvt_x_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) & 0xff, 5, 2, 7) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 8, 5, 2, 7) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 16, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_x_ff_round, FREG_GET(0) >> 24, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) & 0xff, 5, 2, 7) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 8, 5, 2, 7) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 16, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_xu_ff_round, FREG_GET(0) >> 24, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) & 0xff, 5, 2, 7) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 8, 5, 2, 7) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 16, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_x_round, REG_GET(0) >> 24, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_xu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) & 0xff, 5, 2, 7) & 0xff) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 8, 5, 2, 7) & 0xff) << 8) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 16, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL2(lib_flexfloat_cvt_ff_xu_round, REG_GET(0) >> 24, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsum_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/8 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*8);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*8);
        iss_freg_t op3 = FREG_GET(1) >> (i*8);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 5, 2, 7), 5, 2, 7) << (i*8);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsum_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/8 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*8);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*8);
        iss_freg_t op3 = FREG_GET(1) >> (i*8);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 5, 2, 7), 5, 2, 7) << (i*8);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

//
// with Xf8alt
//

VF_OP_ROUND(add, add, ab, 8, 4, 3)
VF_OP_ROUND(sub, sub, ab, 8, 4, 3)
VF_OP_ROUND(mul, mul, ab, 8, 4, 3)
VF_OP_ROUND(div, div, ab, 8, 4, 3)
VF_OP(min, min, ab, 8, 4, 3)
VF_OP(max, max, ab, 8, 4, 3)
VF_OP1(sqrt, sqrt_round, ab, 8, 4, 3)
VF_OP3(mac, madd_round, ab, 8, 4, 3)
VF_OP3(mre, nmsub_round, ab, 8, 4, 3)
VF_OP(sgnj, sgnj, ab, 8, 4, 3)
VF_OP(sgnjn, sgnjn, ab, 8, 4, 3)
VF_OP(sgnjx, sgnjx, ab, 8, 4, 3)

VF_CMP(eq, ab, 8, 4, 3)
VF_CMP(ne, ab, 8, 4, 3)
VF_CMP(lt, ab, 8, 4, 3)
VF_CMP(ge, ab, 8, 4, 3)
VF_CMP(le, ab, 8, 4, 3)
VF_CMP(gt, ab, 8, 4, 3)

VF_CPK(a, ab, 0,  8, 4, 3)
VF_CPK(b, ab, 16, 8, 4, 3)
VF_CPK(c, ab, 32,  8, 4, 3)
VF_CPK(d, ab, 48, 8, 4, 3)

static inline iss_reg_t vfclass_ab_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL3(lib_flexfloat_vclass, FREG_GET(0), 4, 8, 4, 3));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsum_ab_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/8 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*8);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*8);
        iss_freg_t op3 = FREG_GET(1) >> (i*8);

        result |= LIB_FF_CALL3(lib_flexfloat_add_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 4, 3, 7), 4, 3, 7) << (i*8);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsum_ab_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_freg_t result = 0;
    for (int i=0; i<CONFIG_GVSOC_ISS_FP_WIDTH/8 / 2; i++)
    {
        iss_freg_t op1 = FREG_GET(0) >> ((i*2)*8);
        iss_freg_t op2 = FREG_GET(0) >> ((i*2+1)*8);
        iss_freg_t op3 = FREG_GET(1) >> (i*8);

        result |= LIB_FF_CALL3(lib_flexfloat_sub_round,
            op3, LIB_FF_CALL3(lib_flexfloat_add_round, op1, op2, 4, 3, 7), 4, 3, 7) << (i*8);
    }

    FREG_SET(0, result);

    return iss_insn_next(iss, insn, pc);
}

//
// with Xf32
//

VF_OP_ROUND(add, add, s, 32, 8, 23)
VF_OP_ROUND(sub, sub, s, 32, 8, 23)
VF_OP_ROUND(mul, mul, s, 32, 8, 23)
VF_OP_ROUND(div, div, s, 32, 8, 23)
VF_OP(min, min, s, 32, 8, 23)
VF_OP(max, max, s, 32, 8, 23)
VF_OP1(sqrt, sqrt_round, s, 32, 8, 23)
VF_OP3(mac, madd_round, s, 32, 8, 23)
VF_OP3(mre, nmsub_round, s, 32, 8, 23)
VF_OP(sgnj, sgnj, s, 32, 8, 23)
VF_OP(sgnjn, sgnjn, s, 32, 8, 23)
VF_OP(sgnjx, sgnjx, s, 32, 8, 23)

VF_CMP(eq, s, 32, 8, 23)
VF_CMP(ne, s, 32, 8, 23)
VF_CMP(lt, s, 32, 8, 23)
VF_CMP(ge, s, 32, 8, 23)
VF_CMP(le, s, 32, 8, 23)
VF_CMP(gt, s, 32, 8, 23)


static inline iss_reg_t vfsum_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(1) & 0xffffffff,
        (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 32, FREG_GET(0), 8, 23, 7) & 0xffffffff), 8, 23, 7) & 0xffffffff));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnsum_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(1) & 0xffffffff,
        (LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0) >> 32, FREG_GET(0), 8, 23, 7) & 0xffffffff), 8, 23, 7) & 0xffffffff));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcpka_s_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), 8, 23, 8, 23, 7) & 0xffffffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(1), 8, 23, 8, 23, 7) & 0xffffffff) << 32));
    return iss_insn_next(iss, insn, pc);
}



VF_CPK(a, b, 0,  8, 5, 2)
VF_CPK(b, b, 16, 8, 5, 2)
VF_CPK(c, b, 32, 8, 5, 2)
VF_CPK(d, b, 48, 8, 5, 2)


// If Xf16 extension also supported
static inline iss_reg_t vfcvt_h_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xff, 5, 2, 5, 10, 7) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 5, 10, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_h_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 5, 10, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 5, 10, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}

// If Xf16alt extension also supported
static inline iss_reg_t vfcvt_ah_b_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, (LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xff, 5, 2, 8, 7, 7) & 0xffff) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, (REG_GET(0) >> 8) & 0xff, 5, 2, 8, 7, 7) & 0xffff) << 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_b_ah_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) & 0xffff, 8, 7, 5, 2, 7) & 0xff) << 16) |
                   ((LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0) >> 16, 8, 7, 5, 2, 7) & 0xff) << 24));
    return iss_insn_next(iss, insn, pc);
}
