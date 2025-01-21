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
 * Authors: Stefan Mach, ETH (smach@iis.ee.ethz.ch)
 *          Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#pragma once

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"
#include "rv32Xfvec.hpp"

static inline iss_reg_t flh_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return flh_exec(iss, insn, pc);
}

static inline iss_reg_t fsh_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsh_exec(iss, insn, pc);
}

static inline iss_reg_t fmadd_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmadd_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmadd_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmsub_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmsub_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmsub_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fnmsub_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fnmsub_ah_exec(iss, insn, pc);
    }
    else
    {
        return fnmsub_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fnmadd_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fnmadd_ah_exec(iss, insn, pc);
    }
    else
    {
        return fnmadd_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fadd_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fadd_ah_exec(iss, insn, pc);
    }
    else
    {
        return fadd_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsub_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsub_ah_exec(iss, insn, pc);
    }
    else
    {
        return fsub_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmul_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmul_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmul_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fdiv_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fdiv_ah_exec(iss, insn, pc);
    }
    else
    {
        return fdiv_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsqrt_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsqrt_ah_exec(iss, insn, pc);
    }
    else
    {
        return fsqrt_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnj_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnj_ah_exec(iss, insn, pc);
    }
    else
    {
        return fsgnj_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnjn_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnjn_ah_exec(iss, insn, pc);
    }
    else
    {
        return fsgnjn_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnjx_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnjx_ah_exec(iss, insn, pc);
    }
    else
    {
        return fsgnjx_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmin_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmin_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmin_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmax_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmax_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmax_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_w_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_w_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_w_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_wu_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_wu_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_wu_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmv_x_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmv_x_ah_exec(iss, insn, pc);
    }
    else
    {
        return fmv_x_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmv_h_x_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmv_ah_x_exec(iss, insn, pc);
    }
    else
    {
        return fmv_h_x_exec(iss, insn, pc);
    }
}

static inline iss_reg_t feq_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return feq_ah_exec(iss, insn, pc);
    }
    else
    {
        return feq_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t flt_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return flt_ah_exec(iss, insn, pc);
    }
    else
    {
        return flt_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fle_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fle_ah_exec(iss, insn, pc);
    }
    else
    {
        return fle_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fclass_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fle_ah_exec(iss, insn, pc);
    }
    else
    {
        return fclass_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_w_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_w_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_w_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_wu_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_wu_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_wu_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_s_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_s_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_s_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_s_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_s_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_s_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_l_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_l_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_l_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_lu_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_lu_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_lu_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_l_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_l_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_l_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_lu_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_lu_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_lu_exec(iss, insn, pc);
    }
}

//
// with Xf16alt stubs
//

VF_ALT_STUB(vfadd, h, ah)
VF_ALT_STUB(vfsub, h, ah)
VF_ALT_STUB(vfmul, h, ah)
VF_ALT_STUB(vfdiv, h, ah)
VF_ALT_STUB(vfmin, h, ah)
VF_ALT_STUB(vfmax, h, ah)
VF_ALT_STUB_NO_R(vfsqrt, h, ah)
VF_ALT_STUB(vfmac, h, ah)
VF_ALT_STUB(vfmre, h, ah)
VF_ALT_STUB(vfsgnj, h, ah)
VF_ALT_STUB(vfsgnjn, h, ah)
VF_ALT_STUB(vfsgnjx, h, ah)
VF_ALT_STUB(vfeq, h, ah)
VF_ALT_STUB(vfne, h, ah)
VF_ALT_STUB(vflt, h, ah)
VF_ALT_STUB(vfge, h, ah)
VF_ALT_STUB(vfle, h, ah)
VF_ALT_STUB(vfgt, h, ah)
VF_ALT_STUB_NO_R(vfcpka, h_s, ah_s)
VF_ALT_STUB_NO_R(vfcpkb, h_s, ah_s)
VF_ALT_STUB_NO_R(vfclass, h, ah)
VF_ALT_STUB_NO_R(vfsum, h, ah)
VF_ALT_STUB_NO_R(vfnsum, h, ah)
VF_ALT_STUB_NO_R(vfsumex, s_h, s_ah)
VF_ALT_STUB_NO_R(vfnsumex, s_h, s_ah)
