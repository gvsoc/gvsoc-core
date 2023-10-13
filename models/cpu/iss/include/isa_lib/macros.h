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

#ifndef __ISA_LIB_MACROS_HPP
#define __ISA_LIB_MACROS_HPP

#define LIB_CALL1(name, s0) name(iss, s0)
#define LIB_CALL2(name, s0, s1) name(iss, s0, s1)
#define LIB_CALL3(name, s0, s1, s2) name(iss, s0, s1, s2)
#define LIB_CALL4(name, s0, s1, s2, s3) name(iss, s0, s1, s2, s3)
#define LIB_CALL5(name, s0, s1, s2, s3, s4) name(iss, s0, s1, s2, s3, s4)
#define LIB_CALL6(name, s0, s1, s2, s3, s4, s5) name(iss, s0, s1, s2, s3, s4, s5)

#define LIB_FF_CALL1(name, s0, s1, s2) LIB_CALL3(name, s0, s1, s2)
#define LIB_FF_CALL2(name, s0, s1, s2, s3) LIB_CALL4(name, s0, s1, s2, s3)
#define LIB_FF_CALL3(name, s0, s1, s2, s3, s4) LIB_CALL5(name, s0, s1, s2, s3, s4)
#define LIB_FF_CALL4(name, s0, s1, s2, s3, s4, s5) LIB_CALL6(name, s0, s1, s2, s3, s4, s5)

#define REG_IN(reg) (insn->in_regs[reg])
#define REG_OUT(reg) (insn->out_regs[reg])

#define REG_IN_REF(reg) (insn->in_regs_ref[reg])
#define REG_OUT_REF(reg) (insn->out_regs_ref[reg])

#define REG_GET(reg) (*(iss_reg_t *)insn->in_regs_ref[reg])
#define REG_SET(reg,val) (*(iss_reg_t *)insn->out_regs_ref[reg] = (val))
#define IN_REG_SET(reg,val) iss->regfile.set_reg(insn->in_regs[reg], val)

#define REG64_GET(reg) iss->regfile.get_reg64(insn->in_regs[reg])
#define REG64_SET(reg,val) iss->regfile.set_reg64(insn->out_regs[reg], val)

#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#ifdef ISS_SINGLE_REGFILE
#define FREG_GET(reg) REG_GET(reg)
#define FREG_SET(reg,val) REG_SET(reg, val)
#else
#define FREG_GET(reg) (*(iss_freg_t *)insn->in_regs_ref[reg])
#define FREG_SET(reg,val) (*(iss_freg_t *)insn->out_regs_ref[reg] = (val))
#endif

#endif
