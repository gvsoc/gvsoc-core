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

#include <Vregfile.hpp>

static inline iss_insn_t *vadd_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VVADD , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));//VLEN = 256 and ELEN = 8 => 32 add are needed and in SPATZ2 we need 16 clk
    return insn->next;
}
static inline iss_insn_t *vadd_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VXADD , REG_IN(0), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vadd_vi_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VIADD , REG_IN(0), SIM_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vsub_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VVSUB , REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));//VLEN = 256 and ELEN = 8 => 32 add are needed and in SPATZ2 we need 16 clk
    return insn->next;
}
static inline iss_insn_t *vsub_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VXSUB , REG_IN(0), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vfmac_vv_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VVFMAC, REG_IN(0), REG_IN(1) , REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vfmac_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VXFMAC, REG_IN(0), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vfmac_vx_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VXFMAC, REG_IN(0), REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}
static inline iss_insn_t *vle8_v_exec(Iss *iss, iss_insn_t *insn){
    LIB_CALL3(lib_VLE8V , REG_GET(0), REG_OUT(0), UIM_GET(0));
    return insn->next;
}