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

#ifdef CONFIG_GVSOC_ISS_V2
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss_v2/include/isa_lib/macros.h"
#else
#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"
#endif

static inline uint32_t float_add_32(Iss *iss, uint32_t a, uint32_t b, uint32_t mode)
{
    return LIB_FF_CALL3(lib_flexfloat_add_round, a, b, 8, 23, mode);
}

static inline uint32_t float_sub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t mode)
{
    return LIB_FF_CALL3(lib_flexfloat_sub_round, a, b, 8, 23, mode);
}

static inline uint32_t float_madd_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_madd_round, a, b, c, 8, 23, mode);
}

static inline uint32_t float_msub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_msub_round, a, b, c, 8, 23, mode);
}

static inline uint32_t float_nmadd_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmadd_round, a, b, c, 8, 23, mode);
}

static inline uint32_t float_nmsub_32(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmsub_round, a, b, c, 8, 23, mode);
}

static inline uint32_t float_madd_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_madd_round, a, b, c, 5, 10, mode);
}

static inline uint32_t float_msub_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_msub_round, a, b, c, 5, 10, mode);
}

static inline uint32_t float_nmadd_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmadd_round, a, b, c, 5, 10, mode);
}

static inline uint32_t float_nmsub_16(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmsub_round, a, b, c, 5, 10, mode);
}

static inline uint32_t float_madd_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_madd_round, a, b, c, 8, 7, mode);
}

static inline uint32_t float_msub_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_msub_round, a, b, c, 8, 7, mode);
}

static inline uint32_t float_nmadd_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmadd_round, a, b, c, 8, 7, mode);
}

static inline uint32_t float_nmsub_16alt(Iss *iss, uint32_t a, uint32_t b, uint32_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_nmsub_round, a, b, c, 8, 7, mode);
}

static inline uint64_t float_madd_64(Iss *iss, uint64_t a, uint64_t b, uint64_t c, uint32_t mode)
{
    return LIB_FF_CALL4(lib_flexfloat_madd_round, a, b, c, 11, 52, mode);
}
