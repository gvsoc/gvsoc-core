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

#ifndef __CPU_ISS_RV32I_HPP
#define __CPU_ISS_RV32I_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

static inline iss_reg_t lui_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, UIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline void lui_decode(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The immediate is extended to the xlen for rv64
    insn->uim[0] = get_signed_value(insn->uim[0], 32);
}

static inline iss_reg_t auipc_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, get_signed_value(insn->uim[0], 32) + pc);
    return iss_insn_next(iss, insn, pc);
}

static inline void auipc_decode(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
}

static inline iss_reg_t jal_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    unsigned int D = insn->out_regs[0];
    if (D != 0)
        REG_SET(0, pc + insn->size);
    iss->timing.stall_jump_account();
    return pc + insn->sim[0];
}

static inline iss_reg_t jal_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jal_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t jal_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jal_exec_common(iss, insn, pc, 1);
}

static inline void jal_decode(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
}

static inline iss_reg_t jalr_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    iss_reg_t next_pc = (insn->sim[0] + iss->regfile.get_reg(insn->in_regs[0])) & ~1;
    unsigned int D = insn->out_regs[0];
    if (D != 0)
        REG_SET(0, pc + insn->size);
    iss->timing.stall_jump_account();

    return next_pc;
}

static inline iss_reg_t jalr_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jalr_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t jalr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jalr_exec_common(iss, insn, pc, 1);
}

static inline void bxx_decode(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
}

static inline iss_reg_t beq_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if (REG_GET(0) == REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t beq_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return beq_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t beq_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return beq_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t bne_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if (REG_GET(0) != REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t bne_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bne_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t bne_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bne_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t blt_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if ((iss_sim_t)REG_GET(0) < (iss_sim_t)REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t blt_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return blt_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t blt_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return blt_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t bge_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if ((iss_sim_t)REG_GET(0) >= (iss_sim_t)REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t bge_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bge_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t bge_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bge_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t bltu_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if (REG_GET(0) < REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t bltu_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bltu_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t bltu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bltu_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t bgeu_exec_common(Iss *iss, iss_insn_t *insn, iss_reg_t pc, int perf)
{
    if (REG_GET(0) >= REG_GET(1))
    {
        iss->timing.stall_taken_branch_account();
        return pc + SIM_GET(0);
    }
    else
    {
        if (perf)
        {
            iss->timing.event_branch_account(1);
        }
        return iss_insn_next(iss, insn, pc);
    }
}

static inline iss_reg_t bgeu_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bgeu_exec_common(iss, insn, pc, 0);
}

static inline iss_reg_t bgeu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bgeu_exec_common(iss, insn, pc, 1);
}

static inline iss_reg_t lb_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load_signed(insn, REG_GET(0) + SIM_GET(0), 1, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lb_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_signed_perf(insn, REG_GET(0) + SIM_GET(0), 1, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lh_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load_signed(insn, REG_GET(0) + SIM_GET(0), 2, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lh_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_signed_perf(insn, REG_GET(0) + SIM_GET(0), 2, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load_signed(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_signed_perf(insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lbu_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load(insn, REG_GET(0) + SIM_GET(0), 1, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lbu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_perf(insn, REG_GET(0) + SIM_GET(0), 1, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lhu_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.load(insn, REG_GET(0) + SIM_GET(0), 2, REG_OUT(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t lhu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_IN(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.load_perf(insn, REG_GET(0) + SIM_GET(0), 2, REG_OUT(0)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sb_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.store(insn, REG_GET(0) + SIM_GET(0), 1, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sb_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_perf(insn, REG_GET(0) + SIM_GET(0), 1, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sh_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.store(insn, REG_GET(0) + SIM_GET(0), 2, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sh_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_perf(insn, REG_GET(0) + SIM_GET(0), 2, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.store(insn, REG_GET(0) + SIM_GET(0), 4, REG_IN(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.stack_access_check(REG_OUT(0), REG_GET(0) + SIM_GET(0));
    if (iss->lsu.store_perf(insn, REG_GET(0) + SIM_GET(0), 4, REG_IN(1)))
    {
        return pc;
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t addi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t in = REG_GET(0);
    iss_reg_t imm = SIM_GET(0);
    REG_SET(0, LIB_CALL2(lib_ADD, in, imm));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t nop_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slti_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (iss_sim_t)REG_GET(0) < insn->sim[0]);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sltiu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, REG_GET(0) < (iss_uim_t)SIM_GET(0));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t xori_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_XOR, REG_GET(0), SIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t ori_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_OR, REG_GET(0), SIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t andi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_AND, REG_GET(0), SIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SLL, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRL, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srai_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRA, REG_GET(0), UIM_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t add_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_ADD, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sub_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SUB, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sll_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SLL, REG_GET(0), REG_GET(1) & ((1 << ISS_REG_WIDTH_LOG2) - 1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t slt_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, (iss_sim_t)REG_GET(0) < (iss_sim_t)REG_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sltu_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, REG_GET(0) < REG_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t xor_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_XOR, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t srl_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRL, REG_GET(0), REG_GET(1) & ((1 << ISS_REG_WIDTH_LOG2) - 1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sra_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_SRA, REG_GET(0), REG_GET(1) & ((1 << ISS_REG_WIDTH_LOG2) - 1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t or_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_OR, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t and_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL2(lib_AND, REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t fence_i_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We have to get the next pc now as we can't access insn aymore after the cache flush
    iss_reg_t next_pc = iss_insn_next(iss, insn, pc);

    iss->exec.icache_flush();

    return next_pc;
}

static inline iss_reg_t fence_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t ebreak_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // TODO prev instruction could not have been decoded if there was a cache flush
    // just before this instruction.
    // The good solution would be to issue load requests and be able to stall the instruction
    // until we got the previous opcode
    iss_insn_t *prev = insn_cache_get_insn(iss, pc - 4);
    if (prev == NULL)
    {
        return pc;
    }

    // The opcode on rv64 is on 64bits. It should be on 32bits but there is a bug
    // somwhere in this case, to be investigated.
    // Until it is fixed, we just cast it for now.
    if (((uint32_t)prev->opcode) == 0x01f01013)
    {
        iss->syscalls.handle_riscv_ebreak();
        return iss_insn_next(iss, insn, pc);
    }

    if (iss->exec.debug_mode)
    {
        return iss->irq.debug_handler;
    }
    else
    {
        iss->exception.raise(pc, ISS_EXCEPT_BREAKPOINT);
        return pc;
    }
}

static inline iss_reg_t ecall_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    /*
      printf("SYSCall %d, Fun(n:%d, A0:%d, A1:%d, A2:%d, A3:%d)\n",
       pc->uim[0], getReg(cpu, 17), getReg(cpu, 10), getReg(cpu, 11), getReg(cpu, 12), getReg(cpu, 13));
    */

    iss->csr.stval.value = 0;
    iss->exception.raise(pc, ISS_EXCEPT_ENV_CALL_U_MODE + iss->core.mode_get());
#if 0
  if (!cpu->conf->useSyscalls) {
    triggerException(cpu, pc, EXCEPTION_ECALL);
    return pc->next;
  } else {
    rv32_SYS_calls(cpu, pc);
    return pc->next;
  }
#endif

    return iss_insn_next(iss, insn, pc);
}

#endif
