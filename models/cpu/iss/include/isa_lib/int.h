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

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

static inline unsigned long int DoExtend(unsigned long int R, unsigned long int e, unsigned long int m)

{
#ifdef OLD
    return R;
#else
    if ((m + e) == 31)
        return R;
    else
    {
        unsigned int S = (R >> (e + m)) & 0x1;
        unsigned int Mask = S ? (((unsigned int)-1) << (e + m)) : 0;
        return (R | Mask);
    }
#endif
}

static inline iss_uim_t getField(iss_uim_t val, int shift, int bits)
{
    return (val >> shift) & ((1ULL << bits) - 1);
}

static inline int get_signed_value(iss_uim_t val, int bits)
{
    return ((iss_sim_t)val) << (ISS_REG_WIDTH - bits) >> (ISS_REG_WIDTH - bits);
}

static inline iss_opcode_t getSignedField(iss_opcode_t val, int shift, int bits)
{
    return get_signed_value(getField(val, shift, bits), bits);
}

/*
 * LOGICAL OPERATIONS
 */

static inline iss_uim_t lib_SLL(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a << b;
}

static inline iss_uim_t lib_SRL(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a >> b;
}

static inline iss_uim_t lib_SRA(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return ((iss_sim_t)a) >> b;
}

static inline iss_uim_t lib_SLLW(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return get_signed_value(a << b, 32);
}

static inline iss_uim_t lib_SRLW(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return get_signed_value(((uint32_t)a) >> b, 32);
}

static inline iss_uim_t lib_SRAW(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return get_signed_value(((int32_t)a) >> b, 32);
}

static inline iss_uim_t lib_ROR(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return (a >> b) | (a << (32 - b));
}

static inline iss_uim_t lib_XOR(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a ^ b;
}

static inline iss_uim_t lib_OR(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a | b;
}

static inline iss_uim_t lib_AND(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a & b;
}

/*
 * ARITHMETIC OPERATIONS
 */

static inline iss_uim_t lib_ADD(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a + b;
}

static inline iss_uim_t lib_ADDW(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return get_signed_value(a + b, 32);
}

static inline iss_uim_t lib_SUB(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return a - b;
}

static inline iss_uim_t lib_SUBW(Iss *s, iss_uim_t a, iss_uim_t b)
{
    return get_signed_value(a - b, 32);
}

static inline unsigned int lib_MAC(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + b * c; }
static inline unsigned int lib_MSU(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - b * c; }
static inline unsigned int lib_MMUL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return -b * c; }

#define SL(val) ((int16_t)((val)&0xffff))
#define SH(val) ((int16_t)(((val) >> 16) & 0xffff))
#define ZL(val) ((uint16_t)((val)&0xffff))
#define ZH(val) ((uint16_t)(((val) >> 16) & 0xffff))
#define L_H_TO_W(l, h) (((l)&0xffff) | ((h) << 16))

static inline unsigned int lib_MAC_SL_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c)
{
    return a + SL(b) * SL(c);
}
static inline unsigned int lib_MAC_SL_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SL(b) * SH(c); }
static inline unsigned int lib_MAC_SH_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SH(b) * SL(c); }
static inline unsigned int lib_MAC_SH_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SH(b) * SH(c); }
static inline unsigned int lib_MAC_ZL_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZL(b) * ZL(c); }
static inline unsigned int lib_MAC_ZL_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZL(b) * ZH(c); }
static inline unsigned int lib_MAC_ZH_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZH(b) * ZL(c); }
static inline unsigned int lib_MAC_ZH_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZH(b) * ZH(c); }

static inline unsigned int lib_MAC_SL_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SL(b) * ZL(c); }
static inline unsigned int lib_MAC_SL_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SL(b) * ZH(c); }
static inline unsigned int lib_MAC_SH_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SH(b) * ZL(c); }
static inline unsigned int lib_MAC_SH_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + SH(b) * ZH(c); }
static inline unsigned int lib_MAC_ZL_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZL(b) * SL(c); }
static inline unsigned int lib_MAC_ZL_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZL(b) * SH(c); }
static inline unsigned int lib_MAC_ZH_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZH(b) * SL(c); }
static inline unsigned int lib_MAC_ZH_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a + ZH(b) * SH(c); }

static inline unsigned int lib_MAC_SL_SL_NR(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift) { return ((int32_t)(a + SL(b) * SL(c))) >> shift; }
static inline unsigned int lib_MAC_SH_SH_NR(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift) { return ((int32_t)(a + SH(b) * SH(c))) >> shift; }
static inline unsigned int lib_MAC_ZL_ZL_NR(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift) { return ((uint32_t)(a + ZL(b) * ZL(c))) >> shift; }
static inline unsigned int lib_MAC_ZH_ZH_NR(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift) { return ((uint32_t)(a + ZH(b) * ZH(c))) >> shift; }

static inline unsigned int lib_MAC_SL_SL_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    int32_t result = (int32_t)(a + SL(b) * SL(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MAC_SH_SH_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    int32_t result = (int32_t)(a + SH(b) * SH(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MAC_ZL_ZL_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    uint32_t result = (uint32_t)(a + ZL(b) * ZL(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MAC_ZH_ZH_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    uint32_t result = (uint32_t)(a + ZH(b) * ZH(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}

static inline unsigned int lib_MSU_SL_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SL(b) * SL(c); }
static inline unsigned int lib_MSU_SL_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SL(b) * SH(c); }
static inline unsigned int lib_MSU_SH_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SH(b) * SL(c); }
static inline unsigned int lib_MSU_SH_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SH(b) * SH(c); }
static inline unsigned int lib_MSU_ZL_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZL(b) * ZL(c); }
static inline unsigned int lib_MSU_ZL_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZL(b) * ZH(c); }
static inline unsigned int lib_MSU_ZH_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZH(b) * ZL(c); }
static inline unsigned int lib_MSU_ZH_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZH(b) * ZH(c); }

static inline unsigned int lib_MSU_SL_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SL(b) * ZL(c); }
static inline unsigned int lib_MSU_SL_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SL(b) * ZH(c); }
static inline unsigned int lib_MSU_SH_ZL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SH(b) * ZL(c); }
static inline unsigned int lib_MSU_SH_ZH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - SH(b) * ZH(c); }
static inline unsigned int lib_MSU_ZL_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZL(b) * SL(c); }
static inline unsigned int lib_MSU_ZL_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZL(b) * SH(c); }
static inline unsigned int lib_MSU_ZH_SL(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZH(b) * SL(c); }
static inline unsigned int lib_MSU_ZH_SH(Iss *s, unsigned int a, unsigned int b, unsigned int c) { return a - ZH(b) * SH(c); }

static inline unsigned int lib_MUL_SL_SL(Iss *s, unsigned int b, unsigned int c) { return SL(b) * SL(c); }
static inline unsigned int lib_MUL_SL_SH(Iss *s, unsigned int b, unsigned int c) { return SL(b) * SH(c); }
static inline unsigned int lib_MUL_SH_SL(Iss *s, unsigned int b, unsigned int c) { return SH(b) * SL(c); }
static inline unsigned int lib_MUL_SH_SH(Iss *s, unsigned int b, unsigned int c) { return SH(b) * SH(c); }
static inline unsigned int lib_MUL_ZL_ZL(Iss *s, unsigned int b, unsigned int c) { return ZL(b) * ZL(c); }
static inline unsigned int lib_MUL_ZL_ZH(Iss *s, unsigned int b, unsigned int c) { return ZL(b) * ZH(c); }
static inline unsigned int lib_MUL_ZH_ZL(Iss *s, unsigned int b, unsigned int c) { return ZH(b) * ZL(c); }
static inline unsigned int lib_MUL_ZH_ZH(Iss *s, unsigned int b, unsigned int c) { return ZH(b) * ZH(c); }

static inline unsigned int lib_MUL_SL_SL_NR(Iss *s, unsigned int b, unsigned int c, unsigned int shift) { return ((int32_t)(SL(b) * SL(c))) >> shift; }
static inline unsigned int lib_MUL_SH_SH_NR(Iss *s, unsigned int b, unsigned int c, unsigned int shift) { return ((int32_t)(SH(b) * SH(c))) >> shift; }
static inline unsigned int lib_MUL_ZL_ZL_NR(Iss *s, unsigned int b, unsigned int c, unsigned int shift) { return ((uint32_t)(ZL(b) * ZL(c))) >> shift; }
static inline unsigned int lib_MUL_ZH_ZH_NR(Iss *s, unsigned int b, unsigned int c, unsigned int shift) { return ((uint32_t)(ZH(b) * ZH(c))) >> shift; }

static inline unsigned int lib_MUL_SL_SL_NR_R(Iss *s, unsigned int b, unsigned int c, unsigned int shift)
{
    int32_t result = (int32_t)(SL(b) * SL(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MUL_SH_SH_NR_R(Iss *s, unsigned int b, unsigned int c, unsigned int shift)
{
    int32_t result = (int32_t)(SH(b) * SH(c));
    if (shift > 0)
        result = (result + (1ULL << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MUL_ZL_ZL_NR_R(Iss *s, unsigned int b, unsigned int c, unsigned int shift)
{
    uint32_t result = (uint32_t)(ZL(b) * ZL(c));
    if (shift > 0)
        result = (result + (1 << (shift - 1))) >> shift;
    return result;
}
static inline unsigned int lib_MUL_ZH_ZH_NR_R(Iss *s, unsigned int b, unsigned int c, unsigned int shift)
{
    uint32_t result = (uint32_t)(ZH(b) * ZH(c));
    if (shift > 0)
        result = (result + (1 << (shift - 1))) >> shift;
    return result;
}

static inline unsigned int lib_MUL_SL_ZL(Iss *s, unsigned int b, unsigned int c) { return SL(b) * ZL(c); }
static inline unsigned int lib_MUL_SL_ZH(Iss *s, unsigned int b, unsigned int c) { return SL(b) * ZH(c); }
static inline unsigned int lib_MUL_SH_ZL(Iss *s, unsigned int b, unsigned int c) { return SH(b) * ZL(c); }
static inline unsigned int lib_MUL_SH_ZH(Iss *s, unsigned int b, unsigned int c) { return SH(b) * ZH(c); }
static inline unsigned int lib_MUL_ZL_SL(Iss *s, unsigned int b, unsigned int c) { return ZL(b) * SL(c); }
static inline unsigned int lib_MUL_ZL_SH(Iss *s, unsigned int b, unsigned int c) { return ZL(b) * SH(c); }
static inline unsigned int lib_MUL_ZH_SL(Iss *s, unsigned int b, unsigned int c) { return ZH(b) * SL(c); }
static inline unsigned int lib_MUL_ZH_SH(Iss *s, unsigned int b, unsigned int c) { return ZH(b) * SH(c); }

static inline unsigned int lib_MMUL_SL_SL(Iss *s, unsigned int b, unsigned int c) { return -SL(b) * SL(c); }
static inline unsigned int lib_MMUL_SL_SH(Iss *s, unsigned int b, unsigned int c) { return -SL(b) * SH(c); }
static inline unsigned int lib_MMUL_SH_SL(Iss *s, unsigned int b, unsigned int c) { return -SH(b) * SL(c); }
static inline unsigned int lib_MMUL_SH_SH(Iss *s, unsigned int b, unsigned int c) { return -SH(b) * SH(c); }
static inline unsigned int lib_MMUL_ZL_ZL(Iss *s, unsigned int b, unsigned int c) { return -ZL(b) * ZL(c); }
static inline unsigned int lib_MMUL_ZL_ZH(Iss *s, unsigned int b, unsigned int c) { return -ZL(b) * ZH(c); }
static inline unsigned int lib_MMUL_ZH_ZL(Iss *s, unsigned int b, unsigned int c) { return -ZH(b) * ZL(c); }
static inline unsigned int lib_MMUL_ZH_ZH(Iss *s, unsigned int b, unsigned int c) { return -ZH(b) * ZH(c); }

static inline unsigned int lib_MMUL_SL_ZL(Iss *s, unsigned int b, unsigned int c) { return -SL(b) * ZL(c); }
static inline unsigned int lib_MMUL_SL_ZH(Iss *s, unsigned int b, unsigned int c) { return -SL(b) * ZH(c); }
static inline unsigned int lib_MMUL_SH_ZL(Iss *s, unsigned int b, unsigned int c) { return -SH(b) * ZL(c); }
static inline unsigned int lib_MMUL_SH_ZH(Iss *s, unsigned int b, unsigned int c) { return -SH(b) * ZH(c); }
static inline unsigned int lib_MMUL_ZL_SL(Iss *s, unsigned int b, unsigned int c) { return -ZL(b) * SL(c); }
static inline unsigned int lib_MMUL_ZL_SH(Iss *s, unsigned int b, unsigned int c) { return -ZL(b) * SH(c); }
static inline unsigned int lib_MMUL_ZH_SL(Iss *s, unsigned int b, unsigned int c) { return -ZH(b) * SL(c); }
static inline unsigned int lib_MMUL_ZH_SH(Iss *s, unsigned int b, unsigned int c) { return -ZH(b) * SH(c); }

static inline unsigned int lib_MULS(Iss *s, int a, int b) { return a * b; }
static inline uint64_t lib_MULU(Iss *s, uint64_t a, uint64_t b) { return a * b; }
static inline uint64_t lib_MULW(Iss *s, uint64_t a, uint64_t b) { return get_signed_value(a * b, 32); }
static inline int64_t lib_MULS_64(Iss *s, int32_t a, int32_t b) { return (int64_t)a * (int64_t)b; }
static inline uint64_t lib_MULU_64(Iss *s, uint64_t a, uint64_t b) { return a * b; }
static inline unsigned int lib_DIVS(Iss *s, int a, int b)
{
    if (b == 0)
        return 0;
    else
        return a / b;
}
static inline unsigned int lib_DIVU(Iss *s, unsigned int a, unsigned int b)
{
    if (b == 0)
        return 0;
    else
        return a / b;
}
static inline unsigned int lib_MINU(Iss *s, unsigned int a, unsigned int b) { return a < b ? a : b; }
static inline int lib_MINS(Iss *s, int a, int b) { return a < b ? a : b; }
static inline unsigned int lib_MAXU(Iss *s, unsigned int a, unsigned int b) { return a > b ? a : b; }
static inline int lib_MAXS(Iss *s, int a, int b) { return a > b ? a : b; }
static inline int lib_ABS(Iss *s, int a) { return a >= 0 ? a : -a; }
static inline unsigned int lib_AVGU(Iss *s, unsigned int a, unsigned int b) { return (a + b) >> 1; }
static inline int lib_AVGS(Iss *s, int a, int b) { return (a + b) >> 1; }

static inline uint64_t lib_MINU_64(Iss *s, uint64_t a, uint64_t b) { return a < b ? a : b; }
static inline int64_t lib_MINS_64(Iss *s, int64_t a, int64_t b) { return a < b ? a : b; }
static inline uint64_t lib_MAXU_64(Iss *s, uint64_t a, uint64_t b) { return a > b ? a : b; }
static inline int64_t lib_MAXS_64(Iss *s, int64_t a, int64_t b) { return a > b ? a : b; }
static inline int64_t lib_ABS_64(Iss *s, int64_t a) { return a < 0 ? -a : a; }

/*
 * CONDITIONAL OPERATIONS
 */

static inline unsigned int lib_CMOV(Iss *s, unsigned int flag, unsigned int a, unsigned int b) { return flag ? a : b; }

/*
 * BIT MANIPULATION OPERATIONS
 */

static inline unsigned int lib_FF1_or1k(Iss *s, unsigned int a) { return ffs(a); }

static inline unsigned int lib_FL1_or1k(Iss *s, unsigned int t)
{
    /* Reverse the word and use ffs */
    t = (((t & 0xaaaaaaaa) >> 1) | ((t & 0x55555555) << 1));
    t = (((t & 0xcccccccc) >> 2) | ((t & 0x33333333) << 2));
    t = (((t & 0xf0f0f0f0) >> 4) | ((t & 0x0f0f0f0f) << 4));
    t = (((t & 0xff00ff00) >> 8) | ((t & 0x00ff00ff) << 8));
    t = ffs((t >> 16) | (t << 16));

    return (0 == t ? t : 33 - t);
}

static inline unsigned int lib_FF1(Iss *s, unsigned int a)
{
    unsigned int result = ffs(a);
    if (result == 0)
        return 32;
    else
        return result - 1;
}

static inline unsigned int lib_FL1(Iss *s, unsigned int t)
{
    /* Reverse the word and use ffs */
    t = (((t & 0xaaaaaaaa) >> 1) | ((t & 0x55555555) << 1));
    t = (((t & 0xcccccccc) >> 2) | ((t & 0x33333333) << 2));
    t = (((t & 0xf0f0f0f0) >> 4) | ((t & 0x0f0f0f0f) << 4));
    t = (((t & 0xff00ff00) >> 8) | ((t & 0x00ff00ff) << 8));
    t = ffs((t >> 16) | (t << 16));

    return (0 == t ? 32 : 32 - t);
}

static inline unsigned int lib_CNT(Iss *s, unsigned int t)
{
#if 1
    return __builtin_popcount(t);
#else
    unsigned int v = cpu->regs[pc->inReg[0]];
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    cpu->regs[pc->outReg[0]] = ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
#endif
}

static inline unsigned int lib_CNT_64(Iss *s, uint64_t t)
{
#if 1
    return __builtin_popcount((t >> 32) & ((1ll << 32) - 1)) + __builtin_popcount(t & ((1ll << 32) - 1));
#else
    uint64_t v = cpu->regs[pc->inReg[0]];
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    cpu->regs[pc->outReg[0]] = ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
#endif
}

static inline unsigned int lib_CLB(Iss *s, unsigned int t)
{
    if (t == 0)
        return 0;
    else
        return __builtin_clrsb(t);
}

static inline unsigned int lib_BSET(Iss *s, unsigned int val, unsigned int mask)
{
    return val | mask;
}

static inline unsigned int lib_BCLR(Iss *s, unsigned int val, unsigned int mask)
{
    return val & ~mask;
}

static inline int lib_BEXTRACT(Iss *s, unsigned int val, unsigned int bits, unsigned int shift)
{
    if (shift + bits > 32)
        bits = 32 - shift;
    return getSignedField(val, shift, bits);
}

static inline unsigned int lib_BEXTRACTU(Iss *s, unsigned int val, unsigned int mask, unsigned int shift)
{
    return (val & mask) >> shift;
}

static inline unsigned int lib_BINSERT(Iss *s, unsigned int a, unsigned int b, unsigned int mask, unsigned int shift)
{
    return (b & ~mask) | ((a << shift) & mask);
}

/*
 *  VECTORS
 */

#define VEC_OP(operName, type, elemType, elemSize, num_elem, oper)                                            \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b)        \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        elemType *tmp_b = (elemType *)&b;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = tmp_a[i] oper tmp_b[i];                                                              \
        return out;                                                                                           \
    }                                                                                                         \
                                                                                                              \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, elemType b) \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = tmp_a[i] oper b;                                                                     \
        return out;                                                                                           \
    }

#define VEC_OP_DIV2(operName, type, elemType, elemSize, num_elem, oper)                                       \
    static inline type lib_VEC_##operName##_##elemType##_to_##type##_div2(Iss *s, type a, type b) \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        elemType *tmp_b = (elemType *)&b;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = ((elemType)(tmp_a[i] oper tmp_b[i])) >> 1;                                           \
        return out;                                                                                           \
    }

#define VEC_OP_DIV4(operName, type, elemType, elemSize, num_elem, oper)                                       \
    static inline type lib_VEC_##operName##_##elemType##_to_##type##_div4(Iss *s, type a, type b) \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        elemType *tmp_b = (elemType *)&b;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = ((elemType)(tmp_a[i] oper tmp_b[i])) >> 2;                                           \
        return out;                                                                                           \
    }

#define VEC_OP_DIV8(operName, type, elemType, elemSize, num_elem, oper)                                       \
    static inline type lib_VEC_##operName##_##elemType##_to_##type##_div8(Iss *s, type a, type b) \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        elemType *tmp_b = (elemType *)&b;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = ((elemType)(tmp_a[i] oper tmp_b[i])) >> 3;                                           \
        return out;                                                                                           \
    }

#define VEC_EXPR(operName, type, elemType, elemSize, num_elem, expr)                                   \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b) \
    {                                                                                                  \
        elemType *tmp_a = (elemType *)&a;                                                              \
        elemType *tmp_b = (elemType *)&b;                                                              \
        type out;                                                                                      \
        elemType *tmp_out = (elemType *)&out;                                                          \
        int i;                                                                                         \
        for (i = 0; i < num_elem; i++)                                                                 \
            tmp_out[i] = expr;                                                                         \
        return out;                                                                                    \
    }

#define VEC_EXPR_SC(operName, type, elemType, elemSize, num_elem, expr)                                       \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, elemType b) \
    {                                                                                                         \
        elemType *tmp_a = (elemType *)&a;                                                                     \
        type out;                                                                                             \
        elemType *tmp_out = (elemType *)&out;                                                                 \
        int i;                                                                                                \
        for (i = 0; i < num_elem; i++)                                                                        \
            tmp_out[i] = expr;                                                                                \
        return out;                                                                                           \
    }

#define VEC_CMP(operName, type, elemType, elemSize, num_elem, oper)                                              \
    static inline type lib_VEC_CMP##operName##_##elemType##_to_##type(Iss *s, type a, type b)        \
    {                                                                                                            \
        elemType *tmp_a = (elemType *)&a;                                                                        \
        elemType *tmp_b = (elemType *)&b;                                                                        \
        type out;                                                                                                \
        elemType *tmp_out = (elemType *)&out;                                                                    \
        int i;                                                                                                   \
        for (i = 0; i < num_elem; i++)                                                                           \
            if (tmp_a[i] oper tmp_b[i])                                                                          \
                tmp_out[i] = -1;                                                                                 \
            else                                                                                                 \
                tmp_out[i] = 0;                                                                                  \
        return out;                                                                                              \
    }                                                                                                            \
                                                                                                                 \
    static inline type lib_VEC_CMP##operName##_SC_##elemType##_to_##type(Iss *s, type a, elemType b) \
    {                                                                                                            \
        elemType *tmp_a = (elemType *)&a;                                                                        \
        type out;                                                                                                \
        elemType *tmp_out = (elemType *)&out;                                                                    \
        int i;                                                                                                   \
        for (i = 0; i < num_elem; i++)                                                                           \
            if (tmp_a[i] oper b)                                                                                 \
                tmp_out[i] = -1;                                                                                 \
            else                                                                                                 \
                tmp_out[i] = 0;                                                                                  \
        return out;                                                                                              \
    }

#define VEC_ALL(operName, type, elemType, elemSize, num_elem, oper)                                                             \
    static inline type lib_VEC_ALL_##operName##_##elemType##_to_##type(Iss *s, type a, type b, int *flagPtr)        \
    {                                                                                                                           \
        elemType *tmp_a = (elemType *)&a;                                                                                       \
        elemType *tmp_b = (elemType *)&b;                                                                                       \
        type out;                                                                                                               \
        elemType *tmp_out = (elemType *)&out;                                                                                   \
        int isOper = 1;                                                                                                         \
        int i;                                                                                                                  \
        for (i = 0; i < num_elem; i++)                                                                                          \
            if (tmp_a[i] oper tmp_b[i])                                                                                         \
                tmp_out[i] = -1;                                                                                                \
            else                                                                                                                \
            {                                                                                                                   \
                tmp_out[i] = 0;                                                                                                 \
                isOper = 0;                                                                                                     \
            }                                                                                                                   \
        *flagPtr = isOper;                                                                                                      \
        return out;                                                                                                             \
    }                                                                                                                           \
                                                                                                                                \
    static inline type lib_VEC_ALL_SC_##operName##_##elemType##_to_##type(Iss *s, type a, elemType b, int *flagPtr) \
    {                                                                                                                           \
        elemType *tmp_a = (elemType *)&a;                                                                                       \
        type out;                                                                                                               \
        elemType *tmp_out = (elemType *)&out;                                                                                   \
        int isOper = 1;                                                                                                         \
        int i;                                                                                                                  \
        for (i = 0; i < num_elem; i++)                                                                                          \
            if (tmp_a[i] oper b)                                                                                                \
                tmp_out[i] = -1;                                                                                                \
            else                                                                                                                \
            {                                                                                                                   \
                tmp_out[i] = 0;                                                                                                 \
                isOper = 0;                                                                                                     \
            }                                                                                                                   \
        *flagPtr = isOper;                                                                                                      \
        return out;                                                                                                             \
    }

#define VEC_ANY(operName, type, elemType, elemSize, num_elem, oper)                                                             \
    static inline type lib_VEC_ANY_##operName##_##elemType##_to_##type(Iss *s, type a, type b, int *flagPtr)        \
    {                                                                                                                           \
        elemType *tmp_a = (elemType *)&a;                                                                                       \
        elemType *tmp_b = (elemType *)&b;                                                                                       \
        type out;                                                                                                               \
        elemType *tmp_out = (elemType *)&out;                                                                                   \
        int isOper = 0;                                                                                                         \
        int i;                                                                                                                  \
        for (i = 0; i < num_elem; i++)                                                                                          \
            if (tmp_a[i] oper tmp_b[i])                                                                                         \
            {                                                                                                                   \
                tmp_out[i] = -1;                                                                                                \
                isOper = 1;                                                                                                     \
            }                                                                                                                   \
            else                                                                                                                \
            {                                                                                                                   \
                tmp_out[i] = 0;                                                                                                 \
            }                                                                                                                   \
        *flagPtr = isOper;                                                                                                      \
        return out;                                                                                                             \
    }                                                                                                                           \
                                                                                                                                \
    static inline type lib_VEC_ANY_SC_##operName##_##elemType##_to_##type(Iss *s, type a, elemType b, int *flagPtr) \
    {                                                                                                                           \
        elemType *tmp_a = (elemType *)&a;                                                                                       \
        type out;                                                                                                               \
        elemType *tmp_out = (elemType *)&out;                                                                                   \
        int isOper = 0;                                                                                                         \
        int i;                                                                                                                  \
        for (i = 0; i < num_elem; i++)                                                                                          \
            if (tmp_a[i] oper b)                                                                                                \
            {                                                                                                                   \
                tmp_out[i] = -1;                                                                                                \
                isOper = 1;                                                                                                     \
            }                                                                                                                   \
            else                                                                                                                \
            {                                                                                                                   \
                tmp_out[i] = 0;                                                                                                 \
            }                                                                                                                   \
        *flagPtr = isOper;                                                                                                      \
        return out;                                                                                                             \
    }

VEC_ALL(EQ, int32_t, int8_t, 1, 4, ==)
VEC_ALL(EQ, int32_t, int16_t, 2, 2, ==)

VEC_ALL(GE, int32_t, int8_t, 1, 4, >=)
VEC_ALL(GE, int32_t, int16_t, 2, 2, >=)

VEC_ALL(GEU, uint32_t, uint8_t, 1, 4, >=)
VEC_ALL(GEU, uint32_t, uint16_t, 2, 2, >=)

VEC_ALL(GT, int32_t, int8_t, 1, 4, >)
VEC_ALL(GT, int32_t, int16_t, 2, 2, >)

VEC_ALL(GTU, uint32_t, uint8_t, 1, 4, >)
VEC_ALL(GTU, uint32_t, uint16_t, 2, 2, >)

VEC_ALL(LE, int32_t, int8_t, 1, 4, <=)
VEC_ALL(LE, int32_t, int16_t, 2, 2, <=)

VEC_ALL(LEU, uint32_t, uint8_t, 1, 4, <=)
VEC_ALL(LEU, uint32_t, uint16_t, 2, 2, <=)

VEC_ALL(LT, int32_t, int8_t, 1, 4, <)
VEC_ALL(LT, int32_t, int16_t, 2, 2, <)

VEC_ALL(LTU, uint32_t, uint8_t, 1, 4, <)
VEC_ALL(LTU, uint32_t, uint16_t, 2, 2, <)

VEC_ALL(NE, int32_t, int8_t, 1, 4, !=)
VEC_ALL(NE, int32_t, int16_t, 2, 2, !=)

VEC_ANY(EQ, int32_t, int8_t, 1, 4, ==)
VEC_ANY(EQ, int32_t, int16_t, 2, 2, ==)

VEC_ANY(GE, int32_t, int8_t, 1, 4, >=)
VEC_ANY(GE, int32_t, int16_t, 2, 2, >=)

VEC_ANY(GEU, uint32_t, uint8_t, 1, 4, >=)
VEC_ANY(GEU, uint32_t, uint16_t, 2, 2, >=)

VEC_ANY(GT, int32_t, int8_t, 1, 4, >)
VEC_ANY(GT, int32_t, int16_t, 2, 2, >)

VEC_ANY(GTU, uint32_t, uint8_t, 1, 4, >)
VEC_ANY(GTU, uint32_t, uint16_t, 2, 2, >)

VEC_ANY(LE, int32_t, int8_t, 1, 4, <=)
VEC_ANY(LE, int32_t, int16_t, 2, 2, <=)

VEC_ANY(LEU, uint32_t, uint8_t, 1, 4, <=)
VEC_ANY(LEU, uint32_t, uint16_t, 2, 2, <=)

VEC_ANY(LT, int32_t, int8_t, 1, 4, <)
VEC_ANY(LT, int32_t, int16_t, 2, 2, <)

VEC_ANY(LTU, uint32_t, uint8_t, 1, 4, <)
VEC_ANY(LTU, uint32_t, uint16_t, 2, 2, <)

VEC_ANY(NE, int32_t, int8_t, 1, 4, !=)
VEC_ANY(NE, int32_t, int16_t, 2, 2, !=)

VEC_CMP(EQ, int32_t, int8_t, 1, 4, ==)
VEC_CMP(EQ, int32_t, int16_t, 2, 2, ==)

VEC_CMP(GE, int32_t, int8_t, 1, 4, >=)
VEC_CMP(GE, int32_t, int16_t, 2, 2, >=)

VEC_CMP(GEU, uint32_t, uint8_t, 1, 4, >=)
VEC_CMP(GEU, uint32_t, uint16_t, 2, 2, >=)

VEC_CMP(GT, int32_t, int8_t, 1, 4, >)
VEC_CMP(GT, int32_t, int16_t, 2, 2, >)

VEC_CMP(GTU, uint32_t, uint8_t, 1, 4, >)
VEC_CMP(GTU, uint32_t, uint16_t, 2, 2, >)

VEC_CMP(LE, int32_t, int8_t, 1, 4, <=)
VEC_CMP(LE, int32_t, int16_t, 2, 2, <=)

VEC_CMP(LEU, uint32_t, uint8_t, 1, 4, <=)
VEC_CMP(LEU, uint32_t, uint16_t, 2, 2, <=)

VEC_CMP(LT, int32_t, int8_t, 1, 4, <)
VEC_CMP(LT, int32_t, int16_t, 2, 2, <)

VEC_CMP(LTU, uint32_t, uint8_t, 1, 4, <)
VEC_CMP(LTU, uint32_t, uint16_t, 2, 2, <)

VEC_CMP(NE, int32_t, int8_t, 1, 4, !=)
VEC_CMP(NE, int32_t, int16_t, 2, 2, !=)

VEC_OP(ADD, int32_t, int8_t, 1, 4, +)
VEC_OP_DIV2(ADD, int32_t, int8_t, 1, 4, +)
VEC_OP_DIV4(ADD, int32_t, int8_t, 1, 4, +)
VEC_OP(ADD, int32_t, int16_t, 2, 2, +)
VEC_OP_DIV2(ADD, int32_t, int16_t, 2, 2, +)
VEC_OP_DIV4(ADD, int32_t, int16_t, 2, 2, +)
VEC_OP_DIV8(ADD, int32_t, int16_t, 2, 2, +)

VEC_OP(SUB, int32_t, int8_t, 1, 4, -)
VEC_OP_DIV2(SUB, int32_t, int8_t, 1, 4, -)
VEC_OP_DIV4(SUB, int32_t, int8_t, 1, 4, -)
VEC_OP(SUB, int32_t, int16_t, 2, 2, -)
VEC_OP_DIV2(SUB, int32_t, int16_t, 2, 2, -)
VEC_OP_DIV4(SUB, int32_t, int16_t, 2, 2, -)
VEC_OP_DIV8(SUB, int32_t, int16_t, 2, 2, -)

VEC_EXPR(AVG, int32_t, int8_t, 1, 4, ((int8_t)(tmp_a[i] + tmp_b[i]) >> (int8_t)1))
VEC_EXPR(AVG, int32_t, int16_t, 2, 2, ((int16_t)(tmp_a[i] + tmp_b[i]) >> (int16_t)1))
VEC_EXPR_SC(AVG, int32_t, int8_t, 1, 4, ((int8_t)(tmp_a[i] + b) >> (int8_t)1))
VEC_EXPR_SC(AVG, int32_t, int16_t, 2, 2, ((int16_t)(tmp_a[i] + b) >> (int16_t)1))

VEC_EXPR(AVGU, uint32_t, uint8_t, 1, 4, ((uint8_t)(tmp_a[i] + tmp_b[i]) >> (uint8_t)1))
VEC_EXPR(AVGU, uint32_t, uint16_t, 2, 2, ((uint16_t)(tmp_a[i] + tmp_b[i]) >> (uint16_t)1))
VEC_EXPR_SC(AVGU, uint32_t, uint8_t, 1, 4, ((uint8_t)(tmp_a[i] + b) >> (uint8_t)1))
VEC_EXPR_SC(AVGU, uint32_t, uint16_t, 2, 2, ((uint16_t)(tmp_a[i] + b) >> (uint16_t)1))

VEC_EXPR(MIN, int32_t, int8_t, 1, 4, (tmp_a[i] > tmp_b[i] ? tmp_b[i] : tmp_a[i]))
VEC_EXPR(MIN, int32_t, int16_t, 2, 2, (tmp_a[i] > tmp_b[i] ? tmp_b[i] : tmp_a[i]))
VEC_EXPR_SC(MIN, int32_t, int8_t, 1, 4, (tmp_a[i] > b ? b : tmp_a[i]))
VEC_EXPR_SC(MIN, int32_t, int16_t, 2, 2, (tmp_a[i] > b ? b : tmp_a[i]))

VEC_EXPR(MINU, uint32_t, uint8_t, 1, 4, (tmp_a[i] > tmp_b[i] ? tmp_b[i] : tmp_a[i]))
VEC_EXPR(MINU, uint32_t, uint16_t, 2, 2, (tmp_a[i] > tmp_b[i] ? tmp_b[i] : tmp_a[i]))
VEC_EXPR_SC(MINU, uint32_t, uint8_t, 1, 4, (tmp_a[i] > b ? b : tmp_a[i]))
VEC_EXPR_SC(MINU, uint32_t, uint16_t, 2, 2, (tmp_a[i] > b ? b : tmp_a[i]))

VEC_EXPR(MAX, int32_t, int8_t, 1, 4, (tmp_a[i] > tmp_b[i] ? tmp_a[i] : tmp_b[i]))
VEC_EXPR(MAX, int32_t, int16_t, 2, 2, (tmp_a[i] > tmp_b[i] ? tmp_a[i] : tmp_b[i]))
VEC_EXPR_SC(MAX, int32_t, int8_t, 1, 4, (tmp_a[i] > b ? tmp_a[i] : b))
VEC_EXPR_SC(MAX, int32_t, int16_t, 2, 2, (tmp_a[i] > b ? tmp_a[i] : b))

VEC_EXPR(MAXU, uint32_t, uint8_t, 1, 4, (tmp_a[i] > tmp_b[i] ? tmp_a[i] : tmp_b[i]))
VEC_EXPR(MAXU, uint32_t, uint16_t, 2, 2, (tmp_a[i] > tmp_b[i] ? tmp_a[i] : tmp_b[i]))
VEC_EXPR_SC(MAXU, uint32_t, uint8_t, 1, 4, (tmp_a[i] > b ? tmp_a[i] : b))
VEC_EXPR_SC(MAXU, uint32_t, uint16_t, 2, 2, (tmp_a[i] > b ? tmp_a[i] : b))

VEC_EXPR(SRL, uint32_t, uint8_t, 1, 4, (tmp_a[i] >> (tmp_b[i] & 0x7)))
VEC_EXPR_SC(SRL, uint32_t, uint8_t, 1, 4, (tmp_a[i] >> (b & 0x7)))
VEC_EXPR(SRL, uint32_t, uint16_t, 1, 2, (tmp_a[i] >> (tmp_b[i] & 0xF)))
VEC_EXPR_SC(SRL, uint32_t, uint16_t, 1, 2, (tmp_a[i] >> (b & 0xF)))

VEC_EXPR(SRA, int32_t, int8_t, 1, 4, (tmp_a[i] >> (tmp_b[i] & 0x7)))
VEC_EXPR_SC(SRA, int32_t, int8_t, 1, 4, (tmp_a[i] >> (b & 0x7)))
VEC_EXPR(SRA, int32_t, int16_t, 1, 2, (tmp_a[i] >> (tmp_b[i] & 0xF)))
VEC_EXPR_SC(SRA, int32_t, int16_t, 1, 2, (tmp_a[i] >> (b & 0xF)))

VEC_EXPR(SLL, uint32_t, uint8_t, 1, 4, (tmp_a[i] << (tmp_b[i] & 0x7)))
VEC_EXPR_SC(SLL, uint32_t, uint8_t, 1, 4, (tmp_a[i] << (b & 0x7)))
VEC_EXPR(SLL, uint32_t, uint16_t, 1, 2, (tmp_a[i] << (tmp_b[i] & 0xF)))
VEC_EXPR_SC(SLL, uint32_t, uint16_t, 1, 2, (tmp_a[i] << (b & 0xF)))

VEC_OP(MUL, int32_t, int8_t, 1, 4, *)
VEC_OP(MUL, int32_t, int16_t, 2, 2, *)

VEC_OP(OR, int32_t, int8_t, 1, 4, |)
VEC_OP(OR, int32_t, int16_t, 2, 2, |)

VEC_OP(XOR, int32_t, int8_t, 1, 4, ^)
VEC_OP(XOR, int32_t, int16_t, 2, 2, ^)

VEC_OP(AND, int32_t, int8_t, 1, 4, &)
VEC_OP(AND, int32_t, int16_t, 2, 2, &)

static inline uint32_t lib_VEC_INS_8(Iss *s, uint32_t a, uint32_t b, uint32_t shift)
{
    shift *= 8;
    uint32_t mask = 0xff << shift;
    uint32_t ins = (b & 0xff) << shift;
    a &= ~mask;
    a |= ins;
    return a;
}

static inline uint32_t lib_VEC_INS_16(Iss *s, uint32_t a, uint32_t b, uint32_t shift)
{
    shift *= 16;
    uint32_t mask = 0xffff << shift;
    uint32_t ins = (b & 0xffff) << shift;
    a &= ~mask;
    a |= ins;
    return a;
}

static inline uint32_t lib_VEC_EXT_8(Iss *s, uint32_t a, uint32_t shift)
{
    shift *= 8;
    uint32_t mask = 0xff << shift;
    a &= mask;
    a >>= shift;
    return (int8_t)a;
}

static inline uint32_t lib_VEC_EXT_16(Iss *s, uint32_t a, uint32_t shift)
{
    shift *= 16;
    uint32_t mask = 0xffff << shift;
    a &= mask;
    a >>= shift;
    return (int16_t)a;
}

static inline uint32_t lib_VEC_EXTU_8(Iss *s, uint32_t a, uint32_t shift)
{
    shift *= 8;
    uint32_t mask = 0xff << shift;
    a &= mask;
    a >>= shift;
    return a;
}

static inline uint32_t lib_VEC_EXTU_16(Iss *s, uint32_t a, uint32_t shift)
{
    shift *= 16;
    uint32_t mask = 0xffff << shift;
    a &= mask;
    a >>= shift;
    return a;
}

static inline int32_t lib_VEC_ABS_int8_t_to_int32_t(Iss *s, uint32_t a)
{
    int8_t *tmp_a = (int8_t *)&a;
    int32_t out;
    int8_t *tmp_out = (int8_t *)&out;
    int i;
    for (i = 0; i < 4; i++)
        tmp_out[i] = tmp_a[i] > 0 ? tmp_a[i] : -tmp_a[i];
    return out;
}

static inline int32_t lib_VEC_ABS_int16_t_to_int32_t(Iss *s, uint32_t a)
{
    int16_t *tmp_a = (int16_t *)&a;
    int32_t out;
    int16_t *tmp_out = (int16_t *)&out;
    int i;
    for (i = 0; i < 2; i++)
        tmp_out[i] = tmp_a[i] > 0 ? tmp_a[i] : -tmp_a[i];
    return out;
}

static inline unsigned int getShuffleHalf(unsigned int a, unsigned int b, unsigned int pos)
{
    unsigned int shift = ((b >> pos) & 1) << 4;
    return ((a >> shift) & 0xffff) << pos;
}

static inline unsigned int lib_VEC_SHUFFLE_16(Iss *s, unsigned int a, unsigned int b)
{
    return getShuffleHalf(a, b, 16) | getShuffleHalf(a, b, 0);
    unsigned int low = b & 1 ? a >> 16 : a & 0xffff;
    unsigned int high = b & (1ULL << 16) ? a >> 16 : a & 0xffff;
    return low | (high << 16);
}

static inline unsigned int getShuffleHalfSci(unsigned int a, unsigned int b, unsigned int pos)
{
    unsigned int bitPos = pos >> 4;
    unsigned int shift = ((b >> bitPos) & 1) << 4;
    return ((a >> shift) & 0xffff) << pos;
}

static inline unsigned int lib_VEC_SHUFFLE_SCI_16(Iss *s, unsigned int a, unsigned int b)
{
    return getShuffleHalfSci(a, b, 16) | getShuffleHalfSci(a, b, 0);
}

static inline unsigned int getShuffleByte(unsigned int a, unsigned int b, unsigned int pos)
{
    unsigned int shift = ((b >> pos) & 0x3) << 3;
    return ((a >> shift) & 0xff) << pos;
}

static inline unsigned int lib_VEC_SHUFFLE_8(Iss *s, unsigned int a, unsigned int b)
{
    return getShuffleByte(a, b, 24) | getShuffleByte(a, b, 16) | getShuffleByte(a, b, 8) | getShuffleByte(a, b, 0);
}

static inline unsigned int getShuffleByteSci(unsigned int a, unsigned int b, unsigned int pos)
{
    unsigned int bitPos = pos >> 2;
    unsigned int shift = ((b >> bitPos) & 0x3) << 3;
    return ((a >> shift) & 0xff) << pos;
}

static inline unsigned int lib_VEC_SHUFFLE_SCI_8(Iss *s, unsigned int a, unsigned int b)
{
    return getShuffleByteSci(a, b, 24) | getShuffleByteSci(a, b, 16) | getShuffleByteSci(a, b, 8) | getShuffleByteSci(a, b, 0);
}

static inline unsigned int lib_VEC_SHUFFLEI0_SCI_8(Iss *s, unsigned int a, unsigned int b)
{
    return (((a >> 0) & 0xff) << 24) | getShuffleByteSci(a, b, 16) | getShuffleByteSci(a, b, 8) | getShuffleByteSci(a, b, 0);
}

static inline unsigned int lib_VEC_SHUFFLEI1_SCI_8(Iss *s, unsigned int a, unsigned int b)
{
    return (((a >> 8) & 0xff) << 24) | getShuffleByteSci(a, b, 16) | getShuffleByteSci(a, b, 8) | getShuffleByteSci(a, b, 0);
}

static inline unsigned int lib_VEC_SHUFFLEI2_SCI_8(Iss *s, unsigned int a, unsigned int b)
{
    return (((a >> 16) & 0xff) << 24) | getShuffleByteSci(a, b, 16) | getShuffleByteSci(a, b, 8) | getShuffleByteSci(a, b, 0);
}

static inline unsigned int lib_VEC_SHUFFLEI3_SCI_8(Iss *s, unsigned int a, unsigned int b)
{
    return (((a >> 24) & 0xff) << 24) | getShuffleByteSci(a, b, 16) | getShuffleByteSci(a, b, 8) | getShuffleByteSci(a, b, 0);
}

static inline unsigned int lib_VEC_SHUFFLE2_16(Iss *s, unsigned int a, unsigned int b, unsigned int c)
{
#ifdef RISCV
    return getShuffleHalf(b & (1ULL << 17) ? a : c, b, 16) | getShuffleHalf(b & (1ULL << 1) ? a : c, b, 0);
#else
    return getShuffleHalf(b & (1ULL << 17) ? c : a, b, 16) | getShuffleHalf(b & (1ULL << 1) ? c : a, b, 0);
#endif
}

static inline unsigned int lib_VEC_SHUFFLE2_8(Iss *s, unsigned int a, unsigned int b, unsigned int c)
{
#ifdef RISCV
    return getShuffleByte(b & (1ULL << 26) ? a : c, b, 24) | getShuffleByte(b & (1ULL << 18) ? a : c, b, 16) | getShuffleByte(b & (1ULL << 10) ? a : c, b, 8) | getShuffleByte(b & (1ULL << 2) ? a : c, b, 0);
#else
    return getShuffleByte(b & (1ULL << 26) ? c : a, b, 24) | getShuffleByte(b & (1ULL << 18) ? c : a, b, 16) | getShuffleByte(b & (1ULL << 10) ? c : a, b, 8) | getShuffleByte(b & (1ULL << 2) ? c : a, b, 0);
#endif
}

static inline unsigned int lib_VEC_PACK_SC_16(Iss *s, unsigned int a, unsigned int b)
{
    return ((a & 0xffff) << 16) | (b & 0xffff);
}

static inline unsigned int lib_VEC_PACKHI_SC_8(Iss *s, unsigned int a, unsigned int b, unsigned c)
{
    return ((a & 0xff) << 24) | ((b & 0xff) << 16) | (c & 0xffff);
}

static inline unsigned int lib_VEC_PACKLO_SC_8(Iss *s, unsigned int a, unsigned int b, unsigned c)
{
    return ((a & 0xff) << 8) | ((b & 0xff) << 0) | (c & 0xffff0000);
}

#define VEC_DOTP(operName, typeOut, typeA, typeB, elemTypeA, elemTypeB, elemSize, num_elem, oper)  \
    static inline typeOut lib_VEC_##operName##_##elemSize(Iss *s, typeA a, typeB b)    \
    {                                                                                              \
        elemTypeA *tmp_a = (elemTypeA *)&a;                                                        \
        elemTypeB *tmp_b = (elemTypeB *)&b;                                                        \
        typeOut out = 0;                                                                           \
        int i;                                                                                     \
        for (i = 0; i < num_elem; i++)                                                             \
        {                                                                                          \
            out += tmp_a[i] oper tmp_b[i];                                                         \
        }                                                                                          \
        return out;                                                                                \
    }                                                                                              \
                                                                                                   \
    static inline typeOut lib_VEC_##operName##_SC_##elemSize(Iss *s, typeA a, typeB b) \
    {                                                                                              \
        elemTypeA *tmp_a = (elemTypeA *)&a;                                                        \
        elemTypeB *tmp_b = (elemTypeB *)&b;                                                        \
        typeOut out = 0;                                                                           \
        int i;                                                                                     \
        for (i = 0; i < num_elem; i++)                                                             \
            out += tmp_a[i] oper tmp_b[0];                                                         \
        return out;                                                                                \
    }

VEC_DOTP(DOTSP, int32_t, int32_t, int32_t, int16_t, int16_t, 16, 2, *)
VEC_DOTP(DOTSP, int32_t, int32_t, int32_t, int8_t, int8_t, 8, 4, *)

VEC_DOTP(DOTUP, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, 16, 2, *)
VEC_DOTP(DOTUP, uint32_t, uint32_t, uint32_t, uint8_t, uint8_t, 8, 4, *)

VEC_DOTP(DOTUSP, int32_t, uint32_t, int32_t, uint16_t, int16_t, 16, 2, *)
VEC_DOTP(DOTUSP, int32_t, uint32_t, int32_t, uint8_t, int8_t, 8, 4, *)

#define VEC_SDOT(operName, typeOut, typeA, typeB, elemTypeA, elemTypeB, elemSize, num_elem, oper)               \
    static inline typeOut lib_VEC_##operName##_##elemSize(Iss *s, typeOut out, typeA a, typeB b)    \
    {                                                                                                           \
        elemTypeA *tmp_a = (elemTypeA *)&a;                                                                     \
        elemTypeB *tmp_b = (elemTypeB *)&b;                                                                     \
        int i;                                                                                                  \
        __asm__ __volatile__(""                                                                                 \
                             :                                                                                  \
                             :                                                                                  \
                             : "memory");                                                                       \
        for (i = 0; i < num_elem; i++)                                                                          \
            out += tmp_a[i] oper tmp_b[i];                                                                      \
        return out;                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline typeOut lib_VEC_##operName##_SC_##elemSize(Iss *s, typeOut out, typeA a, typeB b) \
    {                                                                                                           \
        elemTypeA *tmp_a = (elemTypeA *)&a;                                                                     \
        elemTypeB *tmp_b = (elemTypeB *)&b;                                                                     \
        int i;                                                                                                  \
        __asm__ __volatile__(""                                                                                 \
                             :                                                                                  \
                             :                                                                                  \
                             : "memory");                                                                       \
        for (i = 0; i < num_elem; i++)                                                                          \
            out += tmp_a[i] oper tmp_b[0];                                                                      \
        return out;                                                                                             \
    }

VEC_SDOT(SDOTSP, int32_t, int32_t, int32_t, int16_t, int16_t, 16, 2, *)
VEC_SDOT(SDOTSP, int32_t, int32_t, int32_t, int8_t, int8_t, 8, 4, *)

VEC_SDOT(SDOTUP, uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, 16, 2, *)
VEC_SDOT(SDOTUP, uint32_t, uint32_t, uint32_t, uint8_t, uint8_t, 8, 4, *)

VEC_SDOT(SDOTUSP, int32_t, uint32_t, int32_t, uint16_t, int16_t, 16, 2, *)
VEC_SDOT(SDOTUSP, int32_t, uint32_t, int32_t, uint8_t, int8_t, 8, 4, *)

/*
 *  HW LOOPS
 */

#if 0
static inline pc_t *handleHwLoopStub(cpu_t *cpu, pc_t *pc, int index)
{
  // This stub is here to check at the end of a HW loop if we must jump to
  // the start of the loop
  cpu->hwLoopCounter[index]--;
  if (cpu->hwLoopCounter[index] == 0) {
    return NULL;
  } else {
    return cpu->hwLoopStartPc[index];
  }
}

static inline pc_t *lib_hwLoopStub0(cpu_t *cpu, pc_t *pc)
{
  return handleHwLoopStub(cpu, pc, 0);
}

static inline pc_t *lib_hwLoopStub1(cpu_t *cpu, pc_t *pc)
{
  return handleHwLoopStub(cpu, pc, 1);
}

static inline void lib_hwLoopSetCount(cpu_t *cpu, pc_t *pc, unsigned int index, unsigned int count)
{
  cpu->hwLoopCounter[index] = count;
}

static inline pc_t *lib_hwLoopSetEnd(cpu_t *cpu, pc_t *pc, unsigned int index, unsigned int addr)
{
  // A PC stub is inserted at the end of the HW loop in order to check HW loop count
  // and jump to beginning of loop or continue
  // No need to clean-up pending stubs as they are shared between all the cores
  if (cpu->hwLoopEndPc[index] != NULL)
  {
    destroyPcStub(cpu, cpu->hwLoopEndPc[index]);
    cpu->hwLoopEndPc[index] = NULL;
  }

  if (index == 0) cpu->hwLoopEndPc[index] = createPcCpuStub(cpu, getPc(cpu, addr), lib_hwLoopStub0, CPU_HANDLER_HWLOOP0);
  else cpu->hwLoopEndPc[index] = createPcCpuStub(cpu, getPc(cpu, addr), lib_hwLoopStub1, CPU_HANDLER_HWLOOP1);

  return cpu->hwLoopEndPc[index];
}

static inline void lib_hwLoopSetStart(cpu_t *cpu, pc_t *pc, unsigned int index, unsigned int addr)
{
  cpu->hwLoopStartPc[index] = getPc(cpu, addr);
}
#endif

// Add/Sub with normalization and rounding

static inline unsigned int lib_ADD_NR(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return ((int32_t)(a + b)) >> shift;
}

static inline unsigned int lib_ADD_NRU(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return ((uint32_t)(a + b)) >> shift;
}

static inline unsigned int lib_ADD_NL(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return (a + b) << shift;
}

static inline unsigned int lib_ADD_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    if (shift > 0)
        return ((int32_t)(a + b + (1ULL << (shift - 1)))) >> shift;
    else
        return (int32_t)(a + b);
}

static inline unsigned int lib_ADD_NR_RU(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    if (shift > 0)
        return ((uint32_t)(a + b + (1ULL << (shift - 1)))) >> shift;
    else
        return (uint32_t)(a + b);
}

static inline unsigned int lib_SUB_NR(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return ((int32_t)(a - b)) >> shift;
}

static inline unsigned int lib_SUB_NRU(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return ((uint32_t)(a - b)) >> shift;
}

static inline unsigned int lib_SUB_NL(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    return (a - b) << shift;
}

static inline unsigned int lib_SUB_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    if (shift > 0)
        return ((int32_t)(a - b + (1ULL << (shift - 1)))) >> shift;
    else
        return (int32_t)(a - b);
}

static inline unsigned int lib_SUB_NR_RU(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    shift &= 0x1f;
    if (shift > 0)
        return ((uint32_t)(a - b + (1ULL << (shift - 1)))) >> shift;
    else
        return (uint32_t)(a - b);
}

// Combined normalization and rounding
static inline int lib_MULS_NL(Iss *s, int a, int b, unsigned int shift)
{
    return (a * b) << shift;
}

static inline unsigned int lib_MULU_NL(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    return (a * b) << shift;
}

static inline unsigned int lib_MULUS_NL(Iss *s, unsigned int a, int b, unsigned int shift)
{
    return (a * b) << shift;
}

static inline int lib_MULS_NR(Iss *s, int a, int b, unsigned int shift)
{
    return ((int32_t)(a * b)) >> shift;
}

static inline unsigned int lib_MULU_NR(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    return ((uint32_t)(a * b)) >> shift;
}

static inline unsigned int lib_MULUS_NR(Iss *s, unsigned int a, int b, unsigned int shift)
{
    return ((int32_t)(a * b)) >> shift;
}

static inline int lib_MULS_NR_R(Iss *s, int a, int b, unsigned int shift)
{
    if (shift > 0)
        return ((int32_t)(a * b + (1ULL << (shift - 1)))) >> shift;
    else
        return (int32_t)(a * b);
}

static inline unsigned int lib_MULU_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int shift)
{
    if (shift > 0)
        return ((uint32_t)(a * b + (1ULL << (shift - 1)))) >> shift;
    else
        return (uint32_t)(a * b);
}

static inline int lib_MULUS_NR_R(Iss *s, unsigned int a, int b, unsigned int shift)
{
    unsigned int roundValue = shift == 0 ? 0 : 1ULL << (shift - 1);
    return ((int32_t)(a * b + roundValue)) >> shift;
}

static inline int lib_MACS_NL(Iss *s, int a, int b, int c, unsigned int shift)
{
    return (a + (b * c)) << shift;
}

static inline unsigned int lib_MACU_NL(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    return (a + (b * c)) << shift;
}

static inline unsigned int lib_MACUS_NL(Iss *s, int a, unsigned int b, int c, unsigned int shift)
{
    return (a + (b * c)) << shift;
}

static inline int lib_MACS_NR(Iss *s, int a, int b, int c, unsigned int shift)
{
    return ((int32_t)(a + b * c)) >> shift;
}

static inline unsigned int lib_MACU_NR(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    return ((uint32_t)(a + b * c)) >> shift;
}

static inline unsigned int lib_MACUS_NR(Iss *s, int a, unsigned int b, int c, unsigned int shift)
{
    return ((int32_t)(a + b * c)) >> shift;
}

static inline int lib_MACS_NR_R(Iss *s, int a, int b, int c, unsigned int shift)
{
    if (shift > 0)
        return ((int32_t)(a + b * c + (1ULL << (shift - 1)))) >> shift;
    else
        return (int32_t)(a + b * c);
}

static inline unsigned int lib_MACU_NR_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, unsigned int shift)
{
    if (shift > 0)
        return ((uint32_t)(a + b * c + (1ULL << (shift - 1)))) >> shift;
    else
        return (uint32_t)(a + b * c);
}

static inline unsigned int lib_MACUS_NR_R(Iss *s, int a, unsigned int b, int c, unsigned int shift)
{
    if (shift > 0)
        return ((int32_t)(a + b * c + (1ULL << (shift - 1)))) >> shift;
    else
        return (int32_t)(a + b * c);
}

// Clipping
static inline unsigned int lib_CLIP(Iss *s, int a, int low, int high)
{
    unsigned int result;

    if (a < low)
        result = low;
    else if (a > high)
        result = high;
    else
        result = a;

    return result;
}

static inline unsigned int lib_CLIPU(Iss *s, int a, int high)
{
    unsigned int result;

    if (a > high)
        result = high;
    else if (a <= 0)
        result = 0;
    else
        result = a;

    return result;
}

// Complex numbers
static inline unsigned int lib_CPLXMUL_H_I(Iss *s, unsigned int a, unsigned int b, unsigned int c, int div)
{
    long long a_imm = (long long)(int16_t)(a >> 16), a_re = (long long)(int16_t)(a & 0xffff);
    long long b_imm = (long long)(int16_t)(b >> 16), b_re = (long long)(int16_t)(b & 0xffff);
    // printf("a = [Re= %5d, Im= %5d], b = [Re= %5d, Im= %5d]\n", (int) a_re, (int) a_imm, (int) b_re, (int) b_imm);
    return (((uint16_t)((a_imm * b_re + a_re * b_imm) >> (15 + div)) << 16)) | (c & 0xffff);
}

static inline unsigned int lib_CPLXMUL_H_R(Iss *s, unsigned int a, unsigned int b, unsigned int c, int div)
{
    long long a_imm = (long long)(int16_t)(a >> 16), a_re = (long long)(int16_t)(a & 0xffff);
    long long b_imm = (long long)(int16_t)(b >> 16), b_re = (long long)(int16_t)(b & 0xffff);
    // printf("a = [Re= %5d, Im= %5d], b = [Re= %5d, Im= %5d]\n", (int) a_re, (int) a_imm, (int) b_re, (int) b_imm);
    return (c & 0xffff0000) | (uint16_t)((a_re * b_re - a_imm * b_imm) >> (15 + div));
}

static inline unsigned int lib_CPLXMULS(Iss *s, unsigned int a, unsigned int b)
{
    long long a_imm = (long long)(int16_t)(a >> 16), a_re = (long long)(int16_t)(a & 0xffff);
    long long b_imm = (long long)(int16_t)(b >> 16), b_re = (long long)(int16_t)(b & 0xffff);
    // printf("a = [Re= %5d, Im= %5d], b = [Re= %5d, Im= %5d]\n", (int) a_re, (int) a_imm, (int) b_re, (int) b_imm);
    return (((uint16_t)((a_imm * b_re + a_re * b_imm) >> 15)) << 16) | (uint16_t)((a_re * b_re - a_imm * b_imm) >> 15);
}

static inline unsigned int lib_CPLXMULS_DIV2(Iss *s, unsigned int a, unsigned int b)
{
    long long a_imm = (long long)(int16_t)(a >> 16), a_re = (long long)(int16_t)(a & 0xffff);
    long long b_imm = (long long)(int16_t)(b >> 16), b_re = (long long)(int16_t)(b & 0xffff);
    return (((uint16_t)((a_imm * b_re + a_re * b_imm) >> 16)) << 16) | (uint16_t)((a_re * b_re - a_imm * b_imm) >> 16);
}

static inline unsigned int lib_CPLXMULS_DIV4(Iss *s, unsigned int a, unsigned int b)
{
    long long a_imm = (long long)(int16_t)(a >> 16), a_re = (long long)(int16_t)(a & 0xffff);
    long long b_imm = (long long)(int16_t)(b >> 16), b_re = (long long)(int16_t)(b & 0xffff);
    return (((uint16_t)((a_imm * b_re + a_re * b_imm) >> 17)) << 16) | (uint16_t)((a_re * b_re - a_imm * b_imm) >> 17);
}

static inline unsigned int lib_CPLXMULS_SC(Iss *s, unsigned int a, int b)
{
    int a_imm = (int)(int16_t)(a >> 16), a_re = (int)(int16_t)(a & 0xffff);
    return (((uint16_t)((a_imm * b + a_re * b) >> 15)) << 16) | ((a_re * b - a_imm * b) >> 15);
}

static inline unsigned int lib_CPLX_CONJ_16(Iss *s, unsigned int a)
{
    int a_imm = (int)(int16_t)(a >> 16), a_re = (int)(int16_t)(a & 0xffff);
    return (((uint16_t)(-a_imm)) << 16) | ((uint16_t)a_re);
}

static inline unsigned int lib_VEC_ADD_16_ROTMJ(Iss *s, unsigned int a, int b)
{
    int16_t a_imm = (int16_t)(a >> 16), a_re = (int16_t)(a & 0xffff),
            b_imm = (int16_t)(b >> 16), b_re = (int16_t)(b & 0xffff);
    return (((int16_t)(b_re - a_re)) << 16) | ((a_imm - b_imm) & 0x0ffff);
}

static inline unsigned int lib_VEC_ADD_16_ROTMJ_DIV2(Iss *s, unsigned int a, int b)
{
    int16_t a_imm = (int16_t)(a >> 16), a_re = (int16_t)(a & 0xffff),
            b_imm = (int16_t)(b >> 16), b_re = (int16_t)(b & 0xffff);
    return (((int16_t)(b_re - a_re) >> 1) << 16) | (((int16_t)(a_imm - b_imm) >> 1) & 0x0ffff);
}

static inline unsigned int lib_VEC_ADD_16_ROTMJ_DIV4(Iss *s, unsigned int a, int b)
{
    int16_t a_imm = (int16_t)(a >> 16), a_re = (int16_t)(a & 0xffff),
            b_imm = (int16_t)(b >> 16), b_re = (int16_t)(b & 0xffff);
    return (((int16_t)(b_re - a_re) >> 2) << 16) | (((int16_t)(a_imm - b_imm) >> 2) & 0x0ffff);
}

static inline unsigned int lib_VEC_ADD_16_ROTMJ_DIV8(Iss *s, unsigned int a, int b)
{
    int16_t a_imm = (int16_t)(a >> 16), a_re = (int16_t)(a & 0xffff),
            b_imm = (int16_t)(b >> 16), b_re = (int16_t)(b & 0xffff);
    return (((int16_t)(b_re - a_re) >> 3) << 16) | (((int16_t)(a_imm - b_imm) >> 3) & 0x0ffff);
}

static inline unsigned int lib_BITREV(Iss *s, unsigned int input, unsigned int points, unsigned int radix)
{
    points = 32 - points;
    unsigned int mask = (1ULL << radix) - 1;
    unsigned int input_reverse = input;

    for (int i = 1; i < points / radix; i++)
    {
        input >>= radix;
        input_reverse <<= radix;
        input_reverse |= (input & mask);
    }

    if (points < 32)
    {
        mask = (1ULL << points) - 1;
        input_reverse &= mask;
    }

    return input_reverse;
}

static inline unsigned int lib_VEC_PACK_SC_H_16(Iss *s, unsigned int a, unsigned int b)
{
    return (b >> 16) | (a & 0xffff0000);
}

static inline unsigned int lib_VEC_PACK_SC_HL_16(Iss *s, unsigned int a, unsigned int b)
{
    return (b & 0xffff) | (a << 16);
}

// Floating-Point Emulation

#include "cpu/iss/flexfloat/flexfloat.h"
#include <stdint.h>
#include <math.h>
#include <fenv.h>
#pragma STDC FENV_ACCESS ON

#define FF_INIT_1(a, e, m)                           \
    flexfloat_t ff_a, ff_res;                        \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);

#define FF_INIT_2(a, b, e, m)                        \
    flexfloat_t ff_a, ff_b, ff_res;                  \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);

#define FF_INIT_3(a, b, c, e, m)                     \
    flexfloat_t ff_a, ff_b, ff_c, ff_res;            \
    flexfloat_desc_t env = (flexfloat_desc_t){e, m}; \
    ff_init(&ff_a, env);                             \
    ff_init(&ff_b, env);                             \
    ff_init(&ff_c, env);                             \
    ff_init(&ff_res, env);                           \
    flexfloat_set_bits(&ff_a, a);                    \
    flexfloat_set_bits(&ff_b, b);                    \
    flexfloat_set_bits(&ff_c, c);

#define FF_EXEC_1(s, name, a, e, m) \
    FF_INIT_(a, e, m)               \
    feclearexcept(FE_ALL_EXCEPT);   \
    name(&ff_res, &ff_a);           \
    update_fflags_fenv(s);          \
    return flexfloat_get_bits(&ff_res);

#define FF_EXEC_2(s, name, a, b, e, m) \
    FF_INIT_2(a, b, e, m)              \
    feclearexcept(FE_ALL_EXCEPT);      \
    name(&ff_res, &ff_a, &ff_b);       \
    update_fflags_fenv(s);             \
    return flexfloat_get_bits(&ff_res);

#define FF_EXEC_3(s, name, a, b, c, e, m) \
    FF_INIT_3(a, b, c, e, m)              \
    feclearexcept(FE_ALL_EXCEPT);         \
    name(&ff_res, &ff_a, &ff_b, &ff_c);   \
    update_fflags_fenv(s);                \
    return flexfloat_get_bits(&ff_res);

static inline void set_fflags(Iss *iss, unsigned long int fflags)
{
    iss->csr.fcsr.fflags |= fflags;
}

static inline void clear_fflags(Iss *iss, unsigned long int fflags)
{
    iss->csr.fcsr.fflags &= ~fflags;
}

// updates the fflags from fenv exceptions
static inline void update_fflags_fenv(Iss *iss)
{
    int ex = fetestexcept(FE_ALL_EXCEPT);
    int flags = !!(ex & FE_INEXACT) |
                !!(ex & FE_UNDERFLOW) << 1 |
                !!(ex & FE_OVERFLOW) << 2 |
                !!(ex & FE_DIVBYZERO) << 3 |
                !!(ex & FE_INVALID) << 4;
    set_fflags(iss, flags);
}

// Inspired by https://stackoverflow.com/a/38470183
// TODO PROPER ROUNDING WITH FLAGS
static inline int32_t double_to_int(Iss *s, double dbl_i)
{
    double dbl = nearbyint(dbl_i);

    if (dbl != dbl_i)
    {
        set_fflags(s, 1ULL << 0);
    }

    if (dbl < 2.0 * (INT32_MAX / 2 + 1))
    {                               // NO OVERFLOW
        if (ceil(dbl) >= INT32_MIN) // NO UNDERFLOW
            return (int32_t)dbl;
        else // UNDERFLOW
        {
            set_fflags(s, 1ULL << 4);
            return INT32_MIN;
        }
    }
    else
    { // OVERFLOW OR NAN
        set_fflags(s, 1ULL << 4);
        return INT32_MAX;
    }
}

// TODO PROPER ROUNDING WITH FLAGS
static inline uint32_t double_to_uint(Iss *s, double dbl_i)
{
    double dbl = nearbyint(dbl_i);

    if (dbl != dbl_i)
    {
        set_fflags(s, 1ULL << 0);
    }

    if (dbl < 2.0 * (UINT32_MAX / 2 + 1))
    {                       // NO OVERFLOW
        if (ceil(dbl) >= 0) // NO UNDERFLOW
            return (uint32_t)dbl;
        else // UNDERFLOW
        {
            set_fflags(s, 1ULL << 4);
            return 0;
        }
    }
    else
    { // OVERFLOW OR NAN
        set_fflags(s, 1ULL << 4);
        return UINT32_MAX;
    }
}

// TODO PROPER ROUNDING WITH FLAGS
static inline int64_t double_to_long(Iss *s, double dbl_i)
{
    double dbl = nearbyint(dbl_i);

    if (dbl != dbl_i)
    {
        set_fflags(s, 1ULL << 0);
    }

    if (dbl < 2.0 * (INT64_MAX / 2 + 1))
    {                               // NO OVERFLOW
        if (ceil(dbl) >= INT64_MIN) // NO UNDERFLOW
            return (int64_t)dbl;
        else // UNDERFLOW
        {
            set_fflags(s, 1ULL << 4);
            return INT64_MIN;
        }
    }
    else
    { // OVERFLOW OR NAN
        set_fflags(s, 1ULL << 4);
        return INT64_MAX;
    }
}

// TODO PROPER ROUNDING WITH FLAGS
static inline uint64_t double_to_ulong(Iss *s, double dbl_i)
{
    double dbl = nearbyint(dbl_i);

    if (dbl != dbl_i)
    {
        set_fflags(s, 1ULL << 0);
    }

    if (dbl < 2.0 * (UINT64_MAX / 2 + 1))
    {                       // NO OVERFLOW
        if (ceil(dbl) >= 0) // NO UNDERFLOW
            return (uint64_t)dbl;
        else // UNDERFLOW
        {
            set_fflags(s, 1ULL << 4);
            return 0;
        }
    }
    else
    { // OVERFLOW OR NAN
        set_fflags(s, 1ULL << 4);
        return UINT64_MAX;
    }
}

static inline unsigned long int lib_flexfloat_add(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    FF_EXEC_2(s, ff_add, a, b, e, m)
}

static inline unsigned long int lib_flexfloat_sub(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    FF_EXEC_2(s, ff_sub, a, b, e, m)
}

static inline unsigned long int lib_flexfloat_mul(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    FF_EXEC_2(s, ff_mul, a, b, e, m)
}

static inline unsigned long int lib_flexfloat_div(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    FF_EXEC_2(s, ff_div, a, b, e, m)
}

static inline unsigned long int lib_flexfloat_avg(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    FF_INIT_2(a, b, e, m)
    flexfloat_t ff_two;
    ff_init_int(&ff_two, 2, (flexfloat_desc_t){e, m});
    feclearexcept(FE_ALL_EXCEPT);
    ff_add(&ff_res, &ff_a, &ff_b);
    ff_div(&ff_res, &ff_res, &ff_two);
    update_fflags_fenv(s);
    return flexfloat_get_bits(&ff_res);
}

// TODO proper flags
static inline unsigned long int lib_flexfloat_itof(Iss *s, unsigned long int a, uint8_t e, uint8_t m)
{
    flexfloat_t ff_a;
    feclearexcept(FE_ALL_EXCEPT);
    ff_init_int(&ff_a, a, (flexfloat_desc_t){e, m});
    update_fflags_fenv(s);
    return flexfloat_get_bits(&ff_a);
}

// TODO proper flags
static inline unsigned long int lib_flexfloat_ftoi(Iss *s, unsigned long int a, uint8_t e, uint8_t m)
{
    FF_INIT_1(a, e, m)
    return (int)ff_a.value;
}

// static inline unsigned long int lib_flexfloat_rem(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m) {
//   FF_INIT_2(a, b, e, m)
// }

static inline unsigned long int lib_flexfloat_madd(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m)
{
    FF_EXEC_3(s, ff_fma, a, b, c, e, m)
}

static inline unsigned long int lib_flexfloat_msub(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m)
{
    FF_INIT_3(a, b, c, e, m)
    ff_inverse(&ff_c, &ff_c);
    feclearexcept(FE_ALL_EXCEPT);
    ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
    update_fflags_fenv(s);
    return flexfloat_get_bits(&ff_res);
}

static inline unsigned long int lib_flexfloat_nmsub(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m)
{
    FF_INIT_3(a, b, c, e, m)
    ff_inverse(&ff_a, &ff_a);
    feclearexcept(FE_ALL_EXCEPT);
    ff_fma(&ff_res, &ff_a, &ff_b, &ff_c);
    update_fflags_fenv(s);
    return flexfloat_get_bits(&ff_res);
}

static inline unsigned long int lib_flexfloat_nmadd(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m)
{
    FF_INIT_3(a, b, c, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    ff_fnma(&ff_res, &ff_a, &ff_b, &ff_c);
    update_fflags_fenv(s);
    return flexfloat_get_bits(&ff_res);
}

static inline unsigned long int setFFRoundingMode(Iss *s, unsigned long int mode)
{
    int old = fegetround();
    switch (mode)
    {
    case 0:
        fesetround(FE_TONEAREST);
        break;
    case 1:
        fesetround(FE_TOWARDZERO);
        break;
    case 2:
        fesetround(FE_DOWNWARD);
        break;
    case 3:
        fesetround(FE_UPWARD);
        break;
    case 4:
        printf("Unimplemented roudning mode nearest ties to max magnitude");
        exit(-1);
        break;
    case 7:
    {
        switch (s->csr.fcsr.frm)
        {
        case 0:
            fesetround(FE_TONEAREST);
            break;
        case 1:
            fesetround(FE_TOWARDZERO);
            break;
        case 2:
            fesetround(FE_DOWNWARD);
            break;
        case 3:
            fesetround(FE_UPWARD);
            break;
        case 4:
            printf("Unimplemented roudning mode nearest ties to max magnitude");
            exit(-1);
            break;
        }
    }
    }
    return old;
}

static inline void restoreFFRoundingMode(unsigned long int mode)
{
    fesetround(mode);
}

static inline unsigned long int lib_flexfloat_madd_round(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_madd(s, a, b, c, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_msub_round(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_msub(s, a, b, c, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_nmadd_round(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_nmadd(s, a, b, c, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_nmsub_round(Iss *s, unsigned long int a, unsigned long int b, unsigned long int c, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_nmsub(s, a, b, c, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_add_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_add(s, a, b, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_sub_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_sub(s, a, b, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_mul_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_mul(s, a, b, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_div_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_div(s, a, b, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_avg_round(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    unsigned long int result = lib_flexfloat_avg(s, a, b, e, m);
    restoreFFRoundingMode(old);
    return result;
}

static inline unsigned long int lib_flexfloat_sqrt_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    ff_init_double(&ff_res, sqrt(ff_get_double(&ff_a)), env);
    update_fflags_fenv(s);
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_res);
}

static inline unsigned long int lib_flexfloat_sgnj(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
#ifdef OLD
    FF_INIT_2(a, b, e, m)
    CAST_TO_INT(ff_res.value) = flexfloat_pack(env, flexfloat_sign(&ff_b), flexfloat_exp(&ff_a), flexfloat_frac(&ff_a));
    return flexfloat_get_bits(&ff_res);
#else
    unsigned long int S_b = (b >> (m + e)) & 0x1;
    unsigned long int R = (a & ((1ULL << (m + e)) - 1)) | ((S_b) << (m + e));
    return DoExtend(R, e, m);
#endif
}

static inline unsigned long int lib_flexfloat_sgnjn(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
#ifdef OLD
    FF_INIT_2(a, b, e, m)
    CAST_TO_INT(ff_res.value) = flexfloat_pack(env, !flexfloat_sign(&ff_b), flexfloat_exp(&ff_a), flexfloat_frac(&ff_a));
    return flexfloat_get_bits(&ff_res);
#else
    unsigned long int S_b = (b >> (m + e)) & 0x1;
    unsigned long int R = (a & ((1ULL << (m + e)) - 1)) | (((unsigned long int)!S_b) << (m + e));
    return DoExtend(R, e, m);
#endif
}

static inline unsigned long int lib_flexfloat_sgnjx(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
#ifdef OLD
    FF_INIT_2(a, b, e, m)
    CAST_TO_INT(ff_res.value) = flexfloat_pack(env, flexfloat_sign(&ff_a) ^ flexfloat_sign(&ff_b), flexfloat_exp(&ff_a), flexfloat_frac(&ff_a));
    return flexfloat_get_bits(&ff_res);
#else
    unsigned long int S_a = (a >> (m + e)) & 0x1;
    unsigned long int S_b = (b >> (m + e)) & 0x1;
    unsigned long int R = (a & ((1ULL << (m + e)) - 1)) | (((unsigned long int)(S_a ^ S_b)) << (m + e));
    return DoExtend(R, e, m);
#endif
}

#ifndef OLD
static int IsNan(unsigned long int X, uint8_t e, uint8_t m) // NEW FUNCTION

{
    unsigned long int E = (X >> m) & ((1ULL << e) - 1);
    unsigned long int M = (X) & ((1ULL << m) - 1);

    if ((E == ((1ULL << e) - 1)) && (M != 0))
    {
        if ((M >> (m - 1)) & 0x1)
            return 1; // Nan quiet
        else
            return 2; // Nam signaling
    }
    else
        return 0;
    return ((E == ((1ULL << e) - 1)) && (M != 0));
}
#endif

// TODO proper nan handling
static inline unsigned long int lib_flexfloat_min(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
#ifdef OLD
    FF_EXEC_2(s, ff_min, a, b, e, m)
#else
    int Nan_a = IsNan(a, e, m);
    int Nan_b = IsNan(b, e, m);
    unsigned long int Nan_Q = (((1ULL << e) - 1) << m) | ((unsigned long int)1ULL << (m - 1));
    unsigned long int Nan_S = (((1ULL << e) - 1) << m) | ((unsigned long int)1ULL << (m - 2));

    if (Nan_a && Nan_b)
    {
        if (Nan_a == 2 || Nan_b == 2)
            return Nan_Q;
        else
            return Nan_Q;
    }
    else if (Nan_a)
        return b;
    else if (Nan_b)
        return a;
    FF_EXEC_2(s, ff_min, a, b, e, m);
#endif
}

// TODO proper NaN handling
static inline unsigned long int lib_flexfloat_max(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
#ifdef OLD
    FF_EXEC_2(s, ff_max, a, b, e, m)
#else
    // The invalid flag is set if either input is a signaling nan
    if (IsNan(a, e, m) == 2 || IsNan(b, e, m) == 2)
    {
        set_fflags(s, 1ULL << 4);
    }
    int Nan_a = IsNan(a, e, m);
    int Nan_b = IsNan(b, e, m);
    unsigned long int Nan_Q = (((1ULL << e) - 1) << m) | ((unsigned long int)1ULL << (m - 1));
    unsigned long int Nan_S = (((1ULL << e) - 1) << m) | ((unsigned long int)1ULL << (m - 2));

    if (Nan_a && Nan_b)
    {
        if (Nan_a == 2 || Nan_b == 2)
            return Nan_Q;
        else
            return Nan_Q;
    }
    else if (Nan_a)
        return b;
    else if (Nan_b)
        return a;
    FF_EXEC_2(s, ff_max, a, b, e, m);
#endif
}

static inline int64_t lib_flexfloat_cvt_w_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old;
    bool neg = false;
    unsigned long int new_round = round == 4 ? 2 : round;
    old = setFFRoundingMode(s, new_round);
    FF_INIT_1(a, e, m)
    if (round == 4)
    {
        if (ff_a.value < 0)
        {
            ff_a.value = -ff_a.value;
            neg = true;
        }
        ff_a.value += 0.5f;
    }
    int32_t result_int = double_to_int(s, ff_a.value);
    if (neg)
    {
        result_int = -result_int;
    }
    restoreFFRoundingMode(new_round);
    return iss_get_signed_value(result_int, 32);
}

static inline int64_t lib_flexfloat_cvt_wu_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old;
    bool neg = false;
    unsigned long int new_round = round == 4 ? 2 : round;
    old = setFFRoundingMode(s, new_round);
    FF_INIT_1(a, e, m)
    if (round == 4)
    {
        if (ff_a.value < 0)
        {
            ff_a.value = -ff_a.value;
            neg = true;
        }
        ff_a.value += 0.5f;
    }
    int32_t result_int = double_to_uint(s, ff_a.value);
    restoreFFRoundingMode(old);
    return (int64_t)result_int;
}

static inline long int lib_flexfloat_cvt_ff_w_round(Iss *s, int64_t a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    flexfloat_t ff_a;
    ff_init_int(&ff_a, a & 0xffffffff, (flexfloat_desc_t){e, m});
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

static inline unsigned long int lib_flexfloat_cvt_ff_wu_round(Iss *s, int64_t a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    flexfloat_t ff_a;
    ff_init_long(&ff_a, (uint32_t)a & 0xffffffff, (flexfloat_desc_t){e, m});
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

static inline int64_t lib_flexfloat_cvt_l_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    int64_t result_long = double_to_long(s, ff_a.value);
    restoreFFRoundingMode(old);
    return result_long;
}

static inline uint64_t lib_flexfloat_cvt_lu_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    uint64_t result_ulong = double_to_ulong(s, ff_a.value);
    restoreFFRoundingMode(old);
    return result_ulong;
}

static inline long int lib_flexfloat_cvt_ff_l_round(Iss *s, int64_t a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    flexfloat_t ff_a;
    ff_init_long(&ff_a, a, (flexfloat_desc_t){e, m});
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

static inline unsigned long int lib_flexfloat_cvt_ff_lu_round(Iss *s, uint64_t a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    flexfloat_t ff_a;
    ff_init_long_long_unsigned(&ff_a, a, (flexfloat_desc_t){e, m});
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

static inline long int lib_flexfloat_cvt_ff_ff_round(Iss *s, unsigned long int a, uint8_t es, uint8_t ms, uint8_t ed, uint8_t md, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, es, ms)
    ff_cast(&ff_res, &ff_a, (flexfloat_desc_t){ed, md});
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_res);
}

static inline iss_uim_t lib_flexfloat_fmv_x_ff(Iss *s, iss_uim_t a, uint8_t e, uint8_t m)
{
    return iss_get_signed_value(a, 32);
}

static inline iss_uim_t lib_flexfloat_fmv_ff_x(Iss *s, iss_uim_t a, uint8_t e, uint8_t m)
{
    return a;
}

static inline unsigned long int lib_flexfloat_eq(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    // The invalid flag is set if either input is a signaling nan
    if (IsNan(a, e, m) == 2 || IsNan(b, e, m) == 2)
    {
        set_fflags(s, 1ULL << 4);
    }

    // if (IsNan(a, e, m)) return 0;  WAS BEFORE, NOT ENOUGH
    if (IsNan(a, e, m) || IsNan(b, e, m))
    {
        return 0;
    }
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = ff_eq(&ff_a, &ff_b);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_ne(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    // if (IsNan(a, e, m)) return 0; WAS BEFORE, NOT ENOUGH
    if (IsNan(a, e, m) || IsNan(b, e, m))
        return 0;
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = (ff_eq(&ff_a, &ff_b) == 0);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_lt(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    if (IsNan(a, e, m) || IsNan(b, e, m))
    {
        set_fflags(s, 1ULL << 4);
        return 0;
    }
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = ff_lt(&ff_a, &ff_b);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_ge(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    if (IsNan(a, e, m) || IsNan(b, e, m))
        return 0;
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = (ff_lt(&ff_a, &ff_b) == 0);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_le(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    if (IsNan(a, e, m) || IsNan(b, e, m))
    {
        set_fflags(s, 1ULL << 4);
        return 0;
    }
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = ff_le(&ff_a, &ff_b);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_gt(Iss *s, unsigned long int a, unsigned long int b, uint8_t e, uint8_t m)
{
    if (IsNan(a, e, m) || IsNan(b, e, m))
        return 0;
    FF_INIT_2(a, b, e, m)
    feclearexcept(FE_ALL_EXCEPT);
    int32_t res = (ff_le(&ff_a, &ff_b) == 0);
    update_fflags_fenv(s);
    return res;
}

static inline unsigned long int lib_flexfloat_class(Iss *s, unsigned long int a, uint8_t e, uint8_t m)
{

#ifdef OLD
    bool sign = (a >> (e + m)) & 0x1;
    int_fast16_t exp = (a >> m) & ((0x1ULL << e) - 1);
    uint_t frac = a & ((UINT_C(1) << m) - 1);

    if (exp == INF_EXP)
    {
        if (frac == 0)
        {
            if (sign)
                return (0x1ULL << 0); // - infinity
            else
                return (0x1ULL << 7); // + infinity
        }
        else
        {
            if (frac & (0x1ULL << (m - 1)))
                return (0x1ULL << 9); // quiet NaN
            else
                return (0x1ULL << 8); // signalling NaN
        }
    }
    else if (exp == 0 && frac == 0)
    {
        if (sign)
            return (0x1ULL << 3); // - 0
        else
            return (0x1ULL << 4); // + 0
    }
    else if (exp == 0 && frac != 0)
    {
        if (sign)
            return (0x1ULL << 2); // negative subnormal
        else
            return (0x1ULL << 5); // positive subnormal
    }
    else
    {
        if (sign)
            return (0x1ULL << 1); // negative number
        else
            return (0x1ULL << 6); // positive number
    }
#else
    unsigned long int S = ((unsigned long int)a >> (e + m)) & 0x1;
    unsigned long int E = (a >> m) & ((0x1ULL << e) - 1);
    unsigned long int M = a & (((1ULL) << m) - 1);

    if (S == 1)
    {
        if (E == ((1ULL << e) - 1) && M == 0)
        {
            return (1ULL << 0); // - infinity
        }
        if (E >= 1 && E <= ((1ULL << e) - 2))
            return (1ULL << 1); // Negative normal
        if (E == 0 && M != 0)
            return (1ULL << 2); // Negative sub normal
        if (E == 0 && M == 0)
            return (1ULL << 3); // -0
        if (E == ((1ULL << e) - 1) && M != 0)
        {
            int Is_Quiet = ((unsigned long int)M >> (m - 1)) & 0x1;
            if (Is_Quiet)
                return (1ULL << 9); // Quiet nan
            else
                return (1ULL << 8); // Signaling nan
        }
    }
    else
    {
        if (E == 0 && M == 0)
            return (1ULL << 4); // +0
        if (E == 0 && M != 0)
            return (1ULL << 5); // Positive sub normal
        if (E >= 1 && E <= ((1ULL << e) - 2))
            return (1ULL << 6); // Normal positive
        if (E == ((1ULL << e) - 1) && M == 0)
            return (1ULL << 7); // + infinity
        if (E == ((1ULL << e) - 1) && M != 0)
        {
            int Is_Quiet = ((unsigned long int)M >> (m - 1)) & 0x1;
            if (Is_Quiet)
                return (1ULL << 9); // Quiet nan
            else
                return (1ULL << 8); // Signaling nan
        }
    }
    return 0;
#endif
}

static inline unsigned long int lib_flexfloat_vclass(Iss *s, unsigned long int a, unsigned long int vlen, int width, uint8_t e, uint8_t m)
{

    unsigned long int result = 0;

    for (int i = 0; i < vlen; i++)
    {
        bool sign = (a >> (e + m)) & 0x1;
        int_fast16_t exp = (a >> m) & ((0x1ULL << e) - 1);
        uint_t frac = a & ((UINT_C(1) << m) - 1);

        unsigned char cblock = ((char)sign << 7) | ((char)!sign << 6);

        if (exp == (1ULL << e) - 1)
        {
            if (frac == 0)
            {
                cblock |= 0x1; // infinity
            }
            else if (frac & (0x1ULL << (m - 1)))
            {
                cblock |= (0x1ULL << 5); // quiet NaN
            }
            else
            {
                cblock |= (0x1ULL << 4); // signalling NaN
            }
        }
        else if (exp == 0 && frac == 0)
        {
            cblock |= (0x1ULL << 3); // zero
        }
        else if (exp == 0 && frac != 0)
        {
            cblock |= (0x1ULL << 2); // subnormal
        }
        else
        {
            cblock |= (0x1ULL << 1); // normal
        }

        result |= cblock << 8 * i;
        a >>= width;
    }
    return result;
}

// TODO proper flags
static inline int lib_flexfloat_cvt_x_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
#ifdef OLD
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    long int result_long = (long int)ff_a.value;
    ;
    restoreFFRoundingMode(old);
    return result_long < -(0x1ULL << e + m) ? -(0x1ULL << e + m) : result_long > (0x1ULL << e + m) - 1 ? (0x1ULL << e + m) - 1
                                                                                              : (int)result_long;
#else
    // On gap9 this is called only on 16b float vectors
    if (IsNan(a, e, m))
        return 0X07FFF;
    unsigned long int S = ((unsigned long int)a >> (e + m)) & 0x1;
    unsigned long int E = (a >> m) & ((0x1ULL << e) - 1);
    unsigned long int M = a & (((1) << m) - 1);
    if ((S == 1) && (E == ((1ULL << e) - 1) && M == 0))
        return 0x08000; // - infinity
    else if ((S == 0) && (E == ((1ULL << e) - 1) && M == 0))
        return 0X07FFF; // + infinity

    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    int result;
    if (ff_a.value < (float)(int)0x80000000)
        result = 0x80000000;
    else if (ff_a.value > (float)(int)0x7FFFFFFF)
        result = 0x7FFFFFFF;
    else
        result = (int)ff_a.value;
    restoreFFRoundingMode(old);
    if (result < -32768)
        result = 0x08000;
    else if (result > 32767)
        result = 0x07fff;
    return result;
#endif
}

// TODO proper flags
static inline unsigned long int lib_flexfloat_cvt_xu_ff_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
#ifdef OLD
    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    long int result_long = (long int)ff_a.value;
    restoreFFRoundingMode(old);
    return result_long < 0 ? 0 : result_long > (0x1ULL << e + m + 1) - 1 ? (0x1ULL << e + m + 1) - 1
                                                                      : (unsigned long int)result_long;
#else
    // On gap9 this is called only on 16b float vectors
    if (IsNan(a, e, m))
        return 0X0FFFF;
    unsigned long int S = ((unsigned long int)a >> (e + m)) & 0x1;
    unsigned long int E = (a >> m) & ((0x1ULL << e) - 1);
    unsigned long int M = a & (((1) << m) - 1);
    if ((S == 1) && (E == ((1ULL << e) - 1) && M == 0))
        return 0; // - infinity
    else if ((S == 0) && (E == ((1ULL << e) - 1) && M == 0))
        return 0X0FFFF; // + infinity

    int old = setFFRoundingMode(s, round);
    FF_INIT_1(a, e, m)
    unsigned long int result;
    if (ff_a.value < (float)(int)0)
        result = 0;
    else if (ff_a.value > (float)(long int)0x0FFFFFFFF)
        result = 0xFFFFFFFF;
    else
        result = (unsigned long int)ff_a.value;
    restoreFFRoundingMode(old);
    if (result > 65535)
        result = 0x0ffff;
    return result;
#endif
}

// TODO proper flags
static inline int lib_flexfloat_cvt_ff_x_round(Iss *s, int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    int sign_mask = 1U << (e + m);
    flexfloat_t ff_a;
    a &= (0x1ULL << (e + m + 1)) - 1;
    a = (a ^ sign_mask) - sign_mask;
    feclearexcept(FE_ALL_EXCEPT);
    ff_init_int(&ff_a, a, (flexfloat_desc_t){e, m});
    update_fflags_fenv(s);
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

// TODO proper flags
static inline unsigned long int lib_flexfloat_cvt_ff_xu_round(Iss *s, unsigned long int a, uint8_t e, uint8_t m, unsigned long int round)
{
    int old = setFFRoundingMode(s, round);
    flexfloat_t ff_a;
    a &= (0x1ULL << (e + m + 1)) - 1;
    feclearexcept(FE_ALL_EXCEPT);
    ff_init_long(&ff_a, (unsigned long)a, (flexfloat_desc_t){e, m});
    update_fflags_fenv(s);
    restoreFFRoundingMode(old);
    return flexfloat_get_bits(&ff_a);
}

/////

#if 0

#include "lnu/lnu.h"

static inline unsigned int lib_log_add_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuAdd(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_sub_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuSub(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_mul_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuMul(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_div_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuDiv(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_itof_s(Iss *s, unsigned int a) {
  return 0;
}

static inline unsigned int lib_log_ftoi_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuF2I(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_powi_s(Iss *s, unsigned int a, unsigned int b) {
  return 0;
}

static inline unsigned int lib_log_powi_inv_s(Iss *s, unsigned int a, unsigned int b) {
  return 0;
}

static inline unsigned int lib_log_sqrt_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  // The spec defines sqrt with a second immediate operand but it seems to not be supported by the HW
  // The test is always using 1
  lnuSqrt(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_pow_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  // The spec defines pow with a second immediate operand but it seems to not be supported by the HW
  // The test is always using 1. This results in a multiplication, pow does not exist in lnu.h
  lnuMul(a, a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_exp_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuExp(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_log_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuLog(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_sin_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuSin(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_cos_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuCos(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_atan_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuAtn(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_ata_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuAta(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_atl_s(Iss *s, unsigned int a) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuAtl(a, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_sca_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuSca(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_fma_s(Iss *s, unsigned int c, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuFma(a, b, c, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_fda_s(Iss *s, unsigned int c, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuFda(a, b, c, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_fms_s(Iss *s, unsigned int c, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuFms(a, b, c, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_fds_s(Iss *s, unsigned int c, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuFds(a, b, c, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_mex_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuMex(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_dex_s(Iss *s, unsigned int a, unsigned int b) {
  long long res;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuDex(a, b, res, absErrLog, absErrArith, relErrLog, relErrArith);
  return res;
}

static inline unsigned int lib_log_halfToFloat(unsigned int a)
{
  // Sign
  unsigned int res = (((a >> 15) & 1) << 31);
  // The exponent is sign-extended from 5bits to 8bits
  if ((a >> 14) & 1) res |= 0x7 << 28;
  res |= (a & 0x3fff) << 13;
  return res;
}

static inline unsigned int lib_log_floatToHalf(unsigned int a)
{
  // TODO we should check for overflow or underflow
  // if the exponent sign is not equal bit 29:27 we have an overflow or underflow
  // lns_overflow  = ((~operand_a_i[30]) && (|(operand_a_i[29:27] & 3'b111))) ? 1'b1 : 1'b0;
  // lns_underflow = (( operand_a_i[30]) && ~(&(operand_a_i[29:27] | 3'b000))) ? 1'b1 : 1'b0;
  // in this case the output will be INF or ZERO
  return (((a >> 31) & 1) << 15) | (( a >> 13) & 0x7fff);
}

static inline unsigned int lib_log_add_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuAdd(lib_log_halfToFloat(ZL(a)), lib_log_halfToFloat(ZL(b)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuAdd(lib_log_halfToFloat(ZH(a)), lib_log_halfToFloat(ZH(b)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}

static inline unsigned int lib_log_sub_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuSub(lib_log_halfToFloat(ZL(a)), lib_log_halfToFloat(ZL(b)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuSub(lib_log_halfToFloat(ZH(a)), lib_log_halfToFloat(ZH(b)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}

static inline unsigned int lib_log_mul_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuMul(lib_log_halfToFloat(ZL(a)), lib_log_halfToFloat(ZL(b)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuMul(lib_log_halfToFloat(ZH(a)), lib_log_halfToFloat(ZH(b)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}

static inline unsigned int lib_log_div_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuDiv(lib_log_halfToFloat(ZL(a)), lib_log_halfToFloat(ZL(b)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuDiv(lib_log_halfToFloat(ZH(a)), lib_log_halfToFloat(ZH(b)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}

static inline unsigned int lib_log_htof_lo(Iss *s, unsigned int a) {
  return lib_log_halfToFloat(ZL(a));
}

static inline unsigned int lib_log_htof_hi(Iss *s, unsigned int a) {
  return lib_log_halfToFloat(ZH(a));
}

static inline unsigned int lib_log_sqrt_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuSqrt(lib_log_halfToFloat(ZL(a)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuSqrt(lib_log_halfToFloat(ZH(a)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}

static inline unsigned int lib_log_pow_vh(Iss *s, unsigned int a, unsigned int b) {
  long long resL, resH;
  double absErrLog, absErrArith, relErrLog, relErrArith;
  lnuMul(lib_log_halfToFloat(ZL(a)), lib_log_halfToFloat(ZL(a)), resL, absErrLog, absErrArith, relErrLog, relErrArith);
  lnuMul(lib_log_halfToFloat(ZH(a)), lib_log_halfToFloat(ZH(a)), resH, absErrLog, absErrArith, relErrLog, relErrArith);
  return L_H_TO_W(lib_log_floatToHalf(resL), lib_log_floatToHalf(resH));
}



static inline pc_t *lf_lnu_sfeq_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) == (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}

static inline pc_t *lf_lnu_sfne_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) != (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}

static inline pc_t *lf_lnu_sfgt_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) > (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}

static inline pc_t *lf_lnu_sfge_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) >= (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}

static inline pc_t *lf_lnu_sflt_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) < (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}

static inline pc_t *lf_lnu_sfle_s_exec(cpu_t *cpu, pc_t *pc)
{
  cpu->flag = (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[0]]) <= (int32_t)lib_log_ftoi_s(&cpu->state, cpu->regs[pc->inReg[1]]);
  return pc->next;
}





static inline pc_t *lf_lnu_sfeq_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al == bl) || (ah == bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfne_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al != bl) || (ah != bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfgt_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al > bl) || (ah > bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfge_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al >= bl) || (ah >= bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sflt_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al < bl) || (ah < bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfle_vh_any_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al <= bl) || (ah <= bh);
  return pc->next;
}




static inline pc_t *lf_lnu_sfeq_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al == bl) && (ah == bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfne_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al != bl) && (ah != bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfgt_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al > bl) && (ah > bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfge_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al >= bl) && (ah >= bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sflt_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al < bl) && (ah < bh);
  return pc->next;
}

static inline pc_t *lf_lnu_sfle_vh_all_exec(cpu_t *cpu, pc_t *pc)
{
  unsigned int a = cpu->regs[pc->inReg[0]];
  unsigned int b = cpu->regs[pc->inReg[1]];
  double al = lnuLns2Double(lib_log_halfToFloat(ZL(a)));
  double ah = lnuLns2Double(lib_log_halfToFloat(ZH(a)));
  double bl = lnuLns2Double(lib_log_halfToFloat(ZL(b)));
  double bh = lnuLns2Double(lib_log_halfToFloat(ZH(b)));
  cpu->flag = (al <= bl) && (ah <= bh);
  return pc->next;
}

#endif

/*
 * RNNEXT OPERATIONS
 */

const short lut_Tanh_m[16] = {4021, 3563, 2835, 2070, 1418, 929, 592, 370, 228, 140, 86, 52, 32, 19, 12, 7};
const int lut_Tanh_q[16] = {17060, 512067, 2012407, 4361003, 7021506, 9510743, 11575189, 13158594, 14311861, 15123015, 15679911, 16055709, 16306104, 16471340, 16579558, 16650000};
const short lut_sig_m[16] = {1019, 988, 930, 850, 758, 660, 563, 472, 391, 319, 258, 207, 165, 131, 104, 82};
const int lut_sig_q[16] = {8389671, 8423495, 8544906, 8789991, 9169470, 9670607, 10264318, 10914030, 11583389, 12241371, 12864661, 13437943, 13952921, 14406803, 14800713, 15138308};

static inline unsigned int lib_TANHorSIG(Iss *s, unsigned int a, short isSig)
{
    unsigned int lutsize = 16;
    unsigned int value1 = 4096;
    unsigned int value0p999 = 4095;
    int m;
    int q;
    int q_signed;
    unsigned short sign = (a >> 31) & 0x1;
    int tmp;
    int mac_result;
    int mac_result_signed;
    int abs_a;

    if (sign == 0x1)
    {
        abs_a = (~a);
    }
    else
    {
        abs_a = (a);
    }
    tmp = abs_a >> (13 - 3); // get index of LUT
    if (tmp >= lutsize)
    {
        if (isSig == 0)
        { // tanh
            return (sign == 0x1) ? (int)-value1 : (int)value1;
        }
        else
        { // sig
            return (sign == 0x1) ? (int)0 : (int)value1;
        }
    }
    else
    {
        if (isSig == 0)
        {
            m = lut_Tanh_m[tmp];
            q = lut_Tanh_q[tmp];
        }
        else
        {
            m = lut_sig_m[tmp];
            q = lut_sig_q[tmp];
        }
        mac_result = (m * abs_a + q) >> 12;
        mac_result_signed = (sign == 1) ? ~mac_result : mac_result;
        if (isSig == 1 && sign == 1)
        {
            return (unsigned int)(value0p999 + (mac_result_signed)); // 1-(mx+q)=4096+(~mac_result+1)=4095+(~mac_result)
        }
        else
        {
            return (unsigned int)mac_result_signed;
        }
    }
}

static inline unsigned int lib_TANH(Iss *s, unsigned int a)
{
    return lib_TANHorSIG(s, a, 0);
}

static inline unsigned int lib_SIG(Iss *s, unsigned int a)
{
    return lib_TANHorSIG(s, a, 1);
}

/*
 *  pulpnn: Nibble and crumb operations
 */

static inline int lib_VEC_QNT_4(Iss *s, int input, uint16_t *pThr)
{
    int ret = 0;
    // printf("pThr: %X, *pThr: %X, input: %d \n", pThr, *pThr, input);

    if (input <= pThr[7])
    {
        if (input <= pThr[3])
        {
            if (input <= pThr[1])
            {
                if (input <= pThr[0])
                    ret = -8;
                else
                    ret = -7;
            }
            else
            {
                if (input <= pThr[2])
                    ret = -6;
                else
                    ret = -5;
            }
        }
        else
        {
            if (input <= pThr[5])
            {
                if (input <= pThr[4])
                    ret = -4;
                else
                    ret = -3;
            }
            else
            {
                if (input <= pThr[6])
                    ret = -2;
                else
                    ret = -1;
            }
        }
    }
    else
    {
        if (input <= pThr[11])
        {
            if (input <= pThr[9])
            {
                if (input <= pThr[8])
                    ret = 0;
                else
                    ret = 1;
            }
            else
            {
                if (input <= pThr[10])
                    ret = 2;
                else
                    ret = 3;
            }
        }
        else
        {
            if (input <= pThr[13])
            {
                if (input <= pThr[12])
                    ret = 4;
                else
                    ret = 5;
            }
            else
            {
                if (input <= pThr[14])
                    ret = 6;
                else
                    ret = 7;
            }
        }
    }
    return ret;
}

#define VEC_OP_NN(operName, type, elemType, elemSize, num_elem, oper)                                                   \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b)                  \
    {                                                                                                                   \
        int8_t *tmp_a = (int8_t *)&a;                                                                                   \
        int8_t *tmp_b = (int8_t *)&b;                                                                                   \
        int8_t lowa, lowb, higha, highb, highout, lowout;                                                               \
        type out;                                                                                                       \
        int8_t *tmp_out = (int8_t *)&out;                                                                               \
        int i;                                                                                                          \
        if (num_elem == 8)                                                                                              \
        {                                                                                                               \
            for (i = 0; i < (num_elem >> 1); i++)                                                                       \
            {                                                                                                           \
                lowa = tmp_a[i] & 0x0F;                                                                                 \
                lowa = (lowa & 0x8) ? (lowa | 0xF0) : (lowa & 0x0F);                                                    \
                higha = (tmp_a[i] >> 4) & 0x0F;                                                                         \
                higha = (higha & 0x8) ? (higha | 0xF0) : (higha & 0x0F);                                                \
                lowb = tmp_b[i] & 0x0F;                                                                                 \
                lowb = (lowb & 0x8) ? (lowb | 0xF0) : (lowb & 0x0F);                                                    \
                highb = (tmp_b[i] >> 4) & 0x0F;                                                                         \
                highb = (highb & 0x8) ? (highb | 0xF0) : (highb & 0x0F);                                                \
                lowout = lowa oper lowb;                                                                                \
                highout = higha oper highb;                                                                             \
                tmp_out[i] = (lowout & 0x0F) | ((highout << 4) & 0xF0);                                                 \
            }                                                                                                           \
        }                                                                                                               \
        else if (num_elem == 16)                                                                                        \
        {                                                                                                               \
            int8_t midla, midlb, midha, midhb, midlo, midho;                                                            \
            for (i = 0; i < (num_elem >> 2); i++)                                                                       \
            {                                                                                                           \
                lowa = tmp_a[i] & 0x03;                                                                                 \
                lowa = (lowa & 0x2) ? (lowa | 0xFC) : (lowa & 0x03);                                                    \
                midla = (tmp_a[i] >> 2) & 0x03;                                                                         \
                midla = (midla & 0x2) ? (midla | 0xFC) : (midla & 0x03);                                                \
                midha = (tmp_a[i] >> 4) & 0x03;                                                                         \
                midha = (midha & 0x2) ? (midha | 0xFC) : (midha & 0x03);                                                \
                higha = (tmp_a[i] >> 6) & 0x03;                                                                         \
                higha = (higha & 0x2) ? (higha | 0xFC) : (higha & 0x03);                                                \
                lowb = tmp_b[i] & 0x03;                                                                                 \
                lowb = (lowb & 0x2) ? (lowb | 0xFC) : (lowb & 0x03);                                                    \
                midlb = (tmp_b[i] >> 2) & 0x03;                                                                         \
                midlb = (midlb & 0x2) ? (midlb | 0xFC) : (midlb & 0x03);                                                \
                midhb = (tmp_b[i] >> 4) & 0x03;                                                                         \
                midhb = (midhb & 0x2) ? (midhb | 0xFC) : (midhb & 0x03);                                                \
                highb = (tmp_b[i] >> 6) & 0x03;                                                                         \
                highb = (highb & 0x2) ? (highb | 0xFC) : (highb & 0x03);                                                \
                lowout = lowa oper lowb;                                                                                \
                midlo = midla oper midlb;                                                                               \
                midho = midha oper midhb;                                                                               \
                highout = higha oper highb;                                                                             \
                tmp_out[i] = (lowout & 0x03) | ((midlo << 2) & 0x0C) | ((midho << 4) & 0x30) | ((highout << 6) & 0xC0); \
            }                                                                                                           \
        }                                                                                                               \
        return out;                                                                                                     \
    }                                                                                                                   \
                                                                                                                        \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, int8_t b)             \
    {                                                                                                                   \
        int8_t *tmp_a = (int8_t *)&a;                                                                                   \
        type out;                                                                                                       \
        int8_t *tmp_out = (int8_t *)&out;                                                                               \
        int8_t lowa, higha, lowb, highout, lowout;                                                                      \
        int i;                                                                                                          \
        if (num_elem == 8)                                                                                              \
        {                                                                                                               \
            for (i = 0; i < (num_elem >> 1); i++)                                                                       \
            {                                                                                                           \
                lowa = tmp_a[i] & 0x0F;                                                                                 \
                lowa = (lowa & 0x8) ? (lowa | 0xF0) : (lowa & 0x0F);                                                    \
                higha = (tmp_a[i] >> 4) & 0x0F;                                                                         \
                higha = (higha & 0x8) ? (higha | 0xF0) : (higha & 0x0F);                                                \
                lowb = b & 0x0F;                                                                                        \
                lowb = (lowb & 0x8) ? (lowb | 0xF0) : (lowb & 0x0F);                                                    \
                lowout = lowa oper lowb;                                                                                \
                highout = higha oper lowb;                                                                              \
                tmp_out[i] = (lowout & 0x0F) | ((highout << 4) & 0xF0);                                                 \
            }                                                                                                           \
        }                                                                                                               \
        else if (num_elem == 16)                                                                                        \
        {                                                                                                               \
            int8_t midla, midlb, midha, midhb, midlo, midho;                                                            \
            for (i = 0; i < (num_elem >> 2); i++)                                                                       \
            {                                                                                                           \
                lowa = tmp_a[i] & 0x03;                                                                                 \
                lowa = (lowa & 0x2) ? (lowa | 0xFC) : (lowa & 0x03);                                                    \
                midla = (tmp_a[i] >> 2) & 0x03;                                                                         \
                midla = (midla & 0x2) ? (midla | 0xFC) : (midla & 0x03);                                                \
                midha = (tmp_a[i] >> 4) & 0x03;                                                                         \
                midha = (midha & 0x2) ? (midha | 0xFC) : (midha & 0x03);                                                \
                higha = (tmp_a[i] >> 6) & 0x03;                                                                         \
                higha = (higha & 0x2) ? (higha | 0xFC) : (higha & 0x03);                                                \
                lowb = b & 0x03;                                                                                        \
                lowb = (lowb & 0x2) ? (lowb | 0xFC) : (lowb & 0x03);                                                    \
                lowout = lowa oper lowb;                                                                                \
                midlo = midla oper lowb;                                                                                \
                midho = midha oper lowb;                                                                                \
                highout = higha oper lowb;                                                                              \
                tmp_out[i] = (lowout & 0x03) | ((midlo << 2) & 0x0C) | ((midho << 4) & 0x30) | ((highout << 6) & 0xC0); \
            }                                                                                                           \
        }                                                                                                               \
        return out;                                                                                                     \
    }

/* pulpnn instantiation */
VEC_OP_NN(ADD, int32_t, int4_t, 1, 8, +)
VEC_OP_NN(ADD, int32_t, int2_t, 1, 16, +)
VEC_OP_NN(SUB, int32_t, int4_t, 1, 8, -)
VEC_OP_NN(SUB, int32_t, int2_t, 1, 16, -)
VEC_OP_NN(OR, int32_t, int4_t, 1, 8, |)
VEC_OP_NN(OR, int32_t, int2_t, 1, 16, |)
VEC_OP_NN(XOR, int32_t, int4_t, 1, 8, ^)
VEC_OP_NN(XOR, int32_t, int2_t, 1, 16, ^)
VEC_OP_NN(AND, int32_t, int4_t, 1, 8, &)
VEC_OP_NN(AND, int32_t, int2_t, 1, 16, &)

#define VEC_DOTP_NN(operName, typeOut, typeA, typeB, elemTypeA, elemTypeB, elemSize, num_elem, oper, signed1, signed2) \
    static inline typeOut lib_VEC_##operName##_##elemSize(Iss *s, typeA a, typeB b)                        \
    {                                                                                                                  \
        typeOut out = 0;                                                                                               \
        int8_t *tmp_a = (int8_t *)&a;                                                                                  \
        int8_t a0, a1;                                                                                                 \
        int8_t *tmp_b = (int8_t *)&b;                                                                                  \
        int8_t b0, b1;                                                                                                 \
        int i;                                                                                                         \
        if (num_elem == 8)                                                                                             \
        {                                                                                                              \
            for (i = 0; i < (num_elem >> 1); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x0F;                                                                                  \
                a0 = signed1 ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                                \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                           \
                a1 = signed1 ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                                \
                b0 = tmp_b[i] & 0x0F;                                                                                  \
                b0 = signed2 ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                                \
                b1 = (tmp_b[i] >> 4) & 0x0F;                                                                           \
                b1 = signed2 ? ((b1 & 0x08) ? (b1 | 0xF0) : (b1 & 0x0F)) : (b1 & 0x0F);                                \
                out += (a1 oper b1 + a0 oper b0);                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        else if (num_elem == 16)                                                                                       \
        {                                                                                                              \
            int8_t a2, a3;                                                                                             \
            int8_t b2, b3;                                                                                             \
            for (i = 0; i < (num_elem >> 2); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x03;                                                                                  \
                a0 = signed1 ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                                \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                           \
                a1 = signed1 ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                                \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                           \
                a2 = signed1 ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                                \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                           \
                a3 = signed1 ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                                \
                b0 = tmp_b[i] & 0x03;                                                                                  \
                b0 = signed2 ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                                \
                b1 = (tmp_b[i] >> 2) & 0x03;                                                                           \
                b1 = signed2 ? ((b1 & 0x02) ? (b1 | 0xFC) : (b1 & 0x03)) : (b1 & 0x03);                                \
                b2 = (tmp_b[i] >> 4) & 0x03;                                                                           \
                b2 = signed2 ? ((b2 & 0x02) ? (b2 | 0xFC) : (b2 & 0x03)) : (b2 & 0x03);                                \
                b3 = (tmp_b[i] >> 6) & 0x03;                                                                           \
                b3 = signed2 ? ((b3 & 0x02) ? (b3 | 0xFC) : (b3 & 0x03)) : (b3 & 0x03);                                \
                out += (a0 oper b0) + (a1 oper b1) + (a2 oper b2) + (a3 oper b3);                                      \
            }                                                                                                          \
        }                                                                                                              \
        return out;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    static inline typeOut lib_VEC_##operName##_SC_##elemSize(Iss *s, typeA a, typeB b)                     \
    {                                                                                                                  \
        typeOut out = 0;                                                                                               \
        int8_t *tmp_a = (int8_t *)&a;                                                                                  \
        int8_t a0, a1;                                                                                                 \
        int8_t *tmp_b = (int8_t *)&b;                                                                                  \
        int8_t b0;                                                                                                     \
        int i;                                                                                                         \
        if (num_elem == 8)                                                                                             \
        {                                                                                                              \
            for (i = 0; i < (num_elem >> 1); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x0F;                                                                                  \
                a0 = signed1 ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                                \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                           \
                a1 = signed1 ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                                \
                b0 = tmp_b[0] & 0x0F;                                                                                  \
                b0 = signed2 ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                                \
                int mid = (a1 oper b0 + a0 oper b0);                                                                   \
                out += mid;                                                                                            \
            }                                                                                                          \
        }                                                                                                              \
        else if (num_elem == 16)                                                                                       \
        {                                                                                                              \
            int8_t a2, a3;                                                                                             \
            for (i = 0; i < (num_elem >> 2); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x03;                                                                                  \
                a0 = signed1 ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                                \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                           \
                a1 = signed1 ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                                \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                           \
                a2 = signed1 ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                                \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                           \
                a3 = signed1 ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                                \
                b0 = tmp_b[0] & 0x03;                                                                                  \
                b0 = signed2 ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                                \
                out += (a0 oper b0) + (a1 oper b0) + (a2 oper b0) + (a3 oper b0);                                      \
            }                                                                                                          \
        }                                                                                                              \
        return out;                                                                                                    \
    }

VEC_DOTP_NN(DOTSP, int32_t, int32_t, int32_t, int4_t, int4_t, 4, 8, *, 1, 1)
VEC_DOTP_NN(DOTSP, int32_t, int32_t, int32_t, int2_t, int2_t, 2, 16, *, 1, 1)

VEC_DOTP_NN(DOTUP, uint32_t, uint32_t, uint32_t, uint4_t, uint4_t, 4, 8, *, 0, 0)
VEC_DOTP_NN(DOTUP, uint32_t, uint32_t, uint32_t, uint2_t, uint2_t, 2, 16, *, 0, 0)

VEC_DOTP_NN(DOTUSP, int32_t, uint32_t, int32_t, uint4_t, int4_t, 4, 8, *, 0, 1)
VEC_DOTP_NN(DOTUSP, int32_t, uint32_t, int32_t, uint2_t, int2_t, 2, 16, *, 0, 1)

#define VEC_SDOT_NN(operName, typeOut, typeA, typeB, elemTypeA, elemTypeB, elemSize, num_elem, oper, signed1, signed2) \
    static inline typeOut lib_VEC_##operName##_##elemSize(Iss *s, typeOut out, typeA a, typeB b)           \
    {                                                                                                                  \
        int8_t *tmp_a = (int8_t *)&a;                                                                                  \
        int8_t a0, a1;                                                                                                 \
        int8_t *tmp_b = (int8_t *)&b;                                                                                  \
        int8_t b0, b1;                                                                                                 \
        int i;                                                                                                         \
        if (num_elem == 8)                                                                                             \
        {                                                                                                              \
            for (i = 0; i < (num_elem >> 1); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x0F;                                                                                  \
                a0 = signed1 ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                                \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                           \
                a1 = signed1 ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                                \
                b0 = tmp_b[i] & 0x0F;                                                                                  \
                b0 = signed2 ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                                \
                b1 = (tmp_b[i] >> 4) & 0x0F;                                                                           \
                b1 = signed2 ? ((b1 & 0x08) ? (b1 | 0xF0) : (b1 & 0x0F)) : (b1 & 0x0F);                                \
                int mid = (a1 oper b1 + a0 oper b0);                                                                   \
                out += mid;                                                                                            \
            }                                                                                                          \
        }                                                                                                              \
        else if (num_elem == 16)                                                                                       \
        {                                                                                                              \
            int8_t a2, a3;                                                                                             \
            int8_t b2, b3;                                                                                             \
            for (i = 0; i < (num_elem >> 2); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x03;                                                                                  \
                a0 = signed1 ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                                \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                           \
                a1 = signed1 ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                                \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                           \
                a2 = signed1 ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                                \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                           \
                a3 = signed1 ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                                \
                b0 = tmp_b[i] & 0x03;                                                                                  \
                b0 = signed2 ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                                \
                b1 = (tmp_b[i] >> 2) & 0x03;                                                                           \
                b1 = signed2 ? ((b1 & 0x02) ? (b1 | 0xFC) : (b1 & 0x03)) : (b1 & 0x03);                                \
                b2 = (tmp_b[i] >> 4) & 0x03;                                                                           \
                b2 = signed2 ? ((b2 & 0x02) ? (b2 | 0xFC) : (b2 & 0x03)) : (b2 & 0x03);                                \
                b3 = (tmp_b[i] >> 6) & 0x03;                                                                           \
                b3 = signed2 ? ((b3 & 0x02) ? (b3 | 0xFC) : (b3 & 0x03)) : (b3 & 0x03);                                \
                out += (a0 oper b0) + (a1 oper b1) + (a2 oper b2) + (a3 oper b3);                                      \
            }                                                                                                          \
        }                                                                                                              \
        return out;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    static inline typeOut lib_VEC_##operName##_SC_##elemSize(Iss *s, typeOut out, typeA a, typeB b)        \
    {                                                                                                                  \
        int8_t *tmp_a = (int8_t *)&a;                                                                                  \
        int8_t a0, a1;                                                                                                 \
        int8_t *tmp_b = (int8_t *)&b;                                                                                  \
        int8_t b0;                                                                                                     \
        int i;                                                                                                         \
        if (num_elem == 8)                                                                                             \
        {                                                                                                              \
            for (i = 0; i < (num_elem >> 1); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x0F;                                                                                  \
                a0 = signed1 ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                                \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                           \
                a1 = signed1 ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                                \
                b0 = tmp_b[0] & 0x0F;                                                                                  \
                b0 = signed2 ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                                \
                int mid = (a1 oper b0 + a0 oper b0);                                                                   \
                out += mid;                                                                                            \
            }                                                                                                          \
        }                                                                                                              \
        else if (num_elem == 16)                                                                                       \
        {                                                                                                              \
            int8_t a2, a3;                                                                                             \
            for (i = 0; i < (num_elem >> 2); i++)                                                                      \
            {                                                                                                          \
                a0 = tmp_a[i] & 0x03;                                                                                  \
                a0 = signed1 ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                                \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                           \
                a1 = signed1 ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                                \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                           \
                a2 = signed1 ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                                \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                           \
                a3 = signed1 ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                                \
                b0 = tmp_b[0] & 0x03;                                                                                  \
                b0 = signed2 ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                                \
                out += (a0 oper b0) + (a1 oper b0) + (a2 oper b0) + (a3 oper b0);                                      \
            }                                                                                                          \
        }                                                                                                              \
        return out;                                                                                                    \
    }
VEC_SDOT_NN(SDOTSP, int32_t, int32_t, int32_t, int4_t, int4_t, 4, 8, *, 1, 1)
VEC_SDOT_NN(SDOTSP, int32_t, int32_t, int32_t, int2_t, int2_t, 2, 16, *, 1, 1)

VEC_SDOT_NN(SDOTUP, uint32_t, uint32_t, uint32_t, uint4_t, uint4_t, 4, 8, *, 0, 0)
VEC_SDOT_NN(SDOTUP, uint32_t, uint32_t, uint32_t, uint2_t, uint2_t, 2, 16, *, 0, 0)

VEC_SDOT_NN(SDOTUSP, int32_t, uint32_t, int32_t, uint4_t, int4_t, 4, 8, *, 0, 1)
VEC_SDOT_NN(SDOTUSP, int32_t, uint32_t, int32_t, uint2_t, int2_t, 2, 16, *, 0, 1)

// AVERAGE VECTORIAL OPERATIONS: NIBBLE AND CRUMB, SCALAR & VECTOR
#define VEC_EXPR_NN_AVG(operName, type, elemType, elemSize, num_elem, signed, gcc_type)                          \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b)           \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        gcc_type *tmp_b = (gcc_type *)&b;                                                                        \
        type out;                                                                                                \
        gcc_type a0, a1, b0, b1;                                                                                 \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[i] & 0x0F;                                                                            \
                b0 = signed ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                           \
                b1 = (tmp_b[i] >> 4) & 0x0F;                                                                     \
                b1 = signed ? (((b1 & 0x08) != 0) ? (b1 | 0xF0) : (b1 & 0x0F)) : (b1 & 0x0F);                    \
                out0 = (a0 + b0) >> 1;                                                                           \
                out1 = (a1 + b1) >> 1;                                                                           \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3, b2, b3;                                                                             \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[i] & 0x03;                                                                            \
                b0 = signed ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                           \
                b1 = (tmp_b[i] >> 2) & 0x03;                                                                     \
                b1 = signed ? (((b1 & 0x02) != 0) ? (b1 | 0xFC) : (b1 & 0x03)) : (b1 & 0x03);                    \
                b2 = (tmp_b[i] >> 4) & 0x03;                                                                     \
                b2 = signed ? ((b2 & 0x02) ? (b2 | 0xFC) : (b2 & 0x03)) : (b2 & 0x03);                           \
                b3 = (tmp_b[i] >> 6) & 0x03;                                                                     \
                b3 = signed ? (((b3 & 0x02) != 0) ? (b3 | 0xFC) : (b3 & 0x03)) : (b3 & 0x03);                    \
                out0 = (a0 + b0) >> 1;                                                                           \
                out1 = (a1 + b1) >> 1;                                                                           \
                out2 = (a2 + b2) >> 1;                                                                           \
                out3 = (a3 + b3) >> 1;                                                                           \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }                                                                                                            \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, type b)        \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        gcc_type *tmp_b = (gcc_type *)&b;                                                                        \
        type out;                                                                                                \
        gcc_type a0, a1, b0;                                                                                     \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[0] & 0x0F;                                                                            \
                b0 = signed ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                           \
                out0 = (a0 + b0) >> 1;                                                                           \
                out1 = (a1 + b0) >> 1;                                                                           \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3, b2, b3;                                                                             \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[0] & 0x03;                                                                            \
                b0 = signed ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                           \
                out0 = (a0 + b0) >> 1;                                                                           \
                out1 = (a1 + b0) >> 1;                                                                           \
                out2 = (a2 + b0) >> 1;                                                                           \
                out3 = (a3 + b0) >> 1;                                                                           \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }

VEC_EXPR_NN_AVG(AVG, int32_t, int4_t, 1, 8, 1, int8_t)
VEC_EXPR_NN_AVG(AVG, int32_t, int2_t, 1, 16, 1, int8_t)
VEC_EXPR_NN_AVG(AVGU, uint32_t, uint4_t, 1, 8, 0, uint8_t)
VEC_EXPR_NN_AVG(AVGU, uint32_t, uint2_t, 1, 16, 0, uint8_t)

// MAX & MIN VECTORIAL OPERATIONS: NIBBLE AND CRUMB, SCALAR & VECTOR
#define VEC_EXPR_NN_MAX(operName, type, elemType, elemSize, num_elem, signed, gcc_type, oper)                    \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b)           \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        gcc_type *tmp_b = (gcc_type *)&b;                                                                        \
        type out;                                                                                                \
        gcc_type a0, a1, b0, b1;                                                                                 \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[i] & 0x0F;                                                                            \
                b0 = signed ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                           \
                b1 = (tmp_b[i] >> 4) & 0x0F;                                                                     \
                b1 = signed ? ((b1 & 0x08) ? (b1 | 0xF0) : (b1 & 0x0F)) : (b1 & 0x0F);                           \
                out0 = (a0 > b0) ? (oper ? a0 : b0) : (oper ? b0 : a0);                                          \
                out1 = (a1 > b1) ? (oper ? a1 : b1) : (oper ? b1 : a1);                                          \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3, b2, b3;                                                                             \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[i] & 0x03;                                                                            \
                b0 = signed ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                           \
                b1 = (tmp_b[i] >> 2) & 0x03;                                                                     \
                b1 = signed ? ((b1 & 0x02) ? (b1 | 0xFC) : (b1 & 0x03)) : (b1 & 0x03);                           \
                b2 = (tmp_b[i] >> 4) & 0x03;                                                                     \
                b2 = signed ? ((b2 & 0x02) ? (b2 | 0xFC) : (b2 & 0x03)) : (b2 & 0x03);                           \
                b3 = (tmp_b[i] >> 6) & 0x03;                                                                     \
                b3 = signed ? ((b3 & 0x02) ? (b3 | 0xFC) : (b3 & 0x03)) : (b3 & 0x03);                           \
                out0 = (a0 > b0) ? (oper ? a0 : b0) : (oper ? b0 : a0);                                          \
                out1 = (a1 > b1) ? (oper ? a1 : b1) : (oper ? b1 : a1);                                          \
                out2 = (a2 > b2) ? (oper ? a2 : b2) : (oper ? b2 : a2);                                          \
                out3 = (a3 > b3) ? (oper ? a3 : b3) : (oper ? b3 : a3);                                          \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }                                                                                                            \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, type b)        \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        gcc_type *tmp_b = (gcc_type *)&b;                                                                        \
        type out;                                                                                                \
        gcc_type a0, a1, b0;                                                                                     \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[0] & 0x0F;                                                                            \
                b0 = signed ? ((b0 & 0x08) ? (b0 | 0xF0) : (b0 & 0x0F)) : (b0 & 0x0F);                           \
                out0 = (a0 > b0) ? (oper ? a0 : b0) : (oper ? b0 : a0);                                          \
                out1 = (a1 > b0) ? (oper ? a1 : b0) : (oper ? b0 : a1);                                          \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3, b2, b3;                                                                             \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[0] & 0x03;                                                                            \
                b0 = signed ? ((b0 & 0x02) ? (b0 | 0xFC) : (b0 & 0x03)) : (b0 & 0x03);                           \
                out0 = (a0 > b0) ? (oper ? a0 : b0) : (oper ? b0 : a0);                                          \
                out1 = (a1 > b0) ? (oper ? a1 : b0) : (oper ? b0 : a1);                                          \
                out0 = (a2 > b0) ? (oper ? a2 : b0) : (oper ? b0 : a2);                                          \
                out1 = (a3 > b0) ? (oper ? a3 : b0) : (oper ? b0 : a3);                                          \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }

VEC_EXPR_NN_MAX(MAX, int32_t, int4_t, 1, 8, 1, int8_t, 1) // (...,1) for max , (..,1,..,..) for signed
VEC_EXPR_NN_MAX(MAX, int32_t, int2_t, 1, 16, 1, int8_t, 1)
VEC_EXPR_NN_MAX(MAXU, uint32_t, uint4_t, 1, 8, 0, uint8_t, 1) // (..., 1) for max, (..,0,..,..) for unsigned
VEC_EXPR_NN_MAX(MAXU, uint32_t, uint2_t, 1, 16, 0, uint8_t, 1)

VEC_EXPR_NN_MAX(MIN, int32_t, int4_t, 1, 8, 1, int8_t, 0) // (..., 0) for min, (..,1,..,..) for signed
VEC_EXPR_NN_MAX(MIN, int32_t, int2_t, 1, 16, 1, int8_t, 0)
VEC_EXPR_NN_MAX(MINU, uint32_t, uint4_t, 1, 8, 0, uint8_t, 0)
VEC_EXPR_NN_MAX(MINU, uint32_t, uint2_t, 1, 16, 0, uint8_t, 0) // (..., 0) for min, (..,0,..,..) for unsigned

// SRA SRL & SLL VECTORIAL OPERATIONS: NIBBLE AND CRUMB, SCALAR & VECTOR
#define VEC_EXPR_NN_SHIFT(operName, type, elemType, elemSize, num_elem, signed, gcc_type, oper)                  \
    static inline type lib_VEC_##operName##_##elemType##_to_##type(Iss *s, type a, type b)           \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        uint8_t *tmp_b = (uint8_t *)&b;                                                                          \
        type out;                                                                                                \
        gcc_type a0, a1;                                                                                         \
        uint8_t b0, b1;                                                                                          \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[i] & 0x0F;                                                                            \
                b1 = (tmp_b[i] >> 4) & 0x0F;                                                                     \
                out0 = a0 oper(b0 & 0x3);                                                                        \
                out1 = a1 oper(b1 & 0x3);                                                                        \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3;                                                                                     \
            uint8_t b2, b3;                                                                                      \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[i] & 0x03;                                                                            \
                b1 = (tmp_b[i] >> 2) & 0x03;                                                                     \
                b2 = (tmp_b[i] >> 4) & 0x03;                                                                     \
                b3 = (tmp_b[i] >> 6) & 0x03;                                                                     \
                out0 = a0 oper(b0 & 0x1);                                                                        \
                out1 = a1 oper(b1 & 0x1);                                                                        \
                out2 = a2 oper(b2 & 0x1);                                                                        \
                out3 = a3 oper(b3 & 0x1);                                                                        \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }                                                                                                            \
    static inline type lib_VEC_##operName##_SC_##elemType##_to_##type(Iss *s, type a, type b)        \
    {                                                                                                            \
        gcc_type *tmp_a = (gcc_type *)&a;                                                                        \
        uint8_t *tmp_b = (uint8_t *)&b;                                                                          \
        type out;                                                                                                \
        gcc_type a0, a1;                                                                                         \
        uint8_t b0;                                                                                              \
        gcc_type out0, out1;                                                                                     \
        gcc_type *tmp_out = (gcc_type *)&out;                                                                    \
        int i;                                                                                                   \
        if (num_elem == 8)                                                                                       \
        {                                                                                                        \
            for (i = 0; i < (num_elem >> 1); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x0F;                                                                            \
                a0 = signed ? ((a0 & 0x08) ? (a0 | 0xF0) : (a0 & 0x0F)) : (a0 & 0x0F);                           \
                a1 = (tmp_a[i] >> 4) & 0x0F;                                                                     \
                a1 = signed ? ((a1 & 0x08) ? (a1 | 0xF0) : (a1 & 0x0F)) : (a1 & 0x0F);                           \
                b0 = tmp_b[0] & 0x0F;                                                                            \
                out0 = a0 oper(b0 & 0x3);                                                                        \
                out1 = a1 oper(b0 & 0x3);                                                                        \
                tmp_out[i] = (out0 & 0x0F) | ((out1 & 0x0F) << 4);                                               \
            }                                                                                                    \
        }                                                                                                        \
        else if (num_elem == 16)                                                                                 \
        {                                                                                                        \
            gcc_type a2, a3;                                                                                     \
            uint8_t b2, b3;                                                                                      \
            gcc_type out2, out3;                                                                                 \
            for (i = 0; i < (num_elem >> 2); i++)                                                                \
            {                                                                                                    \
                a0 = tmp_a[i] & 0x03;                                                                            \
                a0 = signed ? ((a0 & 0x02) ? (a0 | 0xFC) : (a0 & 0x03)) : (a0 & 0x03);                           \
                a1 = (tmp_a[i] >> 2) & 0x03;                                                                     \
                a1 = signed ? ((a1 & 0x02) ? (a1 | 0xFC) : (a1 & 0x03)) : (a1 & 0x03);                           \
                a2 = (tmp_a[i] >> 4) & 0x03;                                                                     \
                a2 = signed ? ((a2 & 0x02) ? (a2 | 0xFC) : (a2 & 0x03)) : (a2 & 0x03);                           \
                a3 = (tmp_a[i] >> 6) & 0x03;                                                                     \
                a3 = signed ? ((a3 & 0x02) ? (a3 | 0xFC) : (a3 & 0x03)) : (a3 & 0x03);                           \
                b0 = tmp_b[0] & 0x03;                                                                            \
                out0 = a0 oper(b0 & 0x1);                                                                        \
                out1 = a1 oper(b0 & 0x1);                                                                        \
                out2 = a2 oper(b0 & 0x1);                                                                        \
                out3 = a3 oper(b0 & 0x1);                                                                        \
                tmp_out[i] = (out0 & 0x03) | ((out1 & 0x03) << 2) | ((out2 & 0x03) << 4) | ((out3 & 0x03) << 6); \
            }                                                                                                    \
        }                                                                                                        \
        return out;                                                                                              \
    }

VEC_EXPR_NN_SHIFT(SRL, uint32_t, uint4_t, 1, 8, 0, uint8_t, >>)
VEC_EXPR_NN_SHIFT(SRL, uint32_t, uint2_t, 1, 16, 0, uint8_t, >>)

VEC_EXPR_NN_SHIFT(SRA, int32_t, int4_t, 1, 8, 1, int8_t, >>)
VEC_EXPR_NN_SHIFT(SRA, int32_t, int2_t, 1, 16, 1, int8_t, >>)

VEC_EXPR_NN_SHIFT(SLL, uint32_t, uint4_t, 1, 8, 0, uint8_t, <<)
VEC_EXPR_NN_SHIFT(SLL, uint32_t, uint2_t, 1, 16, 0, uint8_t, <<)

/* pulp_nn abs nibble and crumb */
static inline int32_t lib_VEC_ABS_int4_t_to_int32_t(Iss *s, int32_t a)
{
    int8_t *tmp_a = (int8_t *)&a;
    int8_t ah, al;
    int8_t oh, ol;
    int32_t out;
    int8_t *tmp_out = (int8_t *)&out;
    int i;
    for (i = 0; i < 4; i++)
    {
        al = tmp_a[i] & 0x0F;
        al = ((al & 0x08) ? (al | 0xF0) : (al & 0x0F));
        ah = (tmp_a[i] >> 4) & 0x0F;
        ah = ((ah & 0x08) ? (ah | 0xF0) : (ah & 0x0F));
        ol = (al > 0) ? al : -al;
        oh = (ah > 0) ? ah : -ah;
        tmp_out[i] = (ol & 0x0F) | ((oh & 0x0F) << 4);
    }
    return out;
}

static inline int32_t lib_VEC_ABS_int2_t_to_int32_t(Iss *s, int32_t a)
{
    int16_t *tmp_a = (int16_t *)&a;
    int8_t ah, al, aml, amh;
    int8_t oh, ol, oml, omh;
    int32_t out;
    int8_t *tmp_out = (int8_t *)&out;
    int i;
    for (i = 0; i < 4; i++)
    {
        al = tmp_a[i] & 0x3;
        al = ((al & 0x02) ? (al | 0xFC) : (al & 0x03));
        aml = (tmp_a[i] >> 2) & 0x3;
        aml = ((aml & 0x02) ? (aml | 0xFC) : (aml & 0x03));
        amh = (tmp_a[i] >> 4) & 0x3;
        amh = ((amh & 0x02) ? (amh | 0xFC) : (amh & 0x03));
        ah = (tmp_a[i] >> 6) & 0x3;
        ah = ((ah & 0x02) ? (ah | 0xFC) : (ah & 0x03));
        ol = (al > 0) ? al : -al;
        omh = (amh > 0) ? amh : -amh;
        oml = (aml > 0) ? aml : -aml;
        oh = (ah > 0) ? ah : -ah;
        tmp_out[i] = (ol & 0x3) | ((oml & 0x3) << 2) | ((omh & 0x3) << 4) | ((oh & 0x3) << 6);
    }
    return out;
}
/*end pulp_nn abs nibble and crumb */

/*
 * GAP int64 extension
 */

static inline uint64_t lib_ADD_64(Iss *s, uint64_t a, uint64_t b) { return a + b; }
static inline uint64_t lib_SUB_64(Iss *s, uint64_t a, uint64_t b) { return a - b; }
static inline int64_t lib_MACS_64(Iss *s, int64_t a, int32_t b, int32_t c) { return a + (int64_t)b * (int64_t)c; }
static inline int64_t lib_MSUS_64(Iss *s, int64_t a, int32_t b, int32_t c) { return a - (int64_t)b * (int64_t)c; }
static inline uint64_t lib_MACU_64(Iss *s, uint64_t a, uint32_t b, uint32_t c) { return a + (int64_t)b * (int64_t)c; }
static inline uint64_t lib_MSUU_64(Iss *s, uint64_t a, uint32_t b, uint32_t c) { return a - (int64_t)b * (int64_t)c; }

static inline uint64_t lib_SLL_64(Iss *s, uint64_t a, uint64_t b) { return a << b; }
static inline uint64_t lib_SRL_64(Iss *s, uint64_t a, uint64_t b) { return a >> b; }
static inline int64_t lib_SRA_64(Iss *s, int64_t a, uint64_t b) { return a >> b; }
static inline uint64_t lib_ROR_64(Iss *s, uint64_t a, uint64_t b) { return (a >> b) | (a << (32 - b)); }
static inline uint64_t lib_XOR_64(Iss *s, uint64_t a, uint64_t b) { return a ^ b; }
static inline uint64_t lib_OR_64(Iss *s, uint64_t a, uint64_t b) { return a | b; }
static inline uint64_t lib_AND_64(Iss *s, uint64_t a, uint64_t b) { return a & b; }
