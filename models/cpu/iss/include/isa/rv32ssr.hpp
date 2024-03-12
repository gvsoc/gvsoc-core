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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#ifndef __CPU_ISS_RV32SSR_HPP
#define __CPU_ISS_RV32SSR_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

// SSR configration register read
static inline iss_reg_t scfgri_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, iss->ssr.cfg_read(insn, UIM_GET(1), UIM_GET(0)));
#endif
    return iss_insn_next(iss, insn, pc);
}

// SSR configration register write
static inline iss_reg_t scfgwi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    iss->exec.trace.msg("REG_GET(0) 0x%llx, UIM_GET(0) %d, UIM_GET(1) %d\n", REG_GET(0), UIM_GET(0), UIM_GET(1));
    iss->ssr.cfg_write(insn, UIM_GET(1), UIM_GET(0), REG_GET(0));
#endif
    return iss_insn_next(iss, insn, pc);
}

// Not needed currently
static inline iss_reg_t scfgr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH

#endif
    return iss_insn_next(iss, insn, pc);
}

// Not needed currently
static inline iss_reg_t scfgw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH

#endif
    return iss_insn_next(iss, insn, pc);
}

#endif