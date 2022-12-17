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

#include "iss.hpp"
#include <string.h>

void iss_reset(Iss *iss, int active)
{
    iss->timing.reset(active);

    iss->irq.reset(active);

    if (active)
    {
        for (int i = 0; i < ISS_NB_TOTAL_REGS; i++)
        {
            iss->regfile.regs[i] = i == 0 ? 0 : 0x57575757;
        }

        iss_cache_flush(iss);

        iss->exec.prev_insn = NULL;
        iss->state.elw_insn = NULL;
        iss->elw_stalled.set(false);
        iss->state.cache_sync = false;
        iss->state.do_fetch = false;

        iss->state.hwloop_end_insn[0] = NULL;
        iss->state.hwloop_end_insn[1] = NULL;

        memset(iss->pulpv2.hwloop_regs, 0, sizeof(iss->pulpv2.hwloop_regs));
    }

    iss_csr_init(iss, active);
}

static inline void iss_init(Iss *iss)
{
    iss->io_req.set_data(new uint8_t[sizeof(iss_reg_t)]);
}

int Iss::iss_open()
{
    iss_isa_pulpv2_init(this);
    iss_isa_corev_init(this);

    if (this->decode.parse_isa())
        return -1;

    insn_cache_init(this);

    this->regfile.regs[0] = 0;
    this->exec.current_insn = NULL;
    this->exec.stall_insn = NULL;
    this->exec.prev_insn = NULL;
    this->state.hwloop_end_insn[0] = NULL;
    this->state.hwloop_end_insn[1] = NULL;

    this->state.fcsr.frm = 0;

    this->irq.build();
    iss_resource_init(this);

    iss_trace_init(this);

    iss_init(this);

    return 0;
}

int Iss::build()
{
    this->syscalls.build();
    this->decode.build();
    this->exec.build();
    this->dbgunit.build();
    this->csr.build();
    this->lsu.build();
    this->irq.build();
    this->trace.build();
    if (this->prefetcher.build(*this))
    {
        return -1;
    }

    traces.new_trace("wrapper", &this->wrapper_trace, vp::DEBUG);
    traces.new_trace("gdbserver_trace", &this->gdbserver.gdbserver_trace, vp::DEBUG);

    traces.new_trace_event("state", &state_event, 8);
    this->new_reg("busy", &this->exec.busy, 1);
    traces.new_trace_event("pc", &pc_trace_event, 32);
    traces.new_trace_event("active_pc", &active_pc_trace_event, 32);
    this->pc_trace_event.register_callback(std::bind(&Trace::insn_trace_callback, &this->trace));
    traces.new_trace_event_string("asm", &insn_trace_event);
    traces.new_trace_event_string("func", &func_trace_event);
    traces.new_trace_event_string("inline_func", &inline_trace_event);
    traces.new_trace_event_string("file", &file_trace_event);
    traces.new_trace_event_string("binaries", &binaries_trace_event);
    traces.new_trace_event("line", &line_trace_event, 32);

    traces.new_trace_event_real("ipc_stat", &ipc_stat_event);

    this->new_reg("bootaddr", &this->exec.bootaddr_reg, get_config_int("boot_addr"));

    this->new_reg("fetch_enable", &this->exec.fetch_enable_reg, get_js_config()->get("fetch_enable")->get_bool());
    this->new_reg("is_active", &this->exec.is_active_reg, false);
    this->new_reg("stalled", &this->exec.stalled, false);
    this->new_reg("wfi", &this->exec.wfi, false);
    this->new_reg("misaligned_access", &this->misaligned_access, false);
    this->new_reg("halted", &this->halted, false);
    this->new_reg("step_mode", &this->step_mode, false);
    this->new_reg("do_step", &this->do_step, false);

    this->new_reg("elw_stalled", &this->elw_stalled, false);

    if (this->get_js_config()->get("**/insn_groups"))
    {
        js::config *config = this->get_js_config()->get("**/insn_groups");
        this->insn_groups_power.resize(config->get_size());
        for (int i = 0; i < config->get_size(); i++)
        {
            power.new_power_source("power_insn_" + std::to_string(i), &this->insn_groups_power[i], config->get_elem(i));
        }
    }
    else
    {
        this->insn_groups_power.resize(1);
        power.new_power_source("power_insn", &this->insn_groups_power[0], this->get_js_config()->get("**/insn"));
    }

    power.new_power_source("background", &background_power, this->get_js_config()->get("**/power_models/background"));

    data.set_resp_meth(&Lsu::data_response);
    data.set_grant_meth(&Lsu::data_grant);
    new_master_port(&this->lsu, "data", &data);

    this->fetch.set_resp_meth(&Prefetcher::fetch_response);
    new_master_port(&this->prefetcher, "fetch", &fetch);

    dbg_unit.set_req_meth(&DbgUnit::dbg_unit_req);
    new_slave_port(&this->dbgunit, "dbg_unit", &dbg_unit);

    irq_req_itf.set_sync_meth(&Irq::irq_req_sync);
    new_slave_port(&this->irq, "irq_req", &irq_req_itf);
    new_master_port("irq_ack", &irq_ack_itf);

    flush_cache_itf.set_sync_meth(&Decode::flush_cache_sync);
    new_slave_port(&this->decode, "flush_cache", &flush_cache_itf);


    halt_itf.set_sync_meth(&DbgUnit::halt_sync);
    new_slave_port(&this->dbgunit, "halt", &halt_itf);

    new_master_port("halt_status", &halt_status_itf);

    for (int i = 0; i < 32; i++)
    {
        new_master_port("ext_counter[" + std::to_string(i) + "]", &ext_counter[i]);
        this->pcer_info[i].name = "";
    }

    exec.instr_event = event_new(&this->exec, Exec::exec_first_instr);

    this->riscv_dbg_unit = this->get_js_config()->get_child_bool("riscv_dbg_unit");
    this->exec.bootaddr_offset = get_config_int("bootaddr_offset");

    this->config.mhartid = (get_config_int("cluster_id") << 5) | get_config_int("core_id");
    this->config.misa = this->get_js_config()->get_int("misa");

    string isa = get_config_str("isa");
    // transform(isa.begin(), isa.end(), isa.begin(),(int (*)(int))tolower);
    this->config.isa = strdup(isa.c_str());
    this->config.debug_handler = this->get_js_config()->get_int("debug_handler");

    this->exec.is_active_reg.set(false);

    ipc_clock_event = this->event_new(&this->timing, Timing::ipc_stat_handler);

    this->ipc_stat_delay = 0;
    this->iss_opened = false;

    return 0;
}

void Iss::start()
{

    vp_assert_always(this->data.is_bound(), &this->wrapper_trace, "Data master port is not connected\n");
    vp_assert_always(this->fetch.is_bound(), &this->wrapper_trace, "Fetch master port is not connected\n");
    // vp_assert_always(this->irq_ack_itf.is_bound(), &this->trace, "IRQ ack master port is not connected\n");

    if (this->iss_open())
        throw logic_error("Error while instantiating the ISS");

    this->target_open();

    this->iss_opened = true;

    for (auto x : this->get_js_config()->get("**/debug_binaries")->get_elems())
    {
        iss_register_debug_info(this, x->get_str().c_str());
    }

    wrapper_trace.msg("ISS start (fetch: %d, is_active: %d, boot_addr: 0x%lx)\n",
        exec.fetch_enable_reg.get(), exec.is_active_reg.get(), get_config_int("boot_addr"));

#ifdef USE_TRDB
    this->trdb = trdb_new();
    INIT_LIST_HEAD(&this->trdb_packet_list);
#endif

    this->background_power.leakage_power_start();
    this->background_power.dynamic_power_start();

    this->gdbserver.start();
}

void Iss::pre_reset()
{
    if (this->exec.is_active_reg.get())
    {
        this->exec.instr_event->disable();
    }
}

void Iss::reset(bool active)
{
    this->prefetcher.reset(active);
    this->exec.reset(active);

    if (active)
    {
        this->irq_req = -1;
        this->wakeup_latency = 0;

        for (int i = 0; i < 32; i++)
        {
            this->pcer_trace_event[i].event(NULL);
        }

        this->ipc_stat_nb_insn = 0;
        this->ipc_stat_delay = 10;

        this->dump_trace_enabled = true;

        this->active_pc_trace_event.event(NULL);

        if (this->get_js_config()->get("**/binaries") != NULL)
        {
            std::string binaries = "static enable";
            for (auto x : this->get_js_config()->get("**/binaries")->get_elems())
            {
                binaries += " " + x->get_str();
            }

            this->binaries_trace_event.event_string(binaries);
        }

        iss_reset(this, 1);
    }
    else
    {
        iss_reset(this, 0);

        uint64_t zero = 0;
        for (int i = 0; i < 32; i++)
        {
            this->pcer_trace_event[i].event((uint8_t *)&zero);
        }

        this->exec.pc_set(this->exec.bootaddr_reg.get() + this->exec.bootaddr_offset);
        this->irq.vector_table_set(this->exec.bootaddr_reg.get() & ~((1 << 8) - 1));

        if (this->gdbserver.gdbserver)
        {
            this->halted.set(true);
        }

        this->exec.instr_event->enable();

        // In case, the core is not fetch-enabled, stall to prevent it from being active
        if (exec.fetch_enable_reg.get() == false)
        {
            this->exec.stalled_inc();
        }
    }
}

Iss::Iss(js::config *config)
    : vp::component(config), prefetcher(*this), exec(*this), decode(*this), timing(*this), irq(*this),
      gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(*this), trace(*this), csr(*this)
{
}

#if 0
// TODO this is deprecated, some parts must be reintegrated
void Iss::check_state()
{
  return;

  this->exec.switch_to_full_mode();

  if (!is_active_reg.get())
  {

    this->trace.msg(vp::trace::LEVEL_TRACE, "Checking state (active: %d, halted: %d, fetch_enable: %d, stalled: %d, wfi: %d, irq_req: %d, debug_mode: %d)\n",
      is_active_reg.get(), halted.get(), fetch_enable_reg.get(), stalled.get(), wfi.get(), irq_req, this->state.debug_mode);

    if (!halted.get() && !stalled.get() && (!wfi.get() || irq_req != -1 || this->state.debug_mode))
    {
      // Check if we can directly reenqueue next instruction because it has already
      // been fetched or we need to fetch it
      if (this->state.do_fetch)
      {
        // In case we need to fetch it, first trigger the fetch
        prefetcher_fetch(this, this->current_insn);

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
        this->ipc_stat_trigger();

      if (step_mode.get() && !this->state.debug_mode)
      {
        do_step.set(true);
      }

      if (this->csr.pcmr & CSR_PCMR_ACTIVE && this->csr.pcer & (1<<CSR_PCER_CYCLES))
      {
        this->csr.pccr[CSR_PCER_CYCLES] += 1 + this->wakeup_latency;
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
        this->ipc_stat_stop();
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
          this->ipc_stat_stop();
      }
      else
        wfi.set(false);
    }
  }
}
#endif

void Iss::target_open()
{

}

#ifndef EXTERNAL_ISS_WRAPPER

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Iss(config);
}

#endif
