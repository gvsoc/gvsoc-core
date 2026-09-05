/*
 * Copyright (C) 2026 GreenWaves Technologies, SAS, ETH Zurich and
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

// Zcmp (code-size reduction) push/pop macro-instructions, ported from the
// iss_v1 implementation in cpu/iss/include/isa/zcmp.hpp.
//
// Each one expands into a sequence of ordinary instructions -- one sw or lw per
// saved register, a stack-pointer adjust, and for the popret forms a zeroing of
// a0 and a return. The expansion is built once, cached on the instruction, and
// stepped through one micro-instruction per execution, the macro-instruction's
// own pc being returned until the last of them.

#pragma once

#include "cpu/iss_v2/include/iss.hpp"
#include "cpu/iss_v2/include/isa_lib/macros.h"

static inline iss_uim_t zcmp_get_field(iss_uim_t val, int shift, int bits)
{
    return (val >> shift) & ((1ULL << bits) - 1);
}

static inline iss_reg_t cm_insn_handle(Iss *iss, iss_insn_t *insn, iss_reg_t pc,
    bool is_push, bool ret, bool retz)
{
    static int reg_list[] = {1, 8, 9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

    // The number of registers to be saved is given by the rlist field (imm 0):
    // 4 for ra, 5 for ra + s0, 15 for ra + s0-s10, and 0 for s0-s11.
    int nb_regs = (UIM_GET(0) - 3) & 0xf;
    int nb_insns = nb_regs + 1 + ret + retz;

    iss_insn_t *table = insn->expand_table;
    if (table == NULL)
    {
        // The table of micro-instructions is empty the first time this
        // instruction executes, and again after a cache flush. Build the
        // opcodes and decode them.
        table = new iss_insn_t[nb_insns];
        insn->expand_table = table;

        for (int i = 0; i < nb_insns; i++)
        {
            iss->insn_cache.insn_init(&table[i], 0);
        }

        // The stack adjustment is a multiple of 16 bytes, rounded up from the
        // 4 bytes each register takes, plus the extra given by the spimm field.
        iss_reg_t imm = (nb_regs + 3) / 4 * 4 * 4 + UIM_GET(1) * 16;
        if (is_push)
        {
            imm = -imm;

            // On push the stack still points above the frame, so the stores
            // start one word below it and walk down.
            for (int i = 0, offset = -4; i < nb_regs; i++, offset -= 4)
            {
                int reg_id = reg_list[nb_regs - 1 - i];
                table[i].opcode = 0x23 | (0x2 << 12) | (reg_id << 20) | (2 << 15)
                    | (zcmp_get_field(offset, 0, 5) << 7)
                    | (zcmp_get_field(offset, 5, 7) << 25);
            }

            table[nb_regs].opcode = 0x13 | (0x0 << 12) | (2 << 7) | (2 << 15) | (imm << 20);
        }
        else
        {
            int index = 0;

            // On pop the stack points at the first word of the frame, so the
            // loads start at the top of it and walk down.
            for (int i = 0, offset = imm - 4; i < nb_regs; i++, offset -= 4)
            {
                int reg_id = reg_list[nb_regs - 1 - i];
                table[index++].opcode = 0x03 | (0x2 << 12) | (reg_id << 7) | (2 << 15)
                    | (zcmp_get_field(offset, 0, 12) << 20);
            }

            table[index++].opcode = 0x13 | (0x0 << 12) | (2 << 7) | (2 << 15) | (imm << 20);

            if (retz)
            {
                // li a0, 0
                table[index++].opcode = 0x00000513;
            }

            if (ret)
            {
                // jr ra
                table[index++].opcode = 0x00008067;
            }
        }

        for (int i = 0; i < nb_insns; i++)
        {
            iss->decode.decode_pc(&table[i], insn->addr);
        }

        // Hand the table to the cache so it is freed when the cache is flushed.
        iss->insn_cache.register_insn_table(table);
    }

    // The stack pointer update and everything after it must not be interrupted.
    if (iss->exec.insn_table_index == nb_regs)
    {
        iss->exec.irq_locked++;
    }

    iss_insn_t *current = &table[iss->exec.insn_table_index++];
    iss_reg_t next = current->handler(iss, current, pc);

    // Report the macro-instruction's own pc until its last micro-instruction.
    if (iss->exec.insn_table_index == nb_insns)
    {
        iss->exec.irq_locked--;
        iss->exec.insn_table_index = 0;

        // Then continue after the macro-instruction, or wherever the return
        // micro-instruction went for the popret forms.
        if (ret)
        {
            return next;
        }
        return pc + insn->size;
    }

    return pc;
}

static inline iss_reg_t cm_push_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return cm_insn_handle(iss, insn, pc, true, false, false);
}

static inline iss_reg_t cm_pop_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return cm_insn_handle(iss, insn, pc, false, false, false);
}

static inline iss_reg_t cm_popretz_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return cm_insn_handle(iss, insn, pc, false, true, true);
}

static inline iss_reg_t cm_popret_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return cm_insn_handle(iss, insn, pc, false, true, false);
}
