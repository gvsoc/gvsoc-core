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
#include "cpu/iss/include/iss.hpp"



Exec::Exec(IssWrapper &top, Iss &iss)
    : iss(iss), instr_event(&top, (vp::Block *)&iss, &Exec::exec_instr_check_all)
{
}



void Exec::build()
{
    this->iss.top.traces.new_trace("exec", &this->trace, vp::DEBUG);

    this->iss.top.new_master_port("busy", &busy_itf);

    this->iss.top.new_master_port("offload", &this->offload_itf);

    this->offload_grant_itf.set_sync_meth(&Exec::offload_grant);
    this->iss.top.new_slave_port("offload_grant", &this->offload_grant_itf, (vp::Block *)this);

    flush_cache_ack_itf.set_sync_meth(&Exec::flush_cache_ack_sync);
    this->iss.top.new_slave_port("flush_cache_ack", &flush_cache_ack_itf, (vp::Block *)this);
    this->iss.top.new_master_port("flush_cache_req", &flush_cache_req_itf);

    bootaddr_itf.set_sync_meth(&Exec::bootaddr_sync);
    this->iss.top.new_slave_port("bootaddr", &bootaddr_itf, (vp::Block *)this);

    clock_itf.set_sync_meth(&Exec::clock_sync);
    this->iss.top.new_slave_port("clock_en", &clock_itf, (vp::Block *)this);

    fetchen_itf.set_sync_meth(&Exec::fetchen_sync);
    this->iss.top.new_slave_port("fetchen", &fetchen_itf, (vp::Block *)this);

    this->iss.top.new_reg("step_mode", &this->step_mode, false);

    this->iss.top.new_reg("halted", &this->halted, false, false);

    this->iss.top.new_reg("busy", &this->busy, 1);

    this->iss.top.new_reg("bootaddr", &this->bootaddr_reg, this->iss.top.get_js_config()->get_child_int("boot_addr"));

    this->iss.top.new_reg("fetch_enable", &this->fetch_enable_reg, 0, true);
    this->iss.top.new_reg("stalled", &this->stalled, 0);
    this->iss.top.new_reg("wfi", &this->wfi, false);

    this->stalled.set(false);
    this->halted.set(false);

    this->bootaddr_offset = this->iss.top.get_js_config()->get_child_int("bootaddr_offset");


    this->current_insn = 0;
    this->stall_insn = 0;
#if defined(CONFIG_GVSOC_ISS_RI5KY)
    this->hwloop_end_insn[0] = 0;
    this->hwloop_end_insn[1] = 0;
#endif
}



void Exec::reset(bool active)
{
    if (active)
    {
        this->pending_flush = false;
        this->clock_active = false;
        this->skip_irq_check = true;
        this->has_exception = false;
        this->pc_set(this->bootaddr_reg.get() + this->bootaddr_offset);

        this->insn_table_index = 0;
        this->irq_locked = 0;
        this->insn_on_hold = false;
        this->stall_cycles = 0;

        // Always increase the stall when reset is asserted since stall count is set to 0
        // and we need to prevent the core from fetching instructions
        this->stalled_inc();
    }
    else
    {
        this->elw_insn = 0;
        this->cache_sync = false;
#if defined(CONFIG_GVSOC_ISS_RI5KY)
        this->hwloop_end_insn[0] = 0;
        this->hwloop_end_insn[1] = 0;
#endif

        // Check if the core should start fetching, if so this will unstall it and it will start
        // executing instructions.
        this->fetchen_sync((vp::Block *)this, this->iss.top.get_js_config()->get("fetch_enable")->get_bool());
    }
}



void Exec::icache_flush()
{
    if (this->flush_cache_req_itf.is_bound())
    {
        this->cache_sync = true;
        this->insn_stall();
        this->flush_cache_req_itf.sync(true);
    }

    // Delay the flush to the next instruction in case we are in the middle of an instruction
    this->pending_flush = true;
    this->switch_to_full_mode();
}

#include <unistd.h>

void Exec::dbg_unit_step_check()
{
    if (this->iss.exec.step_mode.get() && !this->iss.exec.debug_mode)
    {
        this->iss.dbgunit.do_step.set(false);
        this->iss.dbgunit.hit_reg |= 1;
        if (this->iss.gdbserver.gdbserver)
        {
            this->iss.exec.stalled_inc();
            this->iss.exec.step_mode.set(false);
            this->iss.exec.halted.set(true);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, 5);
        }
        else
        {
            this->iss.dbgunit.set_halt_mode(true, HALT_CAUSE_STEP);
        }
    }
}



void Exec::exec_instr(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *const iss = (Iss *)__this;

    if (iss->exec.handle_stall_cycles()) return;

    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction with fast handler\n");

    iss_reg_t pc = iss->exec.current_insn;

#if defined(CONFIG_GVSOC_ISS_TIMED)
    if (iss->prefetcher.fetch(pc))
#endif
    {
        iss_reg_t index;
        iss_insn_t *insn = iss->insn_cache.get_insn(pc, index);
        if (insn == NULL) return;

        // Takes care first of all optional features (traces, VCD and so on)
        iss->exec.insn_exec_profiling();

        // Execute the instruction and replace the current one with the new one
        iss->exec.current_insn = insn->fast_handler(iss, insn, pc);

        // Since power instruction information is filled when the instruction is decoded,
        // make sure we account it only after the instruction is executed
        iss->exec.insn_exec_power(insn);
    }
}


#if defined(CONFIG_GVSOC_ISS_RI5KY)

// TODO HW loop methods could be moved to ri5cy specific code by using inheritance
void Exec::hwloop_set_start(int index, iss_reg_t pc)
{
    this->hwloop_start_insn[index] = pc;
}



void Exec::hwloop_stub_insert(iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->hwloop_handler == NULL)
    {
        insn->hwloop_handler = insn->handler;

#ifdef CONFIG_GVSOC_ISS_RI5KY
        insn->handler = hwloop_check_exec;
        insn->fast_handler = hwloop_check_exec;
#endif
    }
}



void Exec::hwloop_set_end(int index, iss_reg_t pc)
{
    this->hwloop_end_insn[index] = pc;

    iss_reg_t cache_index;
    iss_insn_t *insn = this->iss.insn_cache.get_insn(pc, cache_index);

    if (insn != NULL && this->iss.insn_cache.insn_is_decoded(insn))
    {
        this->hwloop_stub_insert(insn, pc);
    }
}

#endif



void Exec::decode_insn(iss_insn_t *insn, iss_addr_t pc)
{
#if defined(CONFIG_GVSOC_ISS_RI5KY)
    for (int i=0; i<CONFIG_GVSOC_ISS_NB_HWLOOP; i++)
    {
        if (this->hwloop_end_insn[i] == pc)
        {
            this->hwloop_stub_insert(insn, pc);
            break;
        }
    }
#endif
}



void Exec::exec_instr_check_all(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *iss = (Iss *)__this;
    Exec *_this = &iss->exec;

    if (iss->exec.handle_stall_cycles()) return;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction with slow handler (pc: 0x%lx)\n", iss->exec.current_insn);

    if(_this->pending_flush)
    {
        iss->insn_cache.flush();
        _this->pending_flush = false;
    }

    if (_this->has_exception)
    {
        _this->current_insn = _this->exception_pc;
        _this->has_exception = false;
    }

    // Switch back to optimize instruction handler only
    // if HW counters are disabled as they are checked with the slow handler
    if (_this->can_switch_to_fast_mode())
    {
        _this->instr_event.set_callback(&Exec::exec_instr);
    }

    _this->insn_exec_profiling();

    if (!_this->skip_irq_check)
    {
        _this->iss.irq.check();
    }
    else
    {
        _this->skip_irq_check = false;
    }

    iss_reg_t pc = iss->exec.current_insn;

#if defined(CONFIG_GVSOC_ISS_TIMED)
    if (iss->prefetcher.fetch(pc))
#endif
    {
        iss_reg_t index;
        iss_insn_t *insn = iss->insn_cache.get_insn(pc, index);
        if (insn == NULL) return;

        // _this->trace.msg(vp::Trace::LEVEL_TRACE, "Search for register value: 0x%lx\n", iss->regfile.get_reg(2));

        _this->current_insn = _this->insn_exec(insn, pc);
        #ifdef CONFIG_GVSOC_ISS_SNITCH
        // Decode again if the same instruction is executed the next time.
        // Snitch integer instructions always need to check data dependency in memory and register file before execution.
        // Reset the handler function to ensure functionally correct, but sacrifice simulation speed.
        insn->handler = iss_decode_pc_handler;
        insn->fast_handler = iss_decode_pc_handler;
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Next pc: 0x%lx\n", iss->exec.current_insn);
        #endif

        _this->iss.timing.insn_account();

        _this->insn_exec_power(insn);

        _this->dbg_unit_step_check();
    }
}



void Exec::clock_sync(vp::Block *__this, bool active)
{
    Exec *_this = (Exec *)__this;
    _this->trace.msg("Setting clock (active: %d)\n", active);

    _this->clock_active = active;
}



void Exec::fetchen_sync(vp::Block *__this, bool active)
{
    Exec *_this = (Exec *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting fetch enable (active: %d)\n", active);
    int old_val = _this->fetch_enable_reg.get();
    _this->fetch_enable_reg.set(active);

    if (!old_val && active)
    {
        _this->stalled_dec();
        _this->pc_set(_this->bootaddr_reg.get() + _this->bootaddr_offset);
        _this->busy_enter();
    }
    else if (old_val && !active)
    {
        // In case of a falling edge, stall the core to prevent him from executing
        _this->stalled_inc();
        _this->busy_exit();
    }
}



void Exec::bootaddr_sync(vp::Block *__this, uint32_t value)
{
    Exec *_this = (Exec *)__this;
    _this->trace.msg("Setting boot address (value: 0x%x)\n", value);
    _this->bootaddr_reg.set(value);
    iss_reg_t bootaddr = _this->bootaddr_reg.get() & ~((1 << 8) - 1);
    _this->iss.csr.mtvec.access(true, bootaddr);
}



void Exec::pc_set(iss_addr_t value)
{
    this->current_insn = value;
}


void Exec::offload_grant(vp::Block *__this, IssOffloadInsnGrant<iss_reg_t> *result)
{
    Exec *_this = (Exec *)__this;
    _this->iss.regfile.set_reg(_this->stall_reg, result->result);
    _this->stalled_dec();
    _this->insn_terminate();
}

void Exec::flush_cache_ack_sync(vp::Block *__this, bool active)
{
    Exec *_this = (Exec *)__this;
    if (_this->iss.exec.cache_sync)
    {
        _this->iss.exec.cache_sync = false;
        _this->stalled_dec();
        _this->insn_terminate();
    }
}
