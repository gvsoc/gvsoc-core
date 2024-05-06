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

#ifndef __CPU_ISS_RVF_HPP
#define __CPU_ISS_RVF_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t fp_offload_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    if (iss->snitch & !iss->fp_ss)
    {
        // Register int destination in scoreboard
        // Scoreboard stall if there's integer operand data dependency.
        insn->data_arga = REG_GET(0);
        // insn->data_argb = REG_GET(1);

        // Record the memory addess for offloaded load/store instrution, 
        // stall the following instruction which gets access to same memory block.
        bool lsu_label = strstr(insn->decoder_item->u.insn.label, "flw")
                    || strstr(insn->decoder_item->u.insn.label, "fsw")
                    || strstr(insn->decoder_item->u.insn.label, "fld")
                    || strstr(insn->decoder_item->u.insn.label, "fsd");
        if (lsu_label)
        {
            iss->mem_map = REG_GET(0) + SIM_GET(0);
            iss->mem_pc = pc;
            iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "FP instruction memory access address: 0x%llx\n", iss->mem_map);
        }

        // Send an IO request to check whether the subsystem is ready for offloading.
        bool acc_req_ready = iss->check_state(insn);
        iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Integer side receives acceleration request handshaking signal: %d\n", acc_req_ready);

        // Not increment pc if there's dependency or stall
        // Implement "return pc" also possible to realize parallelism, can iterate at pc instruction in instr_event,
        // but lose a bit efficiency because the instruction will be unnecessarily decoded every cycle.
        // If not ready, stay at current PC and fetch the same instruction next cycle.
        if (!acc_req_ready) 
        {
            iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Stall at current instruction\n");
            // Start from offloading handler directly in the next cycle.
            return pc;
        }

        // If ready, send offload request.
        if (acc_req_ready)
        {
            // Pass value related to integer regfile from integer core to subsystem.
            // Store the pointer of integer register file and scoreboard inside the instruction.
            insn->reg_addr = &iss->regfile.regs[0];
            #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
            insn->scoreboard_reg_timestamp_addr = &iss->regfile.scoreboard_reg_timestamp[0];
            #endif

            // Set the integer register to invalid for data dependency, if it's the output of offloading fp instruction.
            if (!insn->out_regs_fp[0] && insn->out_regs[0] != 0xFF)
            {
                #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
                iss->regfile.scoreboard_reg_valid[insn->out_regs[0]] = false;
                #endif   
            }

            // Send out request containing the instruction.
            int stall = iss->handle_req(insn, pc, false);

            iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Total number of stall cycles: %d\n", iss->exec.instr_event.stall_cycle_get());
        }
    }
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->lsu.load_float<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_float_perf<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->csr.mstatus.fs == 0)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    if (iss->lsu.store_float<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->csr.mstatus.fs == 0)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_float_perf<uint32_t>(insn, REG_GET(0) + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_madd_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_msub_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_nmsub_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fnmadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_nmadd_round, FREG_GET(0), FREG_GET(1), FREG_GET(2), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fadd_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_add_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsub_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_sub_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmul_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_mul_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fdiv_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL3(lib_flexfloat_div_round, FREG_GET(0), FREG_GET(1), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsqrt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sqrt_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnj_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnj, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjn_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjn, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fsgnjx_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_sgnjx, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmin_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_min, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmax_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_max, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_w_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_w_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_wu_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_wu_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_x_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_x_ff, FREG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fmv_s_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL1(lib_flexfloat_fmv_ff_x, REG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t feq_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_eq, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t flt_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_lt, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fle_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_le, FREG_GET(0), FREG_GET(1), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fclass_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL1(lib_flexfloat_class, FREG_GET(0), 8, 23));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_w_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_wu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_wu_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

//
// RV64F
//
static inline iss_reg_t fcvt_l_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_l_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_lu_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_lu_ff_round, FREG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_l_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_l_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fcvt_s_lu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    FREG_SET(0, LIB_FF_CALL2(lib_flexfloat_cvt_ff_lu_round, REG_GET(0), 8, 23, UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

//
// COMPRESSED INSTRUCTIONS
//
static inline iss_reg_t c_fsw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_fsw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fsw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fswsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_fswsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fsw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fsdsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fsd_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_fsdsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fsd_exec(iss, insn, pc);
}

static inline iss_reg_t c_flw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return flw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_flw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return flw_exec(iss, insn, pc);
}

static inline iss_reg_t c_flwsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return flw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_flwsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return flw_exec(iss, insn, pc);
}

static inline iss_reg_t c_fldsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return fld_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_fldsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return fld_exec(iss, insn, pc);
}

#endif
