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

#include "exec_inorder.hpp"

inline iss_reg_t ExecInOrder::insn_exec(iss_insn_t *insn, iss_reg_t pc)
{
    return insn->handler(&this->iss, insn, pc);
}

inline iss_reg_t ExecInOrder::insn_exec_fast(iss_insn_t *insn, iss_reg_t pc)
{
    return insn->fast_handler(&this->iss, insn, pc);
}

inline void ExecInOrder::switch_to_full_mode()
{
    this->instr_event.set_callback(&ExecInOrder::exec_instr_check_all);
}

inline void ExecInOrder::interrupt_taken()
{
    this->insn_table_index = 0;
}

inline InsnEntry *ExecInOrder::get_entry()
{
    if (this->first_entry)
    {
        InsnEntry *entry = this->first_entry;
        this->first_entry = entry->next;
        return entry;
    }

    return new InsnEntry();
}

inline void ExecInOrder::release_entry(InsnEntry *entry)
{
    entry->next = this->first_entry;
    this->first_entry = entry;
}

inline void ExecInOrder::enqueue_task(Task *task)
{
    task->next = this->first_task;
    this->first_task = task;
}

inline void ExecInOrder::insn_exec_profiling()
{
    this->trace.msg("Executing instruction (addr: 0x%x)\n", this->iss.exec.current_insn);

    this->irq_enter.set(0);
    this->irq_exit.set(0);

    if (this->iss.timing.pc_trace_event.active_get())
    {
        this->iss.timing.pc_trace_event.dump((uint8_t *)&this->iss.exec.current_insn);
    }
    if (this->iss.timing.active_pc_trace_event.active_get())
    {
        this->iss.timing.active_pc_trace_event.dump((uint8_t *)&this->iss.exec.current_insn);
    }
    if (this->iss.trace.func_trace_event.get_event_active() ||
        this->iss.trace.inline_trace_event.get_event_active() ||
        this->iss.trace.file_trace_event.get_event_active() ||
        this->iss.trace.line_trace_event.get_event_active())
    {
        this->iss.trace.dump_debug_traces();
    }
}

inline void ExecInOrder::insn_exec_power(iss_insn_t *insn)
{
    if (this->iss.power.is_enabled())
    {
        this->iss.timing.insn_groups_power[insn->decoder_item->u.insn.power_group].account_energy_quantum();
    }
}

inline bool ExecInOrder::handle_tasks()
{
    Task *task = this->first_task;
    if (task)
    {
        this->first_task = NULL;

        do
        {
            task->callback(&this->iss, task);
            task = task->next;
        } while (task);

        // In case the core is retained, it means it was only executing tasks. Try to stop it
        // and tell the caller to not continue
        if (this->retained.get())
        {
            this->retain_check();
            return true;
        }
    }
    return false;
}

inline bool ExecInOrder::can_switch_to_fast_mode()
{
    if (this->iss.gdbserver.is_enabled())
    {
        return false;
    }

#ifdef VP_TRACE_ACTIVE
    return false;
#else
#if defined(ISS_HAS_PERF_COUNTERS)
    return !(this->iss.csr.pcmr & CSR_PCMR_ACTIVE);
#else
    return true;
#endif
#endif
}
