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

#include "cpu/iss/include/types.hpp"

inline void Timing::stall_cycles_account(int cycles)
{
    this->iss.exec.instr_event->stall_cycle_inc(cycles);
    if (cycles > 0)
    {
        this->power_stall_first.account_energy_quantum();
        for (int i=0; i<cycles-1; i++)
        {
            this->power_stall_next.account_energy_quantum();
        }
    }
}

inline void Timing::event_trace_account(unsigned int event, int cycles)
{
    static uint64_t zero = 0;
    static uint64_t one = 1;

    if (this->pcer_trace_event[event].get_event_active())
    {
        // TODO this is incompatible with frequency scaling, this should be replaced by an event scheduled with cycles
        this->pcer_trace_event[event].event_pulse(cycles * this->iss.top.clock.get_period(), (uint8_t *)&one, (uint8_t *)&zero);
    }
}

inline void Timing::event_trace_set(unsigned int event)
{
    static uint64_t one = 1;

    if (this->pcer_trace_event[event].get_event_active())
    {
        this->pcer_trace_event[event].event((uint8_t *)&one);
    }
}

inline void Timing::event_trace_reset(unsigned int event)
{
    static uint64_t zero = 0;

    if (this->pcer_trace_event[event].get_event_active())
    {
        this->pcer_trace_event[event].event((uint8_t *)&zero);
    }
}

inline int Timing::event_trace_is_active(unsigned int event)
{
    return this->pcer_trace_event[event].get_event_active() && this->ext_counter[event].is_bound();
}

inline void Timing::event_account(unsigned int event, int incr)
{
    Iss *iss = &this->iss;

    if (iss->csr.pcmr & CSR_PCMR_ACTIVE && (iss->csr.pcer & (1 << event)))
    {
        iss->csr.pccr[event] += incr;
    }

    this->event_trace_account(event, incr);
}

inline void Timing::event_load_account(int incr)
{
    this->event_account(CSR_PCER_LD, incr);
}

inline void Timing::event_rvc_account(int incr)
{
    this->event_account(CSR_PCER_RVC, incr);
}

inline void Timing::event_store_account(int incr)
{
    this->event_account(CSR_PCER_ST, incr);
}

inline void Timing::event_branch_account(int incr)
{
    this->event_account(CSR_PCER_BRANCH, incr);
}

inline void Timing::event_taken_branch_account(int incr)
{
    this->event_account(CSR_PCER_TAKEN_BRANCH, incr);
}

inline void Timing::event_jump_account(int incr)
{
    this->event_account(CSR_PCER_JUMP, incr);
}

inline void Timing::event_misaligned_account(int incr)
{
    this->event_account(CSR_PCER_MISALIGNED, incr);
}

inline void Timing::event_insn_contention_account(int incr)
{
    this->event_account(CSR_PCER_INSN_CONT, incr);
}

inline void Timing::insn_stall_start()
{
    this->event_trace_set(CSR_PCER_CYCLES);
}

inline void Timing::insn_stall_stop()
{
    this->event_trace_reset(CSR_PCER_CYCLES);
}

inline void Timing::cycle_account()
{
    this->event_account(CSR_PCER_CYCLES, 1);
}

inline void Timing::insn_stall_account()
{
    int64_t stall_cycles = this->iss.exec.instr_event->stall_cycle_get();
    if (stall_cycles >= 0)
    {
        this->event_account(CSR_PCER_CYCLES, stall_cycles);
    }
}

inline void Timing::insn_account()
{
    this->event_account(CSR_PCER_INSTR, 1);
    int64_t stall_cycles = this->iss.exec.instr_event->stall_cycle_get();
    int64_t cycles = 1;
    if (stall_cycles >= 0)
    {
        cycles += stall_cycles;
    }
    this->event_account(CSR_PCER_CYCLES, cycles);

#if defined(ISS_HAS_PERF_COUNTERS)
    for (int i = CSR_PCER_NB_INTERNAL_EVENTS; i < CSR_PCER_NB_EVENTS; i++)
    {
        if (this->event_trace_is_active(i))
        {
            update_external_pccr(&this->iss, i, this->iss.csr.pcer, this->iss.csr.pcmr);
        }
    }
#endif
}

inline void Timing::stall_fetch_account(int cycles)
{
    this->stall_cycles_account(cycles);
    this->event_account(CSR_PCER_IMISS, cycles);
}

inline void Timing::stall_misaligned_account()
{
    this->stall_cycles_account(1);
    this->event_account(CSR_PCER_LD, 1);
}

inline void Timing::stall_load_account(int cycles)
{
    this->stall_cycles_account(cycles);
}

inline void Timing::stall_acc_account(int cycles)
{
    this->stall_cycles_account(cycles);
}

inline void Timing::stall_taken_branch_account()
{
    this->stall_cycles_account(2);
    this->event_branch_account(1);
    this->event_taken_branch_account(1);
}

inline void Timing::stall_insn_account(int cycles)
{
    this->stall_cycles_account(cycles);
}

inline void Timing::stall_insn_dependency_account(int latency)
{
    this->stall_cycles_account(latency - 1);
}

inline void Timing::stall_load_dependency_account(int latency)
{
    this->stall_cycles_account(latency - 1);
    this->event_account(CSR_PCER_LD_STALL, latency - 1);
}

inline void Timing::stall_jump_account()
{
    this->stall_cycles_account(1);
    this->event_jump_account(1);
}
