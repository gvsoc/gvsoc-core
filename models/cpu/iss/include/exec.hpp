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

#ifndef __CPU_ISS_ISS_INSN_EXEC_HPP
#define __CPU_ISS_ISS_INSN_EXEC_HPP

#if defined(ISS_HAS_PERF_COUNTERS)
void update_external_pccr(iss_t *iss, int id, unsigned int pcer, unsigned int pcmr);
#endif

static inline int iss_exec_account_cycles(iss_t *iss, int cycles);

iss_insn_t *iss_exec_insn_with_trace(iss_t *iss, iss_insn_t *insn);
void iss_trace_dump(iss_t *iss, iss_insn_t *insn);
void iss_trace_init(iss_t *iss);


static inline void iss_exec_insn_terminate(iss_t *iss)
{
  // Since we get 0 when there is no stall and we need to account the instruction cycle,
  // increase the count by 1
  iss_exec_account_cycles(iss, iss->cpu.state.insn_cycles + 1);

  if (iss_insn_trace_active(iss))
  {
    iss_trace_dump(iss, iss->cpu.stall_insn);
  }
}

static inline void iss_exec_insn_stall_save_insn(iss_t *iss)
{
  iss->cpu.stall_insn = iss->cpu.current_insn;
}

static inline void iss_exec_insn_stall(iss_t *iss)
{
  iss_exec_insn_stall_save_insn(iss);
  iss->stalled_inc();
}

static inline iss_insn_t *iss_exec_insn_handler(iss_t *instance, iss_insn_t *insn, iss_insn_t *(*handler)(iss_t *, iss_insn_t *))
{
  return handler(instance, insn);
}


static inline iss_insn_t *iss_exec_insn_fast(iss_t *iss, iss_insn_t *insn)
{
  return iss_exec_insn_handler(iss, insn, insn->fast_handler);
}

static inline iss_insn_t *iss_exec_insn(iss_t *iss, iss_insn_t *insn)
{
  return iss_exec_insn_handler(iss, insn, insn->handler);
}

static inline iss_insn_t *iss_exec_stalled_insn_fast(iss_t *iss, iss_insn_t *insn)
{
  iss_perf_account_dependency_stall(iss, insn->latency);
  return iss_exec_insn_handler(iss, insn, insn->stall_fast_handler);
}

static inline iss_insn_t *iss_exec_stalled_insn(iss_t *iss, iss_insn_t *insn)
{
  iss_perf_account_dependency_stall(iss, insn->latency);
  iss_pccr_account_event(iss, CSR_PCER_LD_STALL, 1);
  return iss_exec_insn_handler(iss, insn, insn->stall_handler);
}




static inline int iss_exec_switch_to_fast(iss_t *iss)
{
#ifdef VP_TRACE_ACTIVE
  return false;
#else
  return !(iss->cpu.csr.pcmr & CSR_PCMR_ACTIVE);
#endif
}

static inline int iss_exec_account_cycles(iss_t *iss, int cycles)
{
  if (iss->cpu.csr.pcmr & CSR_PCMR_ACTIVE)
  {
    if (cycles >= 0 && (iss->cpu.csr.pcer & (1<<CSR_PCER_CYCLES)))
    {
      iss->cpu.csr.pccr[CSR_PCER_CYCLES] += cycles;
    }
  }

  iss_pccr_incr(iss, CSR_PCER_CYCLES, cycles);

#if defined(ISS_HAS_PERF_COUNTERS)
  for (int i=CSR_PCER_NB_INTERNAL_EVENTS; i<CSR_PCER_NB_EVENTS; i++)
  {
    if (iss_pccr_trace_active(iss, i))
    {
      update_external_pccr(iss, i, iss->cpu.csr.pcer, iss->cpu.csr.pcmr);
    }
  }
#endif
  return 0;
}




static inline int iss_exec_is_stalled(iss_t *iss)
{
  return iss->stalled.get();
}


inline void iss_wrapper::iss_exec_insn_check_debug_step()
{
  if (this->step_mode.get() && !this->cpu.state.debug_mode)
  {
    this->do_step.set(false);
    this->hit_reg |= 1;
    if (this->gdbserver)
    {
      this->halted.set(true);
      this->gdbserver->signal(this);
    }
    else
    {
      this->set_halt_mode(true, HALT_CAUSE_STEP);
    }
    this->check_state();
  }
}


inline void iss_wrapper::stalled_inc()
{
  if (this->stalled.get() == 0)
  {
    this->instr_event->cancel();
  }
  this->stalled.inc(1);
}


inline void iss_wrapper::stalled_dec()
{
  if (this->stalled.get() == 0)
  {
    this->trace.warning("Trying to decrease zero stalled counter\n");
    return;
  }

  this->stalled.dec(1);

  if (this->stalled.get() == 0)
  {
    this->instr_event->enqueue();
  }
}

#endif
