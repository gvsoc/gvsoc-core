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

static int iss_parse_isa(Iss *iss)
{
    const char *current = iss->config.isa;
    int len = strlen(current);

    bool arch_rv32 = false;
    bool arch_rv64 = false;

    if (strncmp(current, "rv32", 4) == 0)
    {
        current += 4;
        len -= 4;
        arch_rv32 = true;
    }
    else if (strncmp(current, "rv64", 4) == 0)
    {
        current += 4;
        len -= 4;
        arch_rv64 = true;
    }
    else
    {
        iss->trace.force_warning("Unsupported ISA: %s\n", current);
        return -1;
    }

    iss_decode_activate_isa(iss, (char *)"priv");
    iss_decode_activate_isa(iss, (char *)"priv_pulp_v2");

    bool has_f = false;
    bool has_d = false;
    bool has_c = false;
    bool has_f16 = false;
    bool has_f16alt = false;
    bool has_f8 = false;
    bool has_fvec = false;
    bool has_faux = false;

    while (len > 0)
    {
        switch (*current)
        {
        case 'd':
            has_d = true; // D needs F
        case 'f':
            has_f = true;
        case 'i':
        case 'm':
        {
            char name[2];
            name[0] = *current;
            name[1] = 0;
            iss_decode_activate_isa(iss, name);
            current++;
            len--;
            break;
        }
        case 'c':
        {
            iss_decode_activate_isa(iss, (char *)"c");
            current++;
            len--;
            has_c = true;
            break;
        }
        case 'X':
        {
            char *token = strtok(strdup(current), "X");

            while (token)
            {

                iss_decode_activate_isa(iss, token);

                if (strcmp(token, "pulpv2") == 0)
                {
                    iss_isa_pulpv2_activate(iss);
                }
                else if (strcmp(token, "corev") == 0)
                {
                    iss_isa_corev_activate(iss);
                }
                else if (strcmp(token, "gap8") == 0)
                {
                    iss_isa_pulpv2_activate(iss);
                    iss_decode_activate_isa(iss, (char *)"pulpv2");
                }
                else if (strcmp(token, "f16") == 0)
                {
                    has_f16 = true;
                }
                else if (strcmp(token, "f16alt") == 0)
                {
                    has_f16alt = true;
                }
                else if (strcmp(token, "f8") == 0)
                {
                    has_f8 = true;
                }
                else if (strcmp(token, "fvec") == 0)
                {
                    has_fvec = true;
                }
                else if (strcmp(token, "faux") == 0)
                {
                    has_faux = true;
                }

                token = strtok(NULL, "X");
            }

            len = 0;

            break;
        }
        default:
            iss->trace.force_warning("Unknwon ISA descriptor: %c\n", *current);
            return -1;
        }
    }

    //
    // Activate inter-dependent ISA extension subsets
    //

    // Compressed floating-point instructions
    if (has_c)
    {
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"cf");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"cd");
    }

    // For F Extension
    if (has_f)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f");
        // Vectors
        if (has_fvec && has_d)
        { // make sure FLEN >= 64
            iss_decode_activate_isa(iss, (char *)"f32vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f32vecno32d");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            // nothing for scalars as expansions are to fp32
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f32auxvec");
        }
    }

    // For Xf16 Extension
    if (has_f16)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f16");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f16f");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f16d");
        // Vectors
        if (has_fvec && has_f)
        { // make sure FLEN >= 32
            iss_decode_activate_isa(iss, (char *)"f16vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f16vecno32d");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f16vecd");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f16aux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f16auxvec");
        }
    }

    // For Xf16alt Extension
    if (has_f16alt)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f16alt");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f16altf");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f16altd");
        if (has_f16)
            iss_decode_activate_isa(iss, (char *)"f16altf16");
        // Vectors
        if (has_fvec && has_f)
        { // make sure FLEN >= 32
            iss_decode_activate_isa(iss, (char *)"f16altvec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f16altvecno32d");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f16altvecd");
            if (has_f16)
                iss_decode_activate_isa(iss, (char *)"f16altvecf16");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f16altaux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f16altauxvec");
        }
    }

    // For Xf8 Extension
    if (has_f8)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f8");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f8f");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f8d");
        if (has_f16)
            iss_decode_activate_isa(iss, (char *)"f8f16");
        if (has_f16alt)
            iss_decode_activate_isa(iss, (char *)"f8f16alt");
        // Vectors
        if (has_fvec && (has_f16 || has_f16alt || has_f))
        { // make sure FLEN >= 16
            iss_decode_activate_isa(iss, (char *)"f8vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f8vecno32d");
            if (has_f)
                iss_decode_activate_isa(iss, (char *)"f8vecf");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f8vecd");
            if (has_f16)
                iss_decode_activate_isa(iss, (char *)"f8vecf16");
            if (has_f16alt)
                iss_decode_activate_isa(iss, (char *)"f8vecf16alt");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f8aux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f8auxvec");
        }
    }

    return 0;
}

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

        iss->prev_insn = NULL;
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

    if (iss_parse_isa(this))
        return -1;

    insn_cache_init(this);

    this->regfile.regs[0] = 0;
    this->current_insn = NULL;
    this->stall_insn = NULL;
    this->prev_insn = NULL;
    this->state.hwloop_end_insn[0] = NULL;
    this->state.hwloop_end_insn[1] = NULL;

    this->state.fcsr.frm = 0;

    this->irq.build();
    iss_resource_init(this);

    iss_trace_init(this);

    iss_init(this);

    return 0;
}

void iss_start(Iss *iss)
{
}

void Iss::pc_set(iss_addr_t value)
{
    this->current_insn = insn_cache_get(this, value);

    // Since the ISS needs to fetch the instruction in advanced, we force the core
    // to refetch the current instruction
    this->prefetcher.fetch(this->current_insn);
}

int Iss::build()
{
    if (this->prefetcher.build(*this))
    {
        return -1;
    }

    traces.new_trace("trace", &trace, vp::DEBUG);
    traces.new_trace("gdbserver_trace", &this->gdbserver.gdbserver_trace, vp::DEBUG);
    traces.new_trace("decode_trace", &decode_trace, vp::DEBUG);
    traces.new_trace("insn", &insn_trace, vp::DEBUG);
    this->insn_trace.register_callback(std::bind(&Iss::insn_trace_callback, this));

    traces.new_trace("csr", &csr_trace, vp::TRACE);
    traces.new_trace("perf", &perf_counter_trace, vp::TRACE);

    traces.new_trace_event("state", &state_event, 8);
    this->new_reg("busy", &this->busy, 1);
    traces.new_trace_event("pc", &pc_trace_event, 32);
    traces.new_trace_event("active_pc", &active_pc_trace_event, 32);
    this->pc_trace_event.register_callback(std::bind(&Iss::insn_trace_callback, this));
    traces.new_trace_event_string("asm", &insn_trace_event);
    traces.new_trace_event_string("func", &func_trace_event);
    traces.new_trace_event_string("inline_func", &inline_trace_event);
    traces.new_trace_event_string("file", &file_trace_event);
    traces.new_trace_event_string("binaries", &binaries_trace_event);
    traces.new_trace_event("line", &line_trace_event, 32);

    traces.new_trace_event_real("ipc_stat", &ipc_stat_event);

    this->new_reg("bootaddr", &this->bootaddr_reg, get_config_int("boot_addr"));

    this->new_reg("fetch_enable", &this->fetch_enable_reg, get_js_config()->get("fetch_enable")->get_bool());
    this->new_reg("is_active", &this->is_active_reg, false);
    this->new_reg("stalled", &this->exec.stalled, false);
    this->new_reg("wfi", &this->wfi, false);
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

    new_master_port("fetch", &fetch);

    dbg_unit.set_req_meth(&Iss::dbg_unit_req);
    new_slave_port("dbg_unit", &dbg_unit);

    bootaddr_itf.set_sync_meth(&Iss::bootaddr_sync);
    new_slave_port("bootaddr", &bootaddr_itf);

    clock_itf.set_sync_meth(&Iss::clock_sync);
    new_slave_port("clock", &clock_itf);

    irq_req_itf.set_sync_meth(&Irq::irq_req_sync);
    new_slave_port(&this->irq, "irq_req", &irq_req_itf);
    new_master_port("irq_ack", &irq_ack_itf);

    new_master_port("busy", &busy_itf);

    fetchen_itf.set_sync_meth(&Iss::fetchen_sync);
    new_slave_port("fetchen", &fetchen_itf);

    flush_cache_itf.set_sync_meth(&Iss::flush_cache_sync);
    new_slave_port("flush_cache", &flush_cache_itf);

    flush_cache_ack_itf.set_sync_meth(&Iss::flush_cache_ack_sync);
    new_slave_port("flush_cache_ack", &flush_cache_ack_itf);
    new_master_port("flush_cache_req", &flush_cache_req_itf);

    halt_itf.set_sync_meth(&Iss::halt_sync);
    new_slave_port("halt", &halt_itf);

    new_master_port("halt_status", &halt_status_itf);

    for (int i = 0; i < 32; i++)
    {
        new_master_port("ext_counter[" + std::to_string(i) + "]", &ext_counter[i]);
        this->pcer_info[i].name = "";
    }

    exec.instr_event = event_new(&this->exec, Exec::exec_first_instr);

    this->riscv_dbg_unit = this->get_js_config()->get_child_bool("riscv_dbg_unit");
    this->bootaddr_offset = get_config_int("bootaddr_offset");

    this->config.mhartid = (get_config_int("cluster_id") << 5) | get_config_int("core_id");
    this->config.misa = this->get_js_config()->get_int("misa");

    string isa = get_config_str("isa");
    // transform(isa.begin(), isa.end(), isa.begin(),(int (*)(int))tolower);
    this->config.isa = strdup(isa.c_str());
    this->config.debug_handler = this->get_js_config()->get_int("debug_handler");

    this->is_active_reg.set(false);

    ipc_clock_event = this->event_new(Iss::ipc_stat_handler);

    this->ipc_stat_delay = 0;
    this->iss_opened = false;

    return 0;
}

void Iss::start()
{

    vp_assert_always(this->data.is_bound(), &this->trace, "Data master port is not connected\n");
    vp_assert_always(this->fetch.is_bound(), &this->trace, "Fetch master port is not connected\n");
    // vp_assert_always(this->irq_ack_itf.is_bound(), &this->trace, "IRQ ack master port is not connected\n");

    if (this->iss_open())
        throw logic_error("Error while instantiating the ISS");

    this->target_open();

    this->iss_opened = true;

    for (auto x : this->get_js_config()->get("**/debug_binaries")->get_elems())
    {
        iss_register_debug_info(this, x->get_str().c_str());
    }

    trace.msg("ISS start (fetch: %d, is_active: %d, boot_addr: 0x%lx)\n", fetch_enable_reg.get(), is_active_reg.get(), get_config_int("boot_addr"));

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
    if (this->is_active_reg.get())
    {
        this->exec.instr_event->disable();
    }
}

void Iss::reset(bool active)
{
    this->prefetcher.reset(active);

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
        this->clock_active = false;

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

        this->pc_set(this->bootaddr_reg.get() + this->bootaddr_offset);
        this->irq.vector_table_set(this->bootaddr_reg.get() & ~((1 << 8) - 1));

        if (this->gdbserver.gdbserver)
        {
            this->halted.set(true);
        }

        this->exec.instr_event->enable();

        // In case, the core is not fetch-enabled, stall to prevent it from being active
        if (fetch_enable_reg.get() == false)
        {
            this->exec.stalled_inc();
        }
    }
}

Iss::Iss(js::config *config)
    : vp::component(config), prefetcher(*this), exec(*this), decode(*this), timing(*this), irq(*this), gdbserver(*this), lsu(*this)
{
}

void Iss::insn_trace_callback()
{
    // This is called when the state of the instruction trace has changed, we need
    // to flush the ISS instruction cache, as it keeps the state of the trace
    if (this->iss_opened)
    {
        iss_cache_flush(this);
    }
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
        this->trigger_ipc_stat();

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
        _this->exec.stalled_dec();
        _this->pc_set(_this->bootaddr_reg.get() + _this->bootaddr_offset);
    }
    else if (old_val && !active)
    {
        // In case of a falling edge, stall the core to prevent him from executing
        _this->exec.stalled_inc();
    }
}

void Iss::declare_pcer(int index, std::string name, std::string help)
{
    this->pcer_info[index].name = name;
    this->pcer_info[index].help = help;
}

void Iss::bootaddr_sync(void *__this, uint32_t value)
{
    Iss *_this = (Iss *)__this;
    _this->trace.msg("Setting boot address (value: 0x%x)\n", value);
    _this->bootaddr_reg.set(value);
    _this->irq.vector_table_set(_this->bootaddr_reg.get() & ~((1 << 8) - 1));
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

void Iss::target_open()
{
    this->declare_pcer(CSR_PCER_CYCLES, "Cycles", "Count the number of cycles the core was running");
    this->declare_pcer(CSR_PCER_INSTR, "instr", "Count the number of instructions executed");
    this->declare_pcer(CSR_PCER_LD_STALL, "ld_stall", "Number of load use hazards");
    this->declare_pcer(CSR_PCER_JMP_STALL, "jmp_stall", "Number of jump register hazards");
    this->declare_pcer(CSR_PCER_IMISS, "imiss", "Cycles waiting for instruction fetches. i.e. the number of instructions wasted due to non-ideal caches");
    this->declare_pcer(CSR_PCER_LD, "ld", "Number of memory loads executed. Misaligned accesses are counted twice");
    this->declare_pcer(CSR_PCER_ST, "st", "Number of memory stores executed. Misaligned accesses are counted twice");
    this->declare_pcer(CSR_PCER_JUMP, "jump", "Number of jump instructions seen, i.e. j, jr, jal, jalr");
    this->declare_pcer(CSR_PCER_BRANCH, "branch", "Number of branch instructions seen, i.e. bf, bnf");
    this->declare_pcer(CSR_PCER_TAKEN_BRANCH, "taken_branch", "Number of taken branch instructions seen, i.e. bf, bnf");
    this->declare_pcer(CSR_PCER_RVC, "rvc", "Number of compressed instructions");
    this->declare_pcer(CSR_PCER_LD_EXT, "ld_ext", "Number of memory loads to EXT executed. Misaligned accesses are counted twice. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_ST_EXT, "st_ext", "Number of memory stores to EXT executed. Misaligned accesses are counted twice. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_LD_EXT_CYC, "ld_ext_cycles", "Cycles used for memory loads to EXT. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_ST_EXT_CYC, "st_ext_cycles", "Cycles used for memory stores to EXT. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_TCDM_CONT, "tcdm_cont", "Cycles wasted due to TCDM/log-interconnect contention");

    traces.new_trace_event("pcer_cycles", &pcer_trace_event[0], 1);
    traces.new_trace_event("pcer_instr", &pcer_trace_event[1], 1);
    traces.new_trace_event("pcer_ld_stall", &pcer_trace_event[2], 1);
    traces.new_trace_event("pcer_jmp_stall", &pcer_trace_event[3], 1);
    traces.new_trace_event("pcer_imiss", &pcer_trace_event[4], 1);
    traces.new_trace_event("pcer_ld", &pcer_trace_event[5], 1);
    traces.new_trace_event("pcer_st", &pcer_trace_event[6], 1);
    traces.new_trace_event("pcer_jump", &pcer_trace_event[7], 1);
    traces.new_trace_event("pcer_branch", &pcer_trace_event[8], 1);
    traces.new_trace_event("pcer_taken_branch", &pcer_trace_event[9], 1);
    traces.new_trace_event("pcer_rvc", &pcer_trace_event[10], 1);
    traces.new_trace_event("pcer_ld_ext", &pcer_trace_event[11], 1);
    traces.new_trace_event("pcer_st_ext", &pcer_trace_event[12], 1);
    traces.new_trace_event("pcer_ld_ext_cycles", &pcer_trace_event[13], 1);
    traces.new_trace_event("pcer_st_ext_cycles", &pcer_trace_event[14], 1);
    traces.new_trace_event("pcer_tcdm_cont", &pcer_trace_event[15], 1);
}

#ifndef EXTERNAL_ISS_WRAPPER

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Iss(config);
}

#endif
