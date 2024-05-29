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

static inline iss_reg_t flb_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return flb_exec(iss, insn, pc);
}

static inline iss_reg_t fsb_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsb_exec(iss, insn, pc);
}

static inline iss_reg_t fmadd_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmadd_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmadd_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmsub_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmsub_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmsub_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fnmsub_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fnmsub_ab_exec(iss, insn, pc);
    }
    else
    {
        return fnmsub_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fnmadd_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fnmadd_ab_exec(iss, insn, pc);
    }
    else
    {
        return fnmadd_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fadd_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fadd_ab_exec(iss, insn, pc);
    }
    else
    {
        return fadd_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsub_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsub_ab_exec(iss, insn, pc);
    }
    else
    {
        return fsub_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmul_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmul_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmul_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fdiv_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fdiv_ab_exec(iss, insn, pc);
    }
    else
    {
        return fdiv_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsqrt_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsqrt_ab_exec(iss, insn, pc);
    }
    else
    {
        return fsqrt_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnj_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnj_ab_exec(iss, insn, pc);
    }
    else
    {
        return fsgnj_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnjn_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnjn_ab_exec(iss, insn, pc);
    }
    else
    {
        return fsgnjn_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fsgnjx_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fsgnjx_ab_exec(iss, insn, pc);
    }
    else
    {
        return fsgnjx_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmin_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmin_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmin_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmax_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmax_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmax_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_w_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_w_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_w_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_wu_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_wu_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_wu_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmv_x_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmv_x_ab_exec(iss, insn, pc);
    }
    else
    {
        return fmv_x_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fmv_b_x_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fmv_ab_x_exec(iss, insn, pc);
    }
    else
    {
        return fmv_b_x_exec(iss, insn, pc);
    }
}

static inline iss_reg_t feq_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return feq_ab_exec(iss, insn, pc);
    }
    else
    {
        return feq_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t flt_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return flt_ab_exec(iss, insn, pc);
    }
    else
    {
        return flt_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fle_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fle_ab_exec(iss, insn, pc);
    }
    else
    {
        return fle_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fclass_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fle_ab_exec(iss, insn, pc);
    }
    else
    {
        return fclass_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_w_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ab_w_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_w_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_wu_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ab_wu_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_wu_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_s_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_s_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_s_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_s_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ab_s_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_s_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_h_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_h_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_h_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_b_h_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_ab_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_ah_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ah_b_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_h_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_ah_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_b_ah_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_h_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_l_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_l_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_l_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_lu_b_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_lu_ab_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_lu_b_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_l_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ab_l_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_l_exec(iss, insn, pc);
    }
}

static inline iss_reg_t fcvt_b_lu_exec_switch(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->fmode == 3)
    {
        return fcvt_ab_lu_exec(iss, insn, pc);
    }
    else
    {
        return fcvt_b_lu_exec(iss, insn, pc);
    }
}
