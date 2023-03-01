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

#include <types.hpp>
#include ISS_CORE_INC(class.hpp)

static inline iss_insn_t *iss_exec_stalled_insn_fast(Iss *iss, iss_insn_t *insn)
{
    iss->timing.stall_load_dependency_account(insn->latency);
    return insn->stall_fast_handler(iss, insn);
}

static inline iss_insn_t *iss_exec_stalled_insn(Iss *iss, iss_insn_t *insn)
{
    iss->timing.stall_load_dependency_account(insn->latency);
    return insn->stall_handler(iss, insn);
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

inline void Exec::insn_hold()
{
    this->iss.trace.dump_trace_enabled = false;
    this->stall_insn = this->current_insn;
}

inline void Exec::insn_terminate()
{
    if (this->iss.trace.insn_trace.get_active())
    {
        iss_trace_dump(&this->iss, this->stall_insn);
    }

    this->iss.timing.insn_stall_account();
}

inline iss_insn_t *Exec::insn_exec(iss_insn_t *insn)
{
    return insn->handler(&this->iss, insn);
}

inline iss_insn_t *Exec::insn_exec_fast(iss_insn_t *insn)
{
    return insn->fast_handler(&this->iss, insn);
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
    this->trace.msg("Executing instruction (addr: 0x%x)\n", this->iss.exec.current_insn->addr);
    if (this->iss.timing.pc_trace_event.get_event_active())
    {
        this->iss.timing.pc_trace_event.event((uint8_t *)&this->iss.exec.current_insn->addr);
    }
    if (this->iss.timing.active_pc_trace_event.get_event_active())
    {
        this->iss.timing.active_pc_trace_event.event((uint8_t *)&this->iss.exec.current_insn->addr);
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
    this->instr_event->meth_set(&this->iss, &Exec::exec_instr_check_all);
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
