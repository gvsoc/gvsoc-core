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

#pragma once

//#include "spatz.hpp"
#include "cpu/iss/include/isa_lib/vint.h"

static inline iss_reg_t vadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vadd_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ADDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vadd_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ADDVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_RSUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrsub_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_RSUBVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ANDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ANDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ANDVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ORVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ORVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_ORVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_XORVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_XORVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_XORVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmin_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MINVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmin_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MINVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vminu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MINUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vminu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MINUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmax_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MAXVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmax_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MAXVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmaxu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MAXUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmaxu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MAXUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmul_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulh_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulh_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulhu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulhu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulhsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulhsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MULHSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MVVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MVVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_i_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MVVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_s_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MVSX , REG_IN(1), REG_GET(0) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_x_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    REG_SET(0,LIB_CALL2(lib_MVXS, REG_IN(1), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmul_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMULSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmacc_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MACCVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmadd_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_MADDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_NMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsac_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_NMSACVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_NMSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_NMSUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmacc_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccus_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCUSVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_WMACCSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredsum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDSUMVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredand_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDANDVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredor_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDORVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredxor_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDXORVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmin_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDMINVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredminu_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDMINUVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmax_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDMAXVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmaxu_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REDMAXUVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}


static inline iss_reg_t vslideup_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDEUPVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslideup_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDEUPVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslidedown_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDEDWVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslidedown_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDEDWVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslide1up_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDE1UVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslide1down_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_SLIDE1DVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdiv_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_DIVVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vdiv_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_DIVVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdivu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_DIVUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vdivu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_DIVUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrem_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REMVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vrem_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REMVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vremu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REMUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vremu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_REMUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

































// static inline iss_reg_t vfmac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
//     LIB_CALL4(lib_VVFMAC, REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
//     return iss_insn_next(iss, insn, pc);
// }
// static inline iss_reg_t vfmac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
//     LIB_CALL4(lib_VFFMAC, REG_IN(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
//     return iss_insn_next(iss, insn, pc);
// }
static inline iss_reg_t vle8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VLE8V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vle16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VLE16V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vle32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VLE32V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vle64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VLE64V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VSE8V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VSE16V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VSE32V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VSE64V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl1re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl1re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl1re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl1re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl2re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl2re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl2re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl2re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl4re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl4re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl4re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl4re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl8re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl8re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl8re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vl8re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VL8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vs1r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VS1RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vs2r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VS2RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vs4r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VS4RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vs8r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL3(lib_VS8RV , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}



static inline iss_reg_t vluxei8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VLUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 8);
    // LIB_CALL4(lib_VLUXEI8V , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vluxei16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VLUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 16);
    // LIB_CALL4(lib_VLUXEI16V , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vluxei32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VLUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 32);
    // LIB_CALL4(lib_VLUXEI32V , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vluxei64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VLUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 64);
    // LIB_CALL4(lib_VLUXEI64V , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsuxei8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VSUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 8);
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsuxei16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VSUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 16);
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsuxei32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VSUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 32);
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsuxei64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL5(lib_VSUXEIV , REG_GET(0), REG_IN(1), REG_OUT(0), UIM_GET(0), 64);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vlse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    // printf("VLSE8\n");
    LIB_CALL4(lib_VLSE8V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vlse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    // printf("VLSE16\n");
    LIB_CALL4(lib_VLSE16V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vlse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VLSE32V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vlse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VLSE64V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VSSE8V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VSSE16V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VSSE32V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vsse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_VSSE64V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

























static inline iss_reg_t vfadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FADDVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSUBVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfrsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FRSUBVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMINVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmin_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMINVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMAXVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmax_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMAXVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmul_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMULVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMACCVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfnmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMACCVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMSACVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfnmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMSACVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMADDVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfnmadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMADDVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfmsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMSUBVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfnmsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNMSUBVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredmax_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FREDMAXVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredmin_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FREDMINVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredsum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FREDSUMVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredosum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FREDSUMVS , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}



static inline iss_reg_t vfwadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWADDVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_wv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWADDWV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_wf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWADDWF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}


static inline iss_reg_t vfwsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWSUBVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwsub_wv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWSUBWV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwsub_wf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWSUBWF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwmul_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMULVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMACCVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWMSACVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWNMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfwnmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FWNMSACVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfsgnj_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJNVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfsgnjn_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJNVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJXVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vfsgnjx_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FSGNJXVF , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTXUFV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTXFV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_f_xu_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTFXUV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_f_x_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTFXV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_rtz_xu_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTRTZXUFV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_rtz_x_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FCVTRTZXFV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_xu_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTXUFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_x_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTXFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_xu_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTFXUW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_x_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTFXW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTFFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rod_f_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTRODFFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rtz_xu_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTRTZXUFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rtz_x_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FNCVTRTZXFW , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}











static inline iss_reg_t vfmv_v_f_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMVVF , REG_IN(1), REG_GET(0) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_s_f_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    LIB_CALL4(lib_FMVSF , REG_IN(1), REG_GET(0) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_f_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    REG_SET(0,LIB_CALL2(lib_FMVFS, REG_IN(1), UIM_GET(0)));
    // LIB_CALL4(lib_FMVFS , REG_IN(1), REG_GET(0) , REG_OUT(0), UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}




//                             V 1.0 
static inline iss_reg_t vsetvli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    // REG_SET(0, LIB_CALL8(lib_VSETVLI, REG_IN(0), REG_OUT(0), REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2), UIM_GET(3), UIM_GET(4)));// VLMUL-VSEW-VTA-VMA
    REG_SET(0, LIB_CALL6(lib_VSETVLI, REG_IN(0), REG_OUT(0), REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2)));// VLMUL-VSEW
    return iss_insn_next(iss, insn, pc);
}

//                             V 0.8
// static inline iss_reg_t vsetvli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
//     REG_SET(0, LIB_CALL6(lib_VSETVLI, REG_IN(0), REG_OUT(0), REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2)));// VLMUL-VSEW
//     return iss_insn_next(iss, insn, pc);
// }
static inline iss_reg_t vsetvl_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc){
    //printf("vsetvl=>vsetvl_exec\n");
    REG_SET(0, LIB_CALL4(lib_VSETVL, REG_IN(0), REG_OUT(0), REG_GET(0), REG_GET(1)));// VLMUL-VSEW
    //LIB_CALL4(lib_VSETVL, REG_IN(0), REG_OUT(0), REG_GET(0), REG_GET(1));// VLMUL-VSEW
    //LIB_CALL4(lib_VSETVL, REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    // printf("rs1idx_vsetval = %d\n",REG_IN(0));
    // printf("rs2idx_vsetval = %d\n",REG_IN(1));
    // printf("rs1val_vsetval = %ld\n",REG_GET(0));
    // printf("rs2val_vsetval = %ld\n",REG_GET(1));
    //lib_VSETVL(iss,REG_IN(0),REG_OUT(0), REG_GET(0), REG_GET(1));
    //printf("vsetvl=>vsetvl_exec RETURN\n");

    return iss_insn_next(iss, insn, pc);
}

// static inline iss_reg_t csrr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
// {
//     iss_reg_t value;
//     iss_reg_t reg_value = REG_GET(0);

//     if (UIM_GET(0) == 3104)
//     {
//         if (insn->out_regs[0] != 0)
//             REG_SET(0, iss->spatz.vl);
//     }else if(UIM_GET(0) == 3105){
//         if (insn->out_regs[0] != 0)
//             REG_SET(0, iss->spatz.vtype);
//     }else{

//     }
//         return iss_insn_next(iss, insn, pc);
// }