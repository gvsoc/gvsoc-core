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
#include "isa_lib/vint.h"

static inline iss_insn_t *vadd_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vadd_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ADDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vadd_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ADDVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vsub_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vsub_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vrsub_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_RSUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vrsub_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_RSUBVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vand_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ANDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vand_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ANDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vand_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ANDVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vor_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ORVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vor_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ORVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vor_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_ORVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vxor_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_XORVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vxor_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_XORVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vxor_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_XORVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmin_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MINVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmin_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MINVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vminu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MINUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vminu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MINUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmax_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MAXVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmax_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MAXVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmaxu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MAXUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmaxu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MAXUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmul_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vmul_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmulh_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vmulh_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmulhu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vmulhu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmulhsu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vmulhsu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MULHSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmv_v_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MVVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmv_v_x_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MVVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmv_v_i_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MVVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmul_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmul_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmulu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmulu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmulsu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmulsu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMULSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmacc_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmacc_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MACCVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmadd_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MADDVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vmadd_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_MADDVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vnmsac_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_NMSACVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vnmsac_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_NMSACVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vnmsub_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_NMSUBVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vnmsub_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_NMSUBVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmacc_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmacc_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmaccu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmaccu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmaccus_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCUSVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmaccsu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCSUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vwmaccsu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_WMACCSUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslideup_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDEUPVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslideup_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDEUPVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslidedown_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDEDWVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslidedown_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDEDWVI , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslide1up_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDE1UVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vslide1down_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_SLIDE1DVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vdiv_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_DIVVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vdiv_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_DIVVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vdivu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_DIVUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vdivu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_DIVUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vrem_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_REMVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vrem_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_REMVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vremu_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_REMUVV , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vremu_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_REMUVX , REG_IN(1), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

































static inline iss_insn_t *vfmac_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_VVFMAC, REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vfmac_vf_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_VFFMAC, REG_IN(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vle8_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VLE8V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vle16_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VLE16V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vle32_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VLE32V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vle64_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VLE64V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vse8_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VSE8V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vse16_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VSE16V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vse32_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VSE32V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vse64_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VSE64V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}

static inline iss_insn_t *vlse8_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_VLSE8V , REG_GET(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
// static inline iss_insn_t *vlse16_v_exec(Iss *iss, iss_insn_t *insn){
//     LIB_CALL3(lib_VLSE16V , REG_GET(0), REG_OUT(0), UIM_GET(0));
//     return insn->next;
// }
// static inline iss_insn_t *vlse32_v_exec(Iss *iss, iss_insn_t *insn){
//     LIB_CALL3(lib_VLSE32V , REG_GET(0), REG_OUT(0), UIM_GET(0));
//     return insn->next;
// }
// static inline iss_insn_t *vlse64_v_exec(Iss *iss, iss_insn_t *insn){
//     LIB_CALL3(lib_VLSE64V , REG_GET(0), REG_OUT(0), UIM_GET(0));
//     return insn->next;
// }


























static inline iss_insn_t *vfadd_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_VVFADD , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vfadd_vf_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL4(lib_VFFADD , REG_IN(0), REG_GET(1), REG_OUT(0), UIM_GET(0));
    return insn->next;
}






























//                             V 1.0 
// static inline iss_insn_t *vsetvli_exec(Iss *iss, iss_insn_t *insn){
//     REG_SET(0, LIB_CALL8(lib_VSETVLI, REG_IN(0), REG_OUT(0), REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2), UIM_GET(3), UIM_GET(4)));// VLMUL-VSEW-VTA-VMA
//     return insn->next;
// }

//                             V 0.8
static inline iss_insn_t *vsetvli_exec(Iss *iss, iss_insn_t *insn){
    REG_SET(0, LIB_CALL6(lib_VSETVLI, REG_IN(0), REG_OUT(0), REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2)));// VLMUL-VSEW
    return insn->next;
}
static inline iss_insn_t *vsetvl_exec(Iss *iss, iss_insn_t *insn){
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

    return insn->next;
}

// static inline iss_insn_t *csrr_exec(Iss *iss, iss_insn_t *insn)
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
//         return insn->next;
// }