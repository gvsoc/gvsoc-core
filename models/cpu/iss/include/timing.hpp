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

#include "types.hpp"


#if 0
static inline int iss_exec_account_cycles(Iss *iss, int cycles)
{
  if (iss->csr.pcmr & CSR_PCMR_ACTIVE)
  {
    if (cycles >= 0 && (iss->csr.pcer & (1<<CSR_PCER_CYCLES)))
    {
      iss->csr.pccr[CSR_PCER_CYCLES] += cycles;
    }
  }

  iss->perf_event_incr(CSR_PCER_CYCLES, cycles);

#if defined(ISS_HAS_PERF_COUNTERS)
  for (int i=CSR_PCER_NB_INTERNAL_EVENTS; i<CSR_PCER_NB_EVENTS; i++)
  {
    if (iss->perf_event_is_active(i))
    {
      update_external_pccr(iss, i, iss->csr.pcer, iss->csr.pcmr);
    }
  }
#endif
  return 0;
}
#endif


inline void Timing::reset(bool active)
{
    if (active)
    {
        this->stall_cycles = false;
    }
}


inline int Timing::stall_cycles_get()
{
    return this->stall_cycles;
}

inline void Timing::stall_cycles_dec()
{
    this->stall_cycles--;
}


inline void Timing::event_account(unsigned int event, int incr)
{
//   if (iss->csr.pcmr & CSR_PCMR_ACTIVE && (iss->csr.pcer & (1<<event)))
//   {
//     iss->csr.pccr[event] += incr;
//   }

//   iss->perf_event_incr(event, incr);

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

inline void Timing::insn_account()
{
    this->stall_cycles++;

    //   if (_this->csr.pcmr & CSR_PCMR_ACTIVE)
    //   {
    //     if (_this->csr.pcer & (1<<CSR_PCER_INSTR))
    //       _this->csr.pccr[CSR_PCER_INSTR] += 1;
    //   }
    //   _this->perf_event_incr(CSR_PCER_INSTR, 1);
}

inline void Timing::stall_fetch_account(int cycles)
{
    this->stall_cycles += cycles;
    // this->event_account(CSR_PCER_IMISS, cycles);
}

inline void Timing::stall_misaligned_account()
{
    this->stall_cycles += 1;
    // this->event_account(CSR_PCER_LD, 1);
}

inline void Timing::stall_load_account(int cycles)
{
    this->stall_cycles += cycles;
}

inline void Timing::stall_taken_branch_account()
{
    this->stall_cycles += 2;
    this->event_branch_account(1);
    this->event_taken_branch_account(1);
}

inline void Timing::stall_insn_account(int cycles)
{
    this->stall_cycles += cycles;
}

inline void Timing::stall_insn_dependency_account(int latency)
{
    this->stall_cycles += latency - 1;
}

inline void Timing::stall_load_dependency_account(int latency)
{
    this->stall_cycles += latency - 1;
    // this->event_account(CSR_PCER_LD_STALL, latency - 1);
}

inline void Timing::stall_jump_account()
{
    this->stall_cycles += 1;
    this->event_jump_account(1);
}
