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
#include <cpu/iss_v2/include/iss.hpp>



ExecInOrder::ExecInOrder(Iss &iss)
    : iss(iss), instr_event(&iss, &ExecInOrder::exec_instr_check_all)
{
    this->iss.traces.new_trace("exec", &this->trace, vp::DEBUG);

    this->iss.new_master_port("busy", &busy_itf);

    flush_cache_ack_itf.set_sync_meth(&ExecInOrder::flush_cache_ack_sync);
    this->iss.new_slave_port("flush_cache_ack", &flush_cache_ack_itf, (vp::Block *)this);
    this->iss.new_master_port("flush_cache_req", &flush_cache_req_itf);

    bootaddr_itf.set_sync_meth(&ExecInOrder::bootaddr_sync);
    this->iss.new_slave_port("bootaddr", &bootaddr_itf, (vp::Block *)this);

    clock_itf.set_sync_meth(&ExecInOrder::clock_sync);
    this->iss.new_slave_port("clock_en", &clock_itf, (vp::Block *)this);

    fetchen_itf.set_sync_meth(&ExecInOrder::fetchen_sync);
    this->iss.new_slave_port("fetchen", &fetchen_itf, (vp::Block *)this);

    this->iss.new_reg("step_mode", &this->step_mode, false);

    this->iss.new_reg("halted", &this->halted, false, false);

    this->iss.new_reg("busy", &this->busy, 1);

    this->iss.new_reg("bootaddr", &this->bootaddr_reg, this->iss.get_js_config()->get_child_int("boot_addr"));

    this->iss.new_reg("fetch_enable", &this->fetch_enable_reg, 0, true);
    this->iss.new_reg("retained", &this->retained, 0);
    this->iss.new_reg("wfi", &this->wfi, false);
    this->iss.new_reg("irq_enter", &this->irq_enter, false);
    this->iss.new_reg("irq_exit", &this->irq_exit, false);

    this->retained.set(false);
    this->halted.set(false);

    this->bootaddr_offset = this->iss.get_js_config()->get_child_int("bootaddr_offset");


    this->current_insn = 0;
    this->stall_insn = 0;
#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
    this->hwloop_end_insn[0] = 0;
    this->hwloop_end_insn[1] = 0;
#endif

    this->iss.traces.new_trace_event_string("label", &this->asm_trace_event);
}



void ExecInOrder::reset(bool active)
{
    if (active)
    {
        this->pending_flush = false;
        this->clock_active = false;
        this->skip_irq_check = true;
        this->has_exception = false;
        this->bootaddr_apply(this->bootaddr_reg.get());
        this->pc_set(this->bootaddr_reg.get() + this->bootaddr_offset);

        this->insn_table_index = 0;
        this->irq_locked = 0;
        this->insn_on_hold = false;
        this->stall_cycles = 0;
        this->cache_sync = false;
        this->is_insn_stalled = false;
        this->is_insn_hold = false;
        this->first_task = NULL;
        this->instr_disabled = false;

        // Always increase the stall when reset is asserted since stall count is set to 0
        // and we need to prevent the core from fetching instructions
        this->retain_inc();
    }
    else
    {
        this->elw_insn = 0;
#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
        this->hwloop_end_insn[0] = 0;
        this->hwloop_end_insn[1] = 0;
#endif

        // Check if the core should start fetching, if so this will unstall it and it will start
        // executing instructions.
        this->fetchen_sync((vp::Block *)this, this->iss.get_js_config()->get("fetch_enable")->get_bool());
    }
}



void ExecInOrder::icache_flush()
{
    // if (this->flush_cache_req_itf.is_bound())
    // {
    //     this->cache_sync = true;
    //     this->insn_stall();
    //     this->flush_cache_req_itf.sync(true);
    // }

    // // Delay the flush to the next instruction in case we are in the middle of an instruction
    // this->pending_flush = true;
    // this->switch_to_full_mode();
}

#include <unistd.h>

// void ExecInOrder::dbg_unit_step_check()
// {
//     if (this->iss.exec.step_mode.get() && !this->iss.exec.debug_mode)
//     {
//         this->iss.dbgunit.do_step.set(false);
//         this->iss.dbgunit.hit_reg |= 1;
//         if (this->iss.gdbserver.gdbserver)
//         {
//             this->iss.exec.stalled_inc();
//             this->iss.exec.step_mode.set(false);
//             this->iss.exec.halted.set(true);
//             this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, 5);
//         }
//         else
//         {
//             this->iss.dbgunit.set_halt_mode(true, HALT_CAUSE_STEP);
//         }
//     }
// }



void ExecInOrder::exec_instr(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *const iss = (Iss *)__this;

//     if (iss->exec.handle_stall_cycles()) return;

    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction with fast handler\n");

//     iss_reg_t pc = iss->exec.current_insn;

// #if defined(CONFIG_GVSOC_ISS_TIMED)
//     if (iss->prefetcher.fetch(pc))
// #endif
//     {
//         iss_reg_t index;
//         iss_insn_t *insn = iss->insn_cache.get_insn(pc, index);
//         if (insn == NULL) return;

//         #if defined(CONFIG_GVSOC_ISS_EXEC_SCOREBOARD)
//         // Since the instruction may be not decoded yet and will be once the handler is executed,
//         // we need to make sure it is decoded before checking arguments
//         if (!iss->insn_cache.insn_is_decoded(insn))
//         {
//             iss->decode.decode_pc(insn, insn->addr);
//         }

//         // Note that for now only LSU instructions are invalidating registers since the others
//         // are immediately executed. This would need to be extended if other instructions are
//         // asynchronous.
//         for (int i=0; i<insn->nb_in_reg; i++)
//         {
//             if (!insn->in_regs_vec[i])
//             {
//                 if (iss->regfile.scoreboard_reg_timestamp[insn->in_regs[i]] == -1)
//                 {
//                     iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to input register dependency (reg: %d)\n", insn->in_regs[i]);
//                     return;
//                 }
//             }
//         }
//         // Also stall on output registers so that they are written in same order
//         for (int i=0; i<insn->nb_out_reg; i++)
//         {
//             if (!insn->out_regs_vec[i])
//             {
//                 if (iss->regfile.scoreboard_reg_timestamp[insn->out_regs[i]] == -1)
//                 {
//                     iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to output register dependency (reg: %d)\n", insn->out_regs[i]);
//                     return;
//                 }
//             }
//         }

//         #endif

//         // Takes care first of all optional features (traces, VCD and so on)
//         iss->exec.insn_exec_profiling();

//         // Execute the instruction and replace the current one with the new one
//         iss->exec.current_insn = insn->fast_handler(iss, insn, pc);

//         iss->exec.asm_trace_event.event_string(insn->desc->label, false);

//         // Since power instruction information is filled when the instruction is decoded,
//         // make sure we account it only after the instruction is executed
//         iss->exec.insn_exec_power(insn);
//     }

//     // Check now register file access faults so that instruction is finished and properly displayed
//     iss->regfile.memcheck_fault();
}



void ExecInOrder::exec_instr_check_all(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *iss = (Iss *)__this;
    ExecInOrder *_this = &iss->exec;

//     if (iss->exec.handle_stall_cycles()) return;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction with slow handler (pc: 0x%lx)\n", iss->exec.current_insn);

//     if(_this->pending_flush)
//     {
//         iss->prefetcher.flush();
//         iss->insn_cache.flush();
//         _this->pending_flush = false;
//     }

//     if (_this->has_exception)
//     {
//         _this->current_insn = _this->exception_pc;
//         _this->has_exception = false;
//     }

//     // Switch back to optimize instruction handler only
//     // if HW counters are disabled as they are checked with the slow handler
//     if (_this->can_switch_to_fast_mode())
//     {
//         _this->instr_event.set_callback(&ExecInOrder::exec_instr);
//     }

//     _this->insn_exec_profiling();

//     if (!_this->skip_irq_check)
//     {
//         _this->iss.irq.check();
//     }
//     else
//     {
//         _this->skip_irq_check = false;
//     }

    Task *task = _this->first_task;
    if (task)
    {
        _this->first_task = NULL;

        do
        {
            task->callback(&_this->iss, task);
            task = task->next;
        } while (task);
    }

    iss_reg_t pc = iss->exec.current_insn;

#if defined(CONFIG_GVSOC_ISS_TIMED)
    if (iss->prefetch.fetch(pc))
#endif
    {
        iss_insn_t *insn = iss->insn_cache.get_insn(pc);
        if (insn == NULL) return;

        if (!iss->decode.is_decoded(insn))
        {
#if !defined(CONFIG_GVSOC_ISS_TIMED)
            if (!iss->prefetch.fetch(pc)) return;
#endif

            iss->decode.decode_pc(insn, insn->addr);
        }

        if (iss->regfile.scoreboard_insn_check(insn)) return;

        iss->regfile.scoreboard_insn_start(insn);

        if (iss->trace.insn_trace.get_active())
        {
            iss->trace.insn_trace_start(insn, pc);
        }

        iss_reg_t next_pc = _this->insn_exec(insn, pc);

        if (_this->is_insn_stalled)
        {
            _this->is_insn_stalled = false;
            iss->regfile.scoreboard_insn_clear(insn);
            return;
        }

        _this->current_insn = next_pc;

        if (!_this->is_insn_hold)
        {
            iss->regfile.scoreboard_insn_end(insn);

            if (iss->trace.insn_trace.get_active())
            {
                iss->trace.insn_trace_dump(insn, pc);
            }
        }

        _this->is_insn_hold = false;


//         _this->asm_trace_event.event_string(insn->desc->label, false);

//         _this->iss.timing.insn_account();

//         _this->insn_exec_power(insn);

//         _this->dbg_unit_step_check();
    }

//     // Check now register file access faults so that instruction is finished and properly displayed
//     iss->regfile.memcheck_fault();
}



void ExecInOrder::insn_terminate(InsnEntry *entry)
{
#ifdef VP_TRACE_ACTIVE
    if (this->iss.trace.insn_trace.get_active())
    {
        iss_insn_t *insn = this->iss.exec.get_insn(entry);
        this->iss.trace.insn_trace_dump(insn, insn->addr, entry->trace);
    }
#endif

    // Make output float registers available to unblock any scalar instructions stalled on them
    this->iss.regfile.scoreboard_insn_end(this->get_insn(entry));

    this->release_entry(entry);
    this->iss.timing.insn_stall_account();
}

void ExecInOrder::clock_sync(vp::Block *__this, bool active)
{
    ExecInOrder *_this = (ExecInOrder *)__this;
    _this->trace.msg("Setting clock (active: %d)\n", active);

    _this->clock_active = active;
}


void ExecInOrder::retain_check()
{
    if (this->retained.get() == 0)
    {
        if (this->instr_disabled)
        {
            this->instr_disabled = false;
            this->busy_enter();
            this->instr_event.enable();
        }
    }
    else
    {
        if (!this->instr_disabled && this->first_task == NULL)
        {
            this->instr_disabled = true;
            this->instr_event.disable();
            this->busy_exit();
        }
    }
}

void ExecInOrder::retain_inc()
{
    this->retained.inc(1);
    this->retain_check();
}

void ExecInOrder::retain_dec()
{
    this->retained.dec(1);
    this->retain_check();
}


void ExecInOrder::busy_enter()
{
    // uint8_t one = 1;
    // this->iss.timing.state_event.event(&one);
    this->busy.set(1);
    if (this->iss.exec.busy_itf.is_bound())
    {
        this->iss.exec.busy_itf.sync(1);
    }
}

void ExecInOrder::busy_exit()
{
    // this->iss.timing.state_event.event_highz();
    this->busy.release();
    if (this->iss.exec.busy_itf.is_bound())
    {
        this->iss.exec.busy_itf.sync(0);
    }
    // if (this->iss.timing.active_pc_trace_event.get_event_active())
    // {
    //     this->iss.timing.active_pc_trace_event.event_highz();
    // }
}

void ExecInOrder::fetchen_sync(vp::Block *__this, bool active)
{
    ExecInOrder *_this = (ExecInOrder *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting fetch enable (active: %d)\n", active);
    int old_val = _this->fetch_enable_reg.get();
    _this->fetch_enable_reg.set(active);

    if (!old_val && active)
    {
        _this->pc_set(_this->bootaddr_reg.get() + _this->bootaddr_offset);
        _this->retain_dec();
    }
    else if (old_val && !active)
    {
        // In case of a falling edge, stall the core to prevent him from executing
        _this->retain_inc();
    }
}


void ExecInOrder::bootaddr_apply(uint32_t value)
{
    this->trace.msg("Setting boot address (value: 0x%x)\n", value);
    iss_reg_t bootaddr = this->bootaddr_reg.get() & ~((1 << 8) - 1);
    // this->iss.csr.mtvec.access(true, bootaddr);

}


void ExecInOrder::bootaddr_sync(vp::Block *__this, uint32_t value)
{
    ExecInOrder *_this = (ExecInOrder *)__this;
    _this->bootaddr_reg.set(value);
    _this->bootaddr_apply(value);
}



void ExecInOrder::pc_set(iss_addr_t value)
{
    this->current_insn = value;
}


void ExecInOrder::flush_cache_ack_sync(vp::Block *__this, bool active)
{
    // ExecInOrder *_this = (ExecInOrder *)__this;
    // if (_this->iss.exec.cache_sync)
    // {
    //     _this->iss.exec.cache_sync = false;
    //     _this->stalled_dec();
    //     _this->insn_terminate();
    // }
}

InsnEntry *ExecInOrder::insn_hold(iss_insn_t *insn)
{
    this->is_insn_hold = true;
    InsnEntry *entry = this->get_entry();
    entry->addr = insn->addr;
    entry->opcode = insn->opcode;
#ifdef VP_TRACE_ACTIVE
    if (this->iss.trace.insn_trace.get_active())
    {
        entry->trace = this->iss.trace.detach_entry();
    }
#endif
    return entry;
}

iss_insn_t *ExecInOrder::get_insn(InsnEntry *entry)
{
    iss_insn_t *insn = this->iss.insn_cache.get_insn(entry->addr);
    if (!this->iss.decode.is_decoded(insn))
    {
        insn->opcode = entry->opcode;
        this->iss.decode.decode_pc(insn, entry->addr);
    }
    return insn;
}
