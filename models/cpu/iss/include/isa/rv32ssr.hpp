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

// SCFGRI and SCFGWI read and write a value from or to an SSR configuration register. 
// The immediate argument reg specifies the index of the register, ssr specifies which SSR should be accessed. 
// SCFGRI places the read value in rd. 
// SCFGWI moves the value in rs1 to the selected SSR configuration register.

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
    iss->ssr.cfg_write(insn, UIM_GET(1), UIM_GET(0), REG_GET(0));
#endif
    return iss_insn_next(iss, insn, pc);
}

// SCFGR and SCFGW read and write a value from or to an SSR configuration register. 
// The value in register rs2 specifies specifies the address of the register as follows: 
// bits 4 to 0 correspond to ssr and indicate the SSR to be used, 
// and the bits 11 to 5 correspond to reg and indicate the index of the register. 
// SCFGR places the read value in rd. SCFGW moves the value in rs1 to the selected SSR configuration register.

// SSR configration register read
static inline iss_reg_t scfgr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    REG_SET(0, iss->ssr.cfg_read(insn, (REG_GET(0)>>5) & 0x7f, REG_GET(0) & 0x1f));
#endif
    return iss_insn_next(iss, insn, pc);
}

// SSR configration register write
static inline iss_reg_t scfgw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    iss->ssr.cfg_write(insn, (REG_GET(1)>>5) & 0x7f, REG_GET(1) & 0x1f, REG_GET(0));
#endif
    return iss_insn_next(iss, insn, pc);
}

#endif