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

#ifndef __CPU_ISS_RV32C_HPP
#define __CPU_ISS_RV32C_HPP

#ifdef CONFIG_GVSOC_ISS_V2
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss_v2/include/isa_lib/macros.h"
#else
#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"
#endif

static inline iss_reg_t c_unimp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
    return pc;
}

/* Reserved RVC code-points: the ISA table wildcards the immediate/register
 * fields, so these encodings decode into the regular handlers, but the RVC
 * spec reserves them and the hardware raises illegal instruction (e.g.
 * cv32e40p_compressed_decoder.sv). Mirrors iss_exec_insn_illegal.
 * The checks below are gated by CONFIG_GVSOC_ISS_RVC_STRICT (opt-in per
 * core, e.g. cv32e40p_v2.py): the historical permissive decoding - and
 * the golden traces built on it - stays the default for every other
 * core. */
static inline iss_reg_t c_reserved_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->decode.trace.msg("Executing illegal instruction\n");
    iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
#ifdef CONFIG_GVSOC_ISS_V2
    iss->exec.insn_stall();
#endif
    return pc;
}

static inline iss_reg_t c_addi4spn_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x1FE0) == 0)  /* nzuimm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_addi4spn_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x1FE0) == 0)  /* nzuimm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    iss->timing.event_rvc_account(1);
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_lw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return lw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_lw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return lw_exec(iss, insn, pc);
}

static inline iss_reg_t c_sw_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return sw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_sw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return sw_exec(iss, insn, pc);
}

static inline iss_reg_t c_swsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return sw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_swsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return sw_exec(iss, insn, pc);
}

static inline iss_reg_t c_nop_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return nop_exec(iss, insn, pc);
}

static inline iss_reg_t c_nop_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return nop_exec(iss, insn, pc);
}

static inline iss_reg_t c_addi_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_addi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_jal_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jal_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_jal_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return jal_exec(iss, insn, pc);
}

static inline iss_reg_t c_li_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_li_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_addi16sp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x107C) == 0)  /* nzimm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_addi16sp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x107C) == 0)  /* nzimm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    iss->timing.event_rvc_account(1);
    return addi_exec(iss, insn, pc);
}

static inline iss_reg_t c_jalr_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jalr_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_jalr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return jalr_exec(iss, insn, pc);
}

static inline iss_reg_t c_lui_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x107C) == 0)  /* imm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    return lui_exec(iss, insn, pc);
}

static inline iss_reg_t c_lui_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x107C) == 0)  /* imm == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    iss->timing.event_rvc_account(1);
    return lui_exec(iss, insn, pc);
}

static inline iss_reg_t c_srli_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return srli_exec(iss, insn, pc);
}

static inline iss_reg_t c_srli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return srli_exec(iss, insn, pc);
}

static inline iss_reg_t c_srai_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return srai_exec(iss, insn, pc);
}

static inline iss_reg_t c_srai_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return srai_exec(iss, insn, pc);
}

static inline iss_reg_t c_andi_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return andi_exec(iss, insn, pc);
}

static inline iss_reg_t c_andi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return andi_exec(iss, insn, pc);
}

static inline iss_reg_t c_sub_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return sub_exec(iss, insn, pc);
}

static inline iss_reg_t c_sub_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return sub_exec(iss, insn, pc);
}

static inline iss_reg_t c_xor_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return xor_exec(iss, insn, pc);
}

static inline iss_reg_t c_xor_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return xor_exec(iss, insn, pc);
}

static inline iss_reg_t c_or_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return or_exec(iss, insn, pc);
}

static inline iss_reg_t c_or_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return or_exec(iss, insn, pc);
}

static inline iss_reg_t c_and_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return and_exec(iss, insn, pc);
}

static inline iss_reg_t c_and_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return and_exec(iss, insn, pc);
}

static inline iss_reg_t c_j_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return jal_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_j_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return jal_exec(iss, insn, pc);
}

static inline iss_reg_t c_beqz_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return beq_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_beqz_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return beq_exec(iss, insn, pc);
}

static inline iss_reg_t c_bnez_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return bne_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_bnez_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return bne_exec(iss, insn, pc);
}

static inline iss_reg_t c_slli_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return slli_exec(iss, insn, pc);
}

static inline iss_reg_t c_slli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return slli_exec(iss, insn, pc);
}

static inline iss_reg_t c_lwsp_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x0F80) == 0)  /* rd == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    return lw_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_lwsp_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x0F80) == 0)  /* rd == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    iss->timing.event_rvc_account(1);
    return lw_exec(iss, insn, pc);
}

static inline iss_reg_t c_jr_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x0F80) == 0)  /* rs1 == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    return jalr_exec_fast(iss, insn, pc);
}

static inline iss_reg_t c_jr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    if ((insn->opcode & 0x0F80) == 0)  /* rs1 == 0: reserved */
        return c_reserved_exec(iss, insn, pc);
#endif
    iss->timing.event_rvc_account(1);
    return jalr_exec(iss, insn, pc);
}

static inline iss_reg_t c_mv_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return add_exec(iss, insn, pc);
}

static inline iss_reg_t c_mv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return add_exec(iss, insn, pc);
}

static inline iss_reg_t c_add_exec_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return add_exec(iss, insn, pc);
}

static inline iss_reg_t c_add_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);
    return add_exec(iss, insn, pc);
}

static inline iss_reg_t c_ebreak_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.event_rvc_account(1);

    // Same semantics as the 32-bit ebreak (rv32i.hpp ebreak_exec), minus the
    // semihosting probe which is specified on the uncompressed form only.
    if (iss->exec.debug_mode)
    {
        // Back to the park loop: an ebreak in debug mode re-enters the
        // debug ROM instead of raising an exception.
        return iss->irq.debug_handler;
    }
#ifdef CONFIG_GVSOC_ISS_CV32E40P
    // dcsr.ebreakm=1 in M-mode: enter debug (RISC-V Debug Spec).
    if (iss->csr.ebreak_m_mode_enters_debug())
    {
        iss->dbgunit.set_halt_mode(true, HALT_CAUSE_EBREAK);
        return pc;
    }
#endif
#ifdef CONFIG_GVSOC_ISS_CV32E40P_V2
    // dcsr.ebreakm=1 in M-mode: enter debug (RISC-V Debug Spec).
    // Arms the request (cause=1); check() performs the entry at the next
    // dispatch boundary - see rv32i.hpp ebreak_exec.
    if (iss->csr.ebreak_m_mode_enters_debug())
    {
        iss->irq.ebreak_enter_debug();
        return pc;
    }
#endif
    iss->exception.raise(pc, ISS_EXCEPT_BREAKPOINT);
    return pc;
}

static inline iss_reg_t c_sbreak_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_RVC_STRICT
    /* 0x8002 is c.jr with rs1=0: an RVC-reserved code-point. The legacy
     * PULP c.sbreak (RI5CY era) repurposed this encoding as a nop-like
     * debug break, so the ISA table decodes it here instead of c_jr_exec
     * (whose rs1==0 strict check can therefore never fire). Cores that
     * opt into strict RVC decoding (e.g. CV32E40P, whose compressed
     * decoder raises illegal instruction on it) must trap HERE, with
     * mepc = the faulting pc: falling through to the nop would retire a
     * phantom instruction and take the illegal one insn later (off-by-2
     * mepc against the RTL). */
    return c_reserved_exec(iss, insn, pc);
#else
    iss->timing.event_rvc_account(1);
    // iss->dbgunit.set_halt_mode(true, 3);
    return iss_insn_next(iss, insn, pc);
#endif
}

#endif
