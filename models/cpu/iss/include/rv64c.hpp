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

#include "rvd.hpp"

static inline iss_reg_t c_addiw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return addiw_exec(iss, insn, pc);
}

static inline iss_reg_t c_addiw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return addiw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fld_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fld_exec(iss, insn, pc);
}

static inline iss_reg_t c_fld_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fld_exec(iss, insn, pc);
}

static inline iss_reg_t c_ld_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return ld_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_ld_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return ld_exec(iss, insn, pc);
}

static inline iss_reg_t c_fsd_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsd_exec(iss, insn, pc);
}

static inline iss_reg_t c_fsd_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fsd_exec(iss, insn, pc);
}

static inline iss_reg_t c_sd_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return sd_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_sd_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return sd_exec(iss, insn, pc);
}

static inline iss_reg_t c_subw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return subw_exec(iss, insn, pc);
}

static inline iss_reg_t c_subw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return subw_exec(iss, insn, pc);
}

static inline iss_reg_t c_addw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return addw_exec(iss, insn, pc);
}

static inline iss_reg_t c_addw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return addw_exec(iss, insn, pc);
}

static inline iss_reg_t c_ldsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return ld_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_ldsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return ld_exec(iss, insn, pc);
}

static inline iss_reg_t c_sdsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return sd_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_sdsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return sd_exec(iss, insn, pc);
}

/***********************************************************************/
static inline iss_insn_t *c_fsdsp_exec_fast(Iss *iss, iss_insn_t *insn)
{
    return sd_exec_fast(iss, insn);
}

static inline iss_insn_t *c_fsdsp_exec(Iss *iss, iss_insn_t *insn)
{
    iss->timing.event_rvc_account(1);
    return sd_exec(iss, insn);
}

static inline iss_insn_t *c_fldsp_exec_fast(Iss *iss, iss_insn_t *insn)
{
    return ld_exec_fast(iss, insn);
}

static inline iss_insn_t *c_fldsp_exec(Iss *iss, iss_insn_t *insn)
{
    iss->timing.event_rvc_account(1);
    return ld_exec(iss, insn);
}

/***********************************************************************/
