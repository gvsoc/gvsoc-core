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

#define REG_GET(reg) (iss->regfile.get_reg(insn->in_regs[reg]))
#define REG_GET_UNTIMED(reg) (iss->regfile.get_reg_untimed(insn->in_regs[reg]))
#define REG_SET(reg,val) (iss->regfile.set_reg(insn->out_regs[reg], val))
#define IN_REG_SET(reg,val) iss->regfile.set_reg(insn->in_regs[reg], val)

#define REG64_GET(reg) iss->regfile.get_reg64(insn->in_regs[reg])
#define REG64_SET(reg,val) iss->regfile.set_reg64(insn->out_regs[reg], val)

#define SIM_GET(index) insn->sim[index]
#define UIM_GET(index) insn->uim[index]

#if defined(ISS_SINGLE_REGFILE) || defined(CONFIG_GVSOC_ISS_ZDINX) || defined(CONFIG_GVSOC_ISS_FDINX)
#if defined(CONFIG_GVSOC_ISS_ZDINX)
#define FREG_GET(reg) REG64_GET(reg)
#else
#define FREG_GET(reg) REG_GET(reg)
#endif
#define FREG_OUT_GET(reg) (iss->regfile.get_reg(insn->out_regs[reg]))
#if defined(CONFIG_GVSOC_ISS_ZDINX)
#define FREG_SET(reg,val) REG64_SET(reg, val)
#else
#define FREG_SET(reg,val) REG_SET(reg, val)
#endif
#define FREG32_GET(reg) REG_GET(reg)
#define FREG32_SET(reg,val) REG_SET(reg, val)
#else
#define FREG32_GET(reg) (iss->regfile.get_freg(insn->in_regs[reg]))
#define FREG_GET(reg) (iss->regfile.get_freg(insn->in_regs[reg]))
#define FREG_OUT_GET(reg) (iss->regfile.get_freg(insn->out_regs[reg]))
#define FREG32_SET(reg,val) (iss->regfile.set_freg(insn->out_regs[reg], val))
#define FREG_SET(reg,val) (iss->regfile.set_freg(insn->out_regs[reg], val))
#endif

#ifdef CONFIG_GVSOC_ISS_CV32E40P
// CV32E40P RTL: a trapped instruction has no architectural side effects, and
// an FP regfile write (fregs_we_i) forces mstatus.FS=Dirty
// (cv32e40p_cs_registers.sv). Redefine the write-back macros accordingly: any
// destination write -- integer or FP -- is killed when the instruction raised
// an exception mid-execution (reserved FP rounding modes raise
// illegal-instruction inside the value expression, see setFFRoundingMode;
// fcvt.w.s and friends land in an integer rd, so guarding only the FP macros
// left that path writing a trapped result), and FP writes additionally
// promote FS. Kept in this isolated block (not inline above) so the shared
// definitions preprocess back to upstream byte-for-byte without this define.
// The write must target the same bank as the definitions above: the integer
// regfile on ISS_SINGLE_REGFILE (ZFINX) builds, the FP regfile otherwise
// (FPU=1 ZFINX=0 builds have a separate FP bank; routing these writes to
// the integer bank there corrupts the integer registers).
// The value expression MUST be evaluated before the guard: the raise happens
// inside it, so testing has_exception first would always see the pre-raise
// state.
#undef REG_SET
#define REG_SET(reg,val) \
    do { iss_reg_t int_wb_val_ = (val); \
         if (!iss->exec.has_exception) { \
             iss->regfile.set_reg(insn->out_regs[reg], int_wb_val_); \
         } } while (0)
#undef FREG_SET
#undef FREG32_SET
#ifdef ISS_SINGLE_REGFILE
#define FREG_SET(reg,val) \
    do { iss_reg_t fp_wb_val_ = (val); \
         if (!iss->exec.has_exception) { \
             REG_SET(reg, fp_wb_val_); \
             iss->csr.fp_state_dirty(); \
         } } while (0)
#else
#define FREG_SET(reg,val) \
    do { iss_freg_t fp_wb_val_ = (val); \
         if (!iss->exec.has_exception) { \
             iss->regfile.set_freg(insn->out_regs[reg], fp_wb_val_); \
             iss->csr.fp_state_dirty(); \
         } } while (0)
#endif
#define FREG32_SET(reg,val) FREG_SET(reg, val)
#endif
#endif
