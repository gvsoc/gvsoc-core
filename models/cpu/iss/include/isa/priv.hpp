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

#pragma once

#include <cpu/iss/include/iss_core.hpp>

static inline void csr_decode(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // In case traces are active, convert the CSR number into a name
#ifdef VP_TRACE_ACTIVE
    insn->args[2].flags = (iss_decoder_arg_flag_e)(insn->args[2].flags | ISS_DECODER_ARG_FLAG_DUMP_NAME);
    insn->args[2].name = iss_csr_name(iss, UIM_GET(0));
#endif
}

static inline iss_reg_t csrrw_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }

    iss_csr_write(iss, UIM_GET(0), reg_value);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t csrrc_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }

    iss_csr_write(iss, UIM_GET(0), value & ~reg_value);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t csrrs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    #ifdef CONFIG_GVSOC_ISS_SNITCH
    // Todo: put csr mcycle performance couter value assignment from here to csr.cpp
    if (UIM_GET(0) == 0xB00)
    {
        iss->csr.mcycle.value = iss->top.clock.get_cycles();
    }
    #endif

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, REG_IN(0) != 0, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }
    if (REG_IN(0) != 0)
    {
        iss_csr_write(iss, UIM_GET(0), value | reg_value);
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t csrrwi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }
    iss_csr_write(iss, UIM_GET(0), UIM_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t csrrci_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }
    iss_csr_write(iss, UIM_GET(0), value & ~UIM_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t csrrsi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t value;

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }
    iss_csr_write(iss, UIM_GET(0), value | UIM_GET(1));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t wfi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->irq.wfi_handle();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t mret_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.stall_insn_dependency_account(5);
    return iss->core.mret_handle();
}

static inline iss_reg_t dret_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss->core.dret_handle();
}

static inline iss_reg_t sret_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.stall_insn_dependency_account(5);
    return iss->core.sret_handle();
}

static inline iss_reg_t sfence_vma_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->core.mode_get() == PRIV_S && iss->csr.mstatus.tvm)
    {
        iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
        return pc;
    }
    else
    {
        iss->mmu.flush(REG_GET(0), REG_GET(1));
        iss->insn_cache.mode_flush();
        return iss_insn_next(iss, insn, pc);
    }
}
