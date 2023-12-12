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

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"



static inline iss_reg_t cm_insn_handle(Iss *iss, iss_insn_t *insn, iss_reg_t pc, bool is_push, bool ret, bool retz)
{
    static int reg_list[] = {1, 8, 9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

    // The number of registers to be saved is given by rlist field (imm 0).
    // 4 for ra, 5 for ra,s0, 15 for ra, s0-s10 and 0 for s0-s11
    int nb_regs = (UIM_GET(0) - 3) & 0xf;
    int nb_insns = nb_regs + 1 + ret + retz;

    iss_insn_t *table = insn->expand_table;
    if (table == NULL)
    {
        // The table of micro-instructions can be empty if it is the first time we execute this instruction or
        // if there was a cache flush.
        // In both cases, we need to fill an array of instruction opcodes and decode it.
        table = new iss_insn_t[nb_insns];
        insn->expand_table = table;

        for (int i=0; i<nb_insns; i++)
        {
            insn_init(&table[i], 0);
        }

        // The maximum immediate is a multiple of 4 with 4 bytes per register, and adjusted with second
        // instruction immediate
        iss_reg_t imm = (nb_regs + 3) / 4 * 4 * 4 + UIM_GET(1)*16;
        if (is_push)
        {
            imm = -imm;

            // For push, stack is pointing to first word above the stack frame,
            // so we need to start from -4 and decrease
            for (int i=0, offset=-4; i<nb_regs; i++, offset-=4)
            {
                // Insert one sw per register
                int reg_id = reg_list[nb_regs - 1 - i];
                table[i].opcode = 0x23 | (0x2 << 12) | (reg_id << 20) | (2 << 15) | (getField(offset, 0, 5) << 7) | (getField(offset, 5, 7) << 25);
            }

            // Insert stack pointer update
            table[nb_regs].opcode = 0x13 | (0x0 << 12) | (2 << 7) | (2 << 15) | (imm << 20);
        }
        else
        {
            int index = 0;

            // For pop, stack is pointing to first word of the stack frame,
            // so we need to start from 0 and increase
            for (int i=0, offset=imm-4; i<nb_regs; i++, offset-=4)
            {
                // Insert one lw per register
                int reg_id = reg_list[nb_regs - 1 - i];
                table[index++].opcode = 0x03 | (0x2 << 12) | (reg_id << 7) | (2 << 15) | (getField(offset, 0, 12) << 20);
            }


            // Insert stack pointer update
            table[index++].opcode = 0x13 | (0x0 << 12) | (2 << 7) | (2 << 15) | (imm << 20);

            if (retz)
            {
                // Mov zero to a0
                table[index++].opcode = 0x00000513;
            }

            if (ret)
            {
                // jr ra
                table[index++].opcode = 0x00008067;
            }
        }

        for (int i=0; i<nb_insns; i++)
        {
            iss->decode.decode_pc(&table[i], insn->addr);
        }

        // Instruction table must be pushed to decoder so that it is freed when cache is flushed
        iss->decode.insn_tables.push_back(table);
    }

    // Lock the IRQs if we enter the atomic section
    if (iss->exec.insn_table_index == nb_regs)
    {
        iss->exec.irq_locked = true;
    }

    // Now execute the current micro-instruction
    iss_insn_t *current = &table[iss->exec.insn_table_index++];
    iss_reg_t next = current->handler(iss, current, pc);

    // We return same pc until the macro-instruction is over
    if (iss->exec.insn_table_index == nb_insns)
    {
        iss->exec.irq_locked = false;

        // Once it is over, we return either the instruction next to the macro one, or
        // the one reported by the ret micro-instruction in case we execute a popret or popretz
        iss->exec.insn_table_index = 0;
        if (ret)
        {
            return next;
        }
        else
        {
            return pc + insn->size;
        }
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
