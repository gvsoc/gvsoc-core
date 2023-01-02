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



Exec::Exec(Iss &iss)
    : iss(iss)
{
}



void Exec::build()
{
    this->iss.top.traces.new_trace("exec", &this->trace, vp::DEBUG);

    this->iss.top.new_master_port("busy", &busy_itf);

    flush_cache_ack_itf.set_sync_meth(&Exec::flush_cache_ack_sync);
    this->iss.top.new_slave_port(this, "flush_cache_ack", &flush_cache_ack_itf);
    this->iss.top.new_master_port("flush_cache_req", &flush_cache_req_itf);

    bootaddr_itf.set_sync_meth(&Exec::bootaddr_sync);
    this->iss.top.new_slave_port(this, "bootaddr", &bootaddr_itf);

    clock_itf.set_sync_meth(&Exec::clock_sync);
    this->iss.top.new_slave_port(this, "clock", &clock_itf);

    fetchen_itf.set_sync_meth(&Exec::fetchen_sync);
    this->iss.top.new_slave_port(this, "fetchen", &fetchen_itf);

    this->iss.top.new_reg("step_mode", &this->step_mode, false);

    this->iss.top.new_reg("halted", &this->halted, false);

    this->iss.top.new_reg("busy", &this->busy, 1);

    this->iss.top.new_reg("bootaddr", &this->bootaddr_reg, this->iss.top.get_config_int("boot_addr"));

    this->iss.top.new_reg("fetch_enable", &this->fetch_enable_reg, this->iss.top.get_js_config()->get("fetch_enable")->get_bool());
    this->iss.top.new_reg("is_active", &this->is_active_reg, false);
    this->iss.top.new_reg("stalled", &this->stalled, false);
    this->iss.top.new_reg("wfi", &this->wfi, false);

    instr_event = this->iss.top.event_new(&this->iss, Exec::exec_instr_check_all);

    this->bootaddr_offset = this->iss.top.get_config_int("bootaddr_offset");
    this->is_active_reg.set(false);


    this->current_insn = NULL;
    this->stall_insn = NULL;
    this->hwloop_end_insn[0] = NULL;
    this->hwloop_end_insn[1] = NULL;

    iss_resource_init(&this->iss);

}



void Exec::reset(bool active)
{
    if (active)
    {
        this->clock_active = false;
        this->pc_set(this->bootaddr_reg.get() + this->bootaddr_offset);

        this->instr_event->disable();
    }
    else
    {
        this->elw_insn = NULL;
        this->cache_sync = false;
        this->hwloop_end_insn[0] = NULL;
        this->hwloop_end_insn[1] = NULL;
        if (this->iss.gdbserver.gdbserver)
        {
            this->halted.set(true);
        }

        this->instr_event->enable();

        // In case, the core is not fetch-enabled, stall to prevent it from being active
        if (fetch_enable_reg.get() == false)
        {
            this->stalled_inc();
        }
    }
}



void Exec::icache_flush()
{
    if (this->flush_cache_req_itf.is_bound())
    {
        this->cache_sync = true;
        this->insn_stall();
        this->flush_cache_req_itf.sync(true);
        iss_cache_flush(&this->iss);
    }
}



void Exec::dbg_unit_step_check()
{
    if (this->iss.exec.step_mode.get() && !this->iss.exec.debug_mode)
    {
        this->iss.dbgunit.do_step.set(false);
        this->iss.dbgunit.hit_reg |= 1;
        if (this->iss.gdbserver.gdbserver)
        {
            this->iss.exec.halted.set(true);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver);
        }
        else
        {
            this->iss.dbgunit.set_halt_mode(true, HALT_CAUSE_STEP);
        }
    }
}


void Exec::exec_instr_untimed(void *__this, vp::clock_event *event)
{
    Iss *const iss = (Iss *)__this;
    iss->exec.trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with fast handler\n");
    iss->exec.loop_count = 64;

    iss_insn_t *insn = iss->exec.current_insn;
    while(1)
    {
        bool stalled;

        // Execute the instruction and replace the current one with the new one
        insn = insn->fast_handler(iss, insn);
        stalled = iss->exec.stalled.get();
        iss->exec.current_insn = insn;
        if (stalled) break;
    }
}


void Exec::exec_instr(void *__this, vp::clock_event *event)
{
    Iss *const iss = (Iss *)__this;

    iss->exec.trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with fast handler\n");

    iss_insn_t *insn = iss->exec.current_insn;

    if (iss->prefetcher.fetch(insn))
    {
        // Takes care first of all optional features (traces, VCD and so on)
        iss->exec.insn_exec_profiling();

        // Execute the instruction and replace the current one with the new one
        iss->exec.current_insn = insn->fast_handler(iss, insn);

        // Since power instruction information is filled when the instruction is decoded,
        // make sure we account it only after the instruction is executed
        iss->exec.insn_exec_power(insn);
    }
}



void Exec::exec_instr_check_all(void *__this, vp::clock_event *event)
{
    Iss *iss = (Iss *)__this;
    Exec *_this = &iss->exec;

    _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling instruction with slow handler\n");

    // Switch back to optimize instruction handler only
    // if HW counters are disabled as they are checked with the slow handler
    if (_this->can_switch_to_fast_mode())
    {
        _this->instr_event->meth_set(&_this->iss, &Exec::exec_instr);
    }

    _this->insn_exec_profiling();

    int cycles;

    _this->iss.irq.check();

    iss_insn_t *insn = _this->current_insn;

    if (iss->prefetcher.fetch(insn))
    {
        _this->current_insn = _this->insn_exec(insn);

        _this->iss.timing.insn_account();

        _this->insn_exec_power(insn);

        _this->dbg_unit_step_check();
    }
}



void Exec::clock_sync(void *__this, bool active)
{
    Exec *_this = (Exec *)__this;
    _this->trace.msg("Setting clock (active: %d)\n", active);

    _this->clock_active = active;
}



void Exec::fetchen_sync(void *__this, bool active)
{
    Exec *_this = (Exec *)__this;
    _this->trace.msg("Setting fetch enable (active: %d)\n", active);
    int old_val = _this->fetch_enable_reg.get();
    _this->fetch_enable_reg.set(active);
    if (!old_val && active)
    {
        _this->stalled_dec();
        _this->pc_set(_this->bootaddr_reg.get() + _this->bootaddr_offset);
    }
    else if (old_val && !active)
    {
        // In case of a falling edge, stall the core to prevent him from executing
        _this->stalled_inc();
    }
}



void Exec::bootaddr_sync(void *__this, uint32_t value)
{
    Exec *_this = (Exec *)__this;
    _this->trace.msg("Setting boot address (value: 0x%x)\n", value);
    _this->bootaddr_reg.set(value);
    _this->iss.irq.vector_table_set(_this->bootaddr_reg.get() & ~((1 << 8) - 1));
}



void Exec::pc_set(iss_addr_t value)
{
    this->current_insn = insn_cache_get(&this->iss, value);
}



void Exec::flush_cache_ack_sync(void *__this, bool active)
{
    Exec *_this = (Exec *)__this;
    if (_this->iss.exec.cache_sync)
    {
        _this->iss.exec.cache_sync = false;
        _this->stalled_dec();
        _this->insn_terminate();
    }
}
