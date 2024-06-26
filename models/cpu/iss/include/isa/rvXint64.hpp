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

#ifndef __CPU_ISS_RVXINT64_HPP
#define __CPU_ISS_RVXINT64_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t add_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_ADD_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sub_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_SUB_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sll_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_SLL_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slt_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, (int64_t)REG64_GET(0) < (int64_t)REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sltu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, REG64_GET(0) < REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t xor_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_XOR_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srl_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_SRL_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sra_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_SRA_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t or_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_OR_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t and_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_AND_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slli_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_SLL_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srli_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_SRL_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srai_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_SRA_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t addi_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_ADD_64, REG64_GET(0), SIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slti_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG_SET(0, (int64_t)REG64_GET(0) < SIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sltiu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG_SET(0, REG64_GET(0) < UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t xori_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_XOR_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t ori_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_OR_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t andi_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL2(lib_AND_64, REG64_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_abs_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, LIB_CALL1(lib_ABS_64, REG64_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_seq_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, REG64_GET(0) == REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_sne_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, REG64_GET(0) != REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_slet_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, (int64_t)REG64_GET(0) <= (int64_t)REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_sletu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, REG64_GET(0) <= REG64_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_min_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MINS_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_minu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MINU_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_max_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MAXS_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_maxu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MAXU_64, REG64_GET(0), REG64_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_cnt_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG_SET(0, LIB_CALL1(lib_CNT_64, REG64_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_exths_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_signed_value64(REG_GET(0), 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_exthz_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_field64(REG_GET(0), 0, 16));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_extbs_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_signed_value64(REG_GET(0), 8));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_extbz_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_field64(REG_GET(0), 0, 8));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_extws_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_signed_value64(REG_GET(0), 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_extwz_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    REG64_SET(0, iss_get_field64(REG_GET(0), 0, 32));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_mac_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(2));
    REG64_SET(0, LIB_CALL3(lib_MACS_64, REG64_GET(2), REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_msu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(2));
    REG64_SET(0, LIB_CALL3(lib_MSUS_64, REG64_GET(2), REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_macu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(2));
    REG64_SET(0, LIB_CALL3(lib_MACU_64, REG64_GET(2), REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_msuu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(2));
    REG64_SET(0, LIB_CALL3(lib_MSUU_64, REG64_GET(2), REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_muls_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MULS_64, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_mulu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG64_SET(0, LIB_CALL2(lib_MULU_64, REG_GET(0), REG_GET(1)));

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_mulsh_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_MULS_64, REG_GET(0), REG_GET(1)) >> 32);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t p_muluh_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(0));
    iss->regfile.memcheck_merge64(REG_OUT(0), REG_IN(1));
    REG_SET(0, LIB_CALL2(lib_MULU_64, REG_GET(0), REG_GET(1)) >> 32);

    return iss_insn_next(iss, insn, pc);
}

#endif
