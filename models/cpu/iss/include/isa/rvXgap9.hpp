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

#ifndef __CPU_ISS_RVXCPLX_HPP
#define __CPU_ISS_RVXCPLX_HPP

#ifdef CONFIG_GVSOC_ISS_V2
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss_v2/include/isa_lib/macros.h"
#else
#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"
#endif

static inline iss_reg_t gap9_CPLXMUL_H_I_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_I, REG_GET(0), REG_GET(1), REG_GET(2), 0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_I_DIV2_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_I, REG_GET(0), REG_GET(1), REG_GET(2), 1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_I_DIV4_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_I, REG_GET(0), REG_GET(1), REG_GET(2), 2));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_I_DIV8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_I, REG_GET(0), REG_GET(1), REG_GET(2), 3));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_R_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_R, REG_GET(0), REG_GET(1), REG_GET(2), 0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_R_DIV2_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_R, REG_GET(0), REG_GET(1), REG_GET(2), 1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_R_DIV4_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_R, REG_GET(0), REG_GET(1), REG_GET(2), 2));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLXMUL_H_R_DIV8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(2));
    REG_SET(0, LIB_CALL4(lib_CPLXMUL_H_R, REG_GET(0), REG_GET(1), REG_GET(2), 3));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_CPLX_CONJ_16_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    REG_SET(0, LIB_CALL1(lib_CPLX_CONJ_16, REG_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_ROTMJ_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_16_ROTMJ, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_ROTMJ_DIV2_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_16_ROTMJ_DIV2, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_ROTMJ_DIV4_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_16_ROTMJ_DIV4, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_ROTMJ_DIV8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_16_ROTMJ_DIV8, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_DIV2_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_int16_t_to_int32_t_div2, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_DIV4_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_int16_t_to_int32_t_div4, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_ADD_16_DIV8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_ADD_int16_t_to_int32_t_div8, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_SUB_16_DIV2_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_SUB_int16_t_to_int32_t_div2, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_SUB_16_DIV4_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_SUB_int16_t_to_int32_t_div4, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_SUB_16_DIV8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_SUB_int16_t_to_int32_t_div8, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_VEC_PACK_SC_H_16_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_VEC_PACK_SC_H_16, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t gap9_BITREV_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge(REG_OUT(0), REG_IN(0));
    REG_SET(0, LIB_CALL3(lib_BITREV, REG_GET(0), UIM_GET(0), UIM_GET(1) + 1));
    return iss_insn_next(iss, insn, pc);
}

#endif
