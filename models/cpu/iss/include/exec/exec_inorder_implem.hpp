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

#include <cpu/iss/include/types.hpp>
#include ISS_CORE_INC(class.hpp)


inline void Exec::interrupt_taken()
{
    this->iss.exec.insn_table_index = 0;
}


static inline iss_reg_t iss_exec_stalled_insn_fast(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.stall_load_dependency_account(insn->latency);
    return insn->stall_fast_handler(iss, insn, pc);
}

static inline iss_reg_t iss_exec_stalled_insn(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->timing.stall_load_dependency_account(insn->latency);
    return insn->stall_handler(iss, insn, pc);
}

inline iss_insn_callback_t Exec::insn_trace_callback_get()
{
    return iss_exec_insn_with_trace;
}

inline iss_insn_callback_t Exec::insn_stalled_callback_get()
{
    return iss_exec_stalled_insn;
}

inline iss_insn_callback_t Exec::insn_stalled_fast_callback_get()
{
    return iss_exec_stalled_insn_fast;
}



inline void Exec::insn_stall()
{
    this->stall_insn = this->current_insn;
    this->iss.timing.insn_stall_start();
    this->stalled_inc();
}

inline void Exec::insn_hold(vp::ClockEventMeth *meth)
{
    this->iss.trace.dump_trace_enabled = false;
    this->stall_insn = this->current_insn;
    // Flag that we cannto execute instructions so that no one tries
    // to change the event callback
    this->insn_on_hold = true;
    this->iss.exec.instr_event->set_callback(meth);
}

inline void Exec::insn_resume()
{
    // Instruction execution can go on
    this->insn_on_hold = false;
    this->instr_event->set_callback(&Exec::exec_instr_check_all);
}

inline void Exec::insn_terminate()
{
    if (this->iss.trace.insn_trace.get_active())
    {
        // TODO this is not possible to correctly handle it for now, a better system should be implemented
        // for staling execution.
        // For now if the cache returns NULL due to mmu or prefetcher, we drop the instruction.
        iss_insn_t *insn = insn_cache_get_insn(&this->iss, this->stall_insn);
        if (insn != NULL)
        {
            iss_trace_dump(&this->iss, insn, this->stall_insn);
        }
    }

    this->iss.timing.insn_stall_account();
}

inline iss_reg_t Exec::insn_exec(iss_insn_t *insn, iss_reg_t pc)
{
    return insn->handler(&this->iss, insn, pc);
}

inline iss_reg_t Exec::insn_exec_fast(iss_insn_t *insn, iss_reg_t pc)
{
    return insn->fast_handler(&this->iss, insn, pc);
}

inline bool Exec::can_switch_to_fast_mode()
{
    if (this->iss.gdbserver.is_enabled())
    {
        return false;
    }

#ifdef VP_TRACE_ACTIVE
    return false;
#else
    return !(this->iss.csr.pcmr & CSR_PCMR_ACTIVE);
#endif
}

inline bool Exec::is_stalled()
{
    return this->stalled.get();
}

inline void Exec::stalled_inc()
{
    if (this->stalled.get() == 0)
    {
        this->loop_count = 0;
        this->instr_event->disable();
    }
    this->stalled.inc(1);
}

inline void Exec::stalled_dec()
{
    if (this->stalled.get() == 0)
    {
        this->trace.warning("Trying to decrease zero stalled counter\n");
        exit(1);
        return;
    }

    this->stalled.dec(1);

    if (this->stalled.get() == 0)
    {
        this->instr_event->enable();
    }
}

inline void Exec::insn_exec_profiling()
{
    this->trace.msg("Executing instruction (addr: 0x%x)\n", this->iss.exec.current_insn);
    if (this->iss.timing.pc_trace_event.get_event_active())
    {
        this->iss.timing.pc_trace_event.event((uint8_t *)&this->iss.exec.current_insn);
    }
    if (this->iss.timing.active_pc_trace_event.get_event_active())
    {
        this->iss.timing.active_pc_trace_event.event((uint8_t *)&this->iss.exec.current_insn);
    }
    if (this->iss.timing.func_trace_event.get_event_active() || this->iss.timing.inline_trace_event.get_event_active() || this->iss.timing.file_trace_event.get_event_active() || this->iss.timing.line_trace_event.get_event_active())
    {
        this->iss.trace.dump_debug_traces();
    }
}

inline void Exec::insn_exec_power(iss_insn_t *insn)
{
    if (this->iss.top.power.get_power_trace()->get_active())
    {
        this->iss.timing.insn_groups_power[insn->decoder_item->u.insn.power_group].account_energy_quantum();
    }
}

inline void Exec::switch_to_full_mode()
{
    // Only switch to full mode instruction if we are not currently executing instructions,
    // do not overwrite the event callback used for another activity
    if (!this->insn_on_hold)
    {
        this->instr_event->set_callback(&Exec::exec_instr_check_all);
    }
}


inline bool Exec::clock_active_get()
{
    return this->clock_active;
}

inline void Exec::busy_enter()
{
    uint8_t one = 1;
    this->iss.timing.state_event.event(&one);
    this->iss.exec.busy.set(1);
    if (this->iss.exec.busy_itf.is_bound())
    {
        this->iss.exec.busy_itf.sync(1);
    }
}

inline void Exec::busy_exit()
{
    this->iss.timing.state_event.event(NULL);
    this->iss.exec.busy.release();
    if (this->iss.exec.busy_itf.is_bound())
    {
        this->iss.exec.busy_itf.sync(0);
    }
    if (this->iss.timing.active_pc_trace_event.get_event_active())
    {
        this->iss.timing.active_pc_trace_event.event(NULL);
    }
}
