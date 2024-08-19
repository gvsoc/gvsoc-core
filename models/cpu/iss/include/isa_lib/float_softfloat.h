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
#include "cpu/iss/softfloat/softfloat.h"

static int is_nan_32(uint32_t f) {
    uint32_t exp = (f >> 23) & 0xFF;
    uint32_t frac = f & 0x7FFFFF;
    return (exp == 0xFF && frac != 0);
}

static uint32_t sanitize_32(uint32_t value)
{
    if (is_nan_32(value))
    {
      return 0x7FC00000;
    }
    return value;
}

static inline void float_set_rounding_mode(Iss *iss, int mode)
{
    if (mode == 7)
    {
        mode = iss->csr.fcsr.frm;
    }
    iss->core.float_mode = mode;
}

static inline uint32_t float_add_32(Iss *iss, uint32_t a, uint32_t b, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_add(iss, {.v=a}, {.v=b}).v);
}

static inline uint32_t float_sub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_sub(iss, {.v=a}, {.v=b}).v);
}

static inline uint32_t float_madd_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_mulAdd(iss, {.v=a}, {.v=b}, {.v=c}).v);
}

static inline uint32_t float_msub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_mulSub(iss, {.v=a}, {.v=b}, {.v=c}).v);
}

static inline uint32_t float_nmadd_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_NmulSub(iss, {.v=a}, {.v=b}, {.v=c}).v);
}

static inline uint32_t float_nmsub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return sanitize_32(f32_NmulAdd(iss, {.v=a}, {.v=b}, {.v=c}).v);
}



static inline uint32_t float_madd_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return f16_mulAdd(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_msub_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return f16_mulSub(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_nmadd_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return f16_NmulSub(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_nmsub_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return f16_NmulAdd(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}



static inline uint32_t float_madd_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return bf16_mulAdd(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_msub_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return bf16_mulSub(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_nmadd_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return bf16_NmulSub(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}

static inline uint32_t float_nmsub_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    float_set_rounding_mode(iss, mode);
    return bf16_NmulAdd(iss, {.v=(uint16_t)a}, {.v=(uint16_t)b}, {.v=(uint16_t)c}).v;
}
