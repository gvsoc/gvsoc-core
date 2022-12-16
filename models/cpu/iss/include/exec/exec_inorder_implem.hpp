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
    this->iss.stall_insn = this->iss.current_insn;
    this->stalled_inc();
}



inline void Exec::insn_hold()
{
    this->iss.dump_trace_enabled = false;
    this->iss.stall_insn = this->iss.current_insn;
}



inline void Exec::insn_terminate()
{
    if (this->iss.insn_trace.get_active())
    {
        iss_trace_dump(&this->iss, this->iss.stall_insn);
    }
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
    this->iss.trace.msg("Executing instruction\n");
    if (this->iss.pc_trace_event.get_event_active())
    {
        this->iss.pc_trace_event.event((uint8_t *)&this->iss.current_insn->addr);
    }
    if (this->iss.active_pc_trace_event.get_event_active())
    {
        this->iss.active_pc_trace_event.event((uint8_t *)&this->iss.current_insn->addr);
    }
    if (this->iss.func_trace_event.get_event_active() || this->iss.inline_trace_event.get_event_active() || this->iss.file_trace_event.get_event_active() || this->iss.line_trace_event.get_event_active())
    {
        this->iss.dump_debug_traces();
    }
    if (this->iss.ipc_stat_event.get_event_active())
    {
        this->iss.ipc_stat_nb_insn++;
    }
}



inline void Exec::insn_exec_power(iss_insn_t *insn)
{
    if (this->iss.power.get_power_trace()->get_active())
    {
        this->iss.insn_groups_power[insn->decoder_item->u.insn.power_group].account_energy_quantum();
    }
}



inline void Exec::switch_to_full_mode()
{
    this->instr_event->meth_set(this, &Exec::exec_instr_check_all);
}