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
#include <vp/itf/io.hpp>
#include "iss.hpp"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


void Iss::dump_debug_traces()
{
  const char *func, *inline_func, *file;
  int line;

  if (!iss_trace_pc_info(this->cpu.current_insn->addr, &func, &inline_func, &file, &line))
  {
    this->func_trace_event.event_string(func);
    this->inline_trace_event.event_string(inline_func);
    this->file_trace_event.event_string(file);
    this->line_trace_event.event((uint8_t *)&line);
  }
}

static inline void iss_handle_insn_profiling(Iss *_this)
{
  _this->trace.msg("Executing instruction\n");
  if (_this->pc_trace_event.get_event_active())
  {
    _this->pc_trace_event.event((uint8_t *)&_this->cpu.current_insn->addr);
  }
  if (_this->active_pc_trace_event.get_event_active())
  {
    _this->active_pc_trace_event.event((uint8_t *)&_this->cpu.current_insn->addr);
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


void Iss::refetch_handler(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  // This handler is scheduled when we need to refetch the current instruction, for example when
  // the PC has been modified.

  // First set back normal handler
  _this->trigger_check_all();

  // And then trigger the fetch
  // The execution of the next instruction is managed automatically depending
  // on the value of the stall counter.
  prefetcher_fetch(_this, _this->cpu.current_insn);
}


void Iss::exec_instr(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with fast handler (insn_cycles: %d)\n", _this->cpu.state.insn_cycles);

  if (likely(_this->cpu.state.insn_cycles == 0))
  {
    // Takes care first of all optional features (traces, VCD and so on)
    iss_handle_insn_profiling(_this);

    iss_insn_t *insn = _this->cpu.current_insn;

    // Execute the instruction and replace the current one with the new one
    _this->cpu.current_insn = iss_exec_insn_fast(_this, insn);
    _this->cpu.prev_insn = insn;

    // Now that we have the new instruction, we can fetch it. In case the response is asynchronous,
    // this will stall the ISS, which will execute the next instruction when the response is
    // received
    prefetcher_fetch(_this, _this->cpu.current_insn);

    // Since power instruction information is filled when the instruction is decoded,
    // make sure we account it only after the instruction is executed
    iss_handle_insn_power(_this, insn);
  }
  else
  {
    _this->cpu.state.insn_cycles--;
  }
}



void Iss::exec_instr_check_all(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with slow handler\n");

  if (likely(_this->cpu.state.insn_cycles == 0))
  {
    // Switch back to optimize instruction handler only
    // if HW counters are disabled as they are checked with the slow handler
    if (iss_exec_switch_to_fast(_this))
    {
      _this->instr_event->meth_set(&Iss::exec_instr);
    }

    iss_handle_insn_profiling(_this);

    int cycles;

    iss_insn_t *insn = _this->cpu.current_insn;

    // Don't execute the instruction if an IRQ was taken and it triggered a pending fetch
    if (!iss_irq_check(_this) && !_this->stalled.get())
    {
      iss_insn_t *insn = _this->cpu.current_insn;
      _this->cpu.current_insn = iss_exec_insn(_this, insn);
      _this->cpu.prev_insn = insn;

      prefetcher_fetch(_this, _this->cpu.current_insn);

      // Since we get 0 when there is no stall and we need to account the instruction cycle,
      // increase the count by 1
      iss_exec_account_cycles(_this, _this->cpu.state.insn_cycles + 1);

      if (_this->cpu.csr.pcmr & CSR_PCMR_ACTIVE)
      {
        if (_this->cpu.csr.pcer & (1<<CSR_PCER_INSTR))
          _this->cpu.csr.pccr[CSR_PCER_INSTR] += 1;
      }
      _this->perf_event_incr(CSR_PCER_INSTR, 1);
    }

    iss_handle_insn_power(_this, insn);

    _this->iss_exec_insn_check_debug_step();
  }
  else
  {
    _this->cpu.state.insn_cycles--;
  }
}

void Iss::exec_first_instr(vp::clock_event *event)
{
  this->instr_event->meth_set(&Iss::exec_instr);
  iss_start(this);
  exec_instr((void *)this, event);
}

void Iss::exec_first_instr(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;
  _this->exec_first_instr(event);
}

void Iss::data_grant(void *__this, vp::io_req *req)
{
}

void Iss::data_response(void *__this, vp::io_req *req)
{
  Iss *_this = (Iss *)__this;
  _this->stalled_dec();

  _this->trace.msg("Received data response (stalled: %d)\n", _this->stalled.get());

  _this->wakeup_latency = req->get_latency();
  if (_this->misaligned_access.get())
  {
    _this->misaligned_access.set(false);
  }
  else
  {
    // First call the ISS to finish the instruction
    _this->cpu.state.stall_callback(_this);
    iss_exec_insn_terminate(_this);
  }
}

void Iss::fetch_grant(void *_this, vp::io_req *req)
{

}

void Iss::fetch_response(void *__this, vp::io_req *req)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Received fetch response\n");

  _this->stalled_dec();
  if (_this->cpu.state.fetch_stall_callback)
  {
    _this->cpu.state.fetch_stall_callback(_this);
  }
}

void Iss::bootaddr_sync(void *__this, uint32_t value)
{
  Iss *_this = (Iss *)__this;
  _this->trace.msg("Setting boot address (value: 0x%x)\n", value);
  _this->bootaddr_reg.set(value);
  iss_irq_set_vector_table(_this, _this->bootaddr_reg.get() & ~((1<<8) - 1));
}

void Iss::gen_ipc_stat(bool pulse)
{
  if (!pulse)
    this->ipc_stat_event.event_real((float)this->ipc_stat_nb_insn / 100);
  else
    this->ipc_stat_event.event_real_pulse(this->get_period(), (float)this->ipc_stat_nb_insn / 100, 0);

  this->ipc_stat_nb_insn = 0;
  if (this->ipc_stat_delay == 10)
    this->ipc_stat_delay = 30;
  else
    this->ipc_stat_delay = 100;
}

void Iss::trigger_ipc_stat()
{
  // In case the core is resuming execution, set IPC to 1 as we are executing
  // first instruction to not have the signal to zero.
  if (this->ipc_stat_delay == 10)
  {
    this->ipc_stat_event.event_real(1.0);
  }

  if (this->ipc_stat_event.get_event_active() && !this->ipc_clock_event->is_enqueued() && this->is_active_reg.get() && this->clock_active)
  {
    this->event_enqueue(this->ipc_clock_event, this->ipc_stat_delay);
  }
}

void Iss::stop_ipc_stat()
{
  this->gen_ipc_stat(true);
  if (this->ipc_clock_event->is_enqueued())
  {
    this->event_cancel(this->ipc_clock_event);
  }
  this->ipc_stat_delay = 10;
}

void Iss::ipc_stat_handler(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;
  _this->gen_ipc_stat();
  _this->trigger_ipc_stat();
}

void Iss::clock_sync(void *__this, bool active)
{
  Iss *_this = (Iss *)__this;
  _this->trace.msg("Setting clock (active: %d)\n", active);

  _this->clock_active = active;
}

void Iss::fetchen_sync(void *__this, bool active)
{
  Iss *_this = (Iss *)__this;
  _this->trace.msg("Setting fetch enable (active: %d)\n", active);
  int old_val = _this->fetch_enable_reg.get();
  _this->fetch_enable_reg.set(active);
  if (!old_val && active)
  {
    _this->stalled_dec();
    iss_pc_set(_this, _this->bootaddr_reg.get() + _this->bootaddr_offset);
  }
  else if (old_val && !active)
  {
      // In case of a falling edge, stall the core to prevent him from executing
      _this->stalled_inc();
  }
}

void Iss::flush_cache_sync(void *__this, bool active)
{
  Iss *_this = (Iss *)__this;
  if (_this->iss_opened)
  {
    iss_cache_flush(_this);
  }
}

void Iss::flush_cache_ack_sync(void *__this, bool active)
{
    Iss *_this = (Iss *)__this;
    if (_this->cpu.state.cache_sync)
    {
      _this->cpu.state.cache_sync = false;
      _this->stalled_dec();
      iss_exec_insn_terminate(_this);
    }
}

#if 0
void Iss::check_state()
{
  return;

  this->trigger_check_all();

  if (!is_active_reg.get())
  {

    this->trace.msg(vp::trace::LEVEL_TRACE, "Checking state (active: %d, halted: %d, fetch_enable: %d, stalled: %d, wfi: %d, irq_req: %d, debug_mode: %d)\n",
      is_active_reg.get(), halted.get(), fetch_enable_reg.get(), stalled.get(), wfi.get(), irq_req, this->cpu.state.debug_mode);

    if (!halted.get() && !stalled.get() && (!wfi.get() || irq_req != -1 || this->cpu.state.debug_mode))
    {
      // Check if we can directly reenqueue next instruction because it has already
      // been fetched or we need to fetch it
      if (this->cpu.state.do_fetch)
      {
        // In case we need to fetch it, first trigger the fetch
        prefetcher_fetch(this, this->cpu.current_insn);

        // Then enqueue the instruction only if the fetch ws completed synchronously.
        // Otherwise, the next instruction will be enqueued when we receive the fetch
        // response
        if (this->stalled.get())
        {
          return;
        }
      }

      wfi.set(false);
      is_active_reg.set(true);
      uint8_t one = 1;
      this->state_event.event(&one);
      this->busy.set(1);
      if (this->ipc_stat_event.get_event_active())
        this->trigger_ipc_stat();

      if (step_mode.get() && !this->cpu.state.debug_mode)
      {
        do_step.set(true);
      }

      if (this->cpu.csr.pcmr & CSR_PCMR_ACTIVE && this->cpu.csr.pcer & (1<<CSR_PCER_CYCLES))
      {
        this->cpu.csr.pccr[CSR_PCER_CYCLES] += 1 + this->wakeup_latency;
      }

      enqueue_next_instr(1 + this->wakeup_latency);

      this->wakeup_latency = 0;
    }
  }
  else
  {
    if (halted.get() && !do_step.get())
    {
      if (this->active_pc_trace_event.get_event_active())
        this->active_pc_trace_event.event(NULL);
      is_active_reg.set(false);
      this->state_event.event(NULL);
      if (this->ipc_stat_event.get_event_active())
        this->stop_ipc_stat();
      this->halt_core();
    }
    else if (wfi.get())
    {
      if (irq_req == -1)
      {
        if (this->active_pc_trace_event.get_event_active())
          this->active_pc_trace_event.event(NULL);
        is_active_reg.set(false);
        this->state_event.event(NULL);
        this->busy.release();
        if (this->ipc_stat_event.get_event_active())
          this->stop_ipc_stat();
      }
      else
        wfi.set(false);
    }
  }
}
#endif


int Iss::data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{

  iss_addr_t addr0 = addr & ADDR_MASK;
  iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

  decode_trace.msg("Misaligned data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

  iss_pccr_account_event(this, CSR_PCER_MISALIGNED, 1);

  // The access is a misaligned access
  // Change the event so that we can do the first access now and the next access
  // during the next cycle
  int size0 = addr1 - addr;
  int size1 = size - size0;
  
  misaligned_access.set(true);

  // Remember the access properties for the second access
  misaligned_size = size1;
  misaligned_data = data_ptr + size0;
  misaligned_addr = addr1;
  misaligned_is_write = is_write;

  // And do the first one now
  int err = data_req_aligned(addr, data_ptr, size0, is_write);
  if (err == vp::IO_REQ_OK)
  {
    // As the transaction is split into 2 parts, we must tell the ISS
    // that the access is pending as the instruction must be executed
    // only when the second access is finished.
    this->instr_event->meth_set(&Iss::exec_misaligned);
    this->cpu.state.insn_cycles += io_req.get_latency() + 1;
    this->dump_trace_enabled = false;
    iss_exec_insn_stall_save_insn(this);
    return 1;
  }
  else
  {
    trace.force_warning("UNIMPLEMENTED AT %s %d, error during misaligned access\n", __FILE__, __LINE__);
    return 0;
  }
}

void Iss::declare_pcer(int index, std::string name, std::string help)
{
    this->pcer_info[index].name = name;
    this->pcer_info[index].help = help;
}
