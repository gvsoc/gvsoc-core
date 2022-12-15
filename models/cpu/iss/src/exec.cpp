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


#include <vp/vp.hpp>
#include "iss.hpp"


static inline void iss_handle_insn_profiling(Iss *_this)
{
  _this->trace.msg("Executing instruction\n");
  if (_this->pc_trace_event.get_event_active())
  {
    _this->pc_trace_event.event((uint8_t *)&_this->current_insn->addr);
  }
  if (_this->active_pc_trace_event.get_event_active())
  {
    _this->active_pc_trace_event.event((uint8_t *)&_this->current_insn->addr);
  }
  if (_this->func_trace_event.get_event_active() || _this->inline_trace_event.get_event_active() || _this->file_trace_event.get_event_active() || _this->line_trace_event.get_event_active())
  {
    _this->dump_debug_traces();
  }
  if (_this->ipc_stat_event.get_event_active())
  {
    _this->ipc_stat_nb_insn++;
  }
}


void static inline iss_handle_insn_power(Iss *_this, iss_insn_t *insn)
{
  if (_this->power.get_power_trace()->get_active())
  {
    _this->insn_groups_power[insn->decoder_item->u.insn.power_group].account_energy_quantum();
  }
}



void Iss::exec_instr(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with fast handler (insn_cycles: %d)\n", _this->timing.stall_cycles_get());

  if (likely(_this->timing.stall_cycles_get() == 0))
  {
    // Takes care first of all optional features (traces, VCD and so on)
    iss_handle_insn_profiling(_this);

    iss_insn_t *insn = _this->current_insn;

    // Execute the instruction and replace the current one with the new one
    _this->current_insn = iss_exec_insn_fast(_this, insn);
    _this->prev_insn = insn;

    // Now that we have the new instruction, we can fetch it. In case the response is asynchronous,
    // this will stall the ISS, which will execute the next instruction when the response is
    // received
    _this->prefetcher.fetch(_this->current_insn);

    // Since power instruction information is filled when the instruction is decoded,
    // make sure we account it only after the instruction is executed
    iss_handle_insn_power(_this, insn);
  }
  else
  {
    _this->timing.stall_cycles_dec();
  }
}



void Iss::exec_instr_check_all(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with slow handler\n");

  if (likely(_this->timing.stall_cycles_get() == 0))
  {
    // Switch back to optimize instruction handler only
    // if HW counters are disabled as they are checked with the slow handler
    if (iss_exec_switch_to_fast(_this))
    {
      _this->exec.instr_event->meth_set(&Iss::exec_instr);
    }

    iss_handle_insn_profiling(_this);

    int cycles;

    iss_insn_t *insn = _this->current_insn;

    // Don't execute the instruction if an IRQ was taken and it triggered a pending fetch
    if (!_this->irq.check() && !_this->exec.stalled.get())
    {
      iss_insn_t *insn = _this->current_insn;
      _this->current_insn = iss_exec_insn(_this, insn);
      _this->prev_insn = insn;

      _this->prefetcher.fetch(_this->current_insn);

      _this->timing.insn_account();
    }

    iss_handle_insn_power(_this, insn);

    _this->iss_exec_insn_check_debug_step();
  }
  else
  {
    _this->timing.stall_cycles_dec();
  }
}

void Iss::exec_first_instr(vp::clock_event *event)
{
  this->exec.instr_event->meth_set(&Iss::exec_instr);
  iss_start(this);
  exec_instr((void *)this, event);
}

void Iss::exec_first_instr(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;
  _this->exec_first_instr(event);
}
