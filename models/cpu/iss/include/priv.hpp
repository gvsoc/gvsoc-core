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

#include <iss_core.hpp>

static inline void csr_decode(Iss *iss, iss_insn_t *insn)
{
    // In case traces are active, convert the CSR number into a name
#ifdef VP_TRACE_ACTIVE
    insn->args[2].flags = (iss_decoder_arg_flag_e)(insn->args[2].flags | ISS_DECODER_ARG_FLAG_DUMP_NAME);
    insn->args[2].name = iss_csr_name(iss, UIM_GET(0));
#endif
}

static inline iss_insn_t *csrrw_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    if (insn->out_regs[0] != 0)
    {
        if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
        {
            REG_SET(0, value);
        }
    }

    if (REG_IN(0) != 0)
    {
        iss_csr_write(iss, UIM_GET(0), reg_value);
    }

    return insn->next;
}

static inline iss_insn_t *csrrc_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
            REG_SET(0, value);
    }

    iss_csr_write(iss, UIM_GET(0), value & ~reg_value);

    return insn->next;
}

static inline iss_insn_t *csrrs_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;
    iss_reg_t reg_value = REG_GET(0);

    if (insn->out_regs[0] != 0)
    {
        if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
        {
            if (insn->out_regs[0] != 0)
                REG_SET(0, value);
        }
    }
    if (REG_IN(0) != 0)
    {
        iss_csr_write(iss, UIM_GET(0), value | reg_value);
    }
    return insn->next;
}

static inline iss_insn_t *csrrwi_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
            REG_SET(0, value);
    }
    iss_csr_write(iss, UIM_GET(0), UIM_GET(1));
    return insn->next;
}

static inline iss_insn_t *csrrci_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
            REG_SET(0, value);
    }
    iss_csr_write(iss, UIM_GET(0), value & ~UIM_GET(1));
    return insn->next;
}

static inline iss_insn_t *csrrsi_exec(Iss *iss, iss_insn_t *insn)
{
    iss_reg_t value;

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
            REG_SET(0, value);
    }
    iss_csr_write(iss, UIM_GET(0), value | UIM_GET(1));
    return insn->next;
}

static inline iss_insn_t *wfi_exec(Iss *iss, iss_insn_t *insn)
{
    iss->irq.wfi_handle();
    return insn->next;
}

static inline iss_insn_t *mret_exec(Iss *iss, iss_insn_t *insn)
{
    iss->timing.stall_insn_dependency_account(5);
    return iss->core.mret_handle();
}

static inline iss_insn_t *dret_exec(Iss *iss, iss_insn_t *insn)
{
    return iss->core.dret_handle();
}

static inline iss_insn_t *sret_exec(Iss *iss, iss_insn_t *insn)
{
    return insn->next;
}
