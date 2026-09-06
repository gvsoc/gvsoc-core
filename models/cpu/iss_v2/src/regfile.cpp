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

#include <vp/memcheck.hpp>

Regfile::Regfile(Iss &iss)
: iss(iss)
{
    this->iss.traces.new_trace("regfile", &this->trace, vp::DEBUG);

    this->regs[0] = 0;

#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
    this->iss.traces.new_trace_event_real("stack_usage", &this->stack_usage_event);
    this->stack_enabled = false;
    this->stack_active = false;
#endif

#if defined(CONFIG_GVSOC_EVENT_ACTIVE)
    this->reg_signals.reserve(ISS_NB_REGS + ISS_NB_FREGS);
    for (int i = 0; i < ISS_NB_REGS + ISS_NB_FREGS; i++)
    {
        this->reg_signals.emplace_back(
            this->iss,
            "regfile/x" + std::to_string(i),
            sizeof(iss_reg_t) * 8,
            vp::SignalCommon::ResetKind::HighZ
        );
    }
#endif
}


#ifdef VP_MEMCHECK_ACTIVE
// Arm the memory checker: every register is uninitialized with no buffer
// attached, except x0 and the dummy register which are always valid. Called at
// core reset, and again when the runtime declares the application is starting
// (semihosting 0x11B), so that what the boot ROM left behind is not taken for
// initialized data.
void Regfile::memcheck_reset()
{
    for (int i = 0; i < ISS_NB_REGS + ISS_NB_FREGS + 1; i++)
    {
        this->regs_memcheck[i] = 0;
        this->regs_memcheck_id[i] = 0;
    }
    this->regs_memcheck[0] = (iss_reg_t)-1;
    this->regs_memcheck[ISS_DUMMY_REG] = (iss_reg_t)-1;
    this->memcheck_reg_fault = false;
}
#endif


void Regfile::reset(bool active)
{
    if (active)
    {
        for (int i = 1; i < ISS_NB_REGS + ISS_NB_FREGS; i++)
        {
            this->regs[i] = (iss_reg_t)0x5757575757575757;
        }

#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
        this->stack_enabled = false;
        this->stack_active = false;
#endif

#ifdef VP_MEMCHECK_ACTIVE
        this->memcheck_reset();
#endif

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
        this->sb_reg_invalid = 0;
        this->sb_reason_set_mask = 0;
        for (int i = 0; i < 64; i++)
        {
            this->sb_reason[i] = 0;
        }
        this->sb_last_stall_pc    = (iss_addr_t)-1;
        this->sb_last_stall_cycle = -2;
#endif
    }
}

#if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)

void Regfile::stack_set(iss_reg_t base, iss_reg_t size)
{
    this->stack_enabled = size != 0;
    this->stack_active = false;
    this->stack_base = base;
    this->stack_top = base + size;

    if (this->stack_enabled)
    {
        this->trace.msg(vp::Trace::LEVEL_INFO,
            "Declared stack (base: 0x%" PRIxFULLREG ", size: 0x%" PRIxREG ")\n", base, size);
        // If SP is already inside the declared stack (boot / cluster fork
        // case), this activates the checks immediately
        this->stack_sp_update(this->regs[2]);
    }
}

void Regfile::stack_sp_update(iss_reg_t sp)
{
    if (!this->stack_enabled)
    {
        return;
    }

    bool outside = sp < this->stack_base || sp > this->stack_top;

    if (!this->stack_active)
    {
        if (outside)
        {
            return;
        }
        this->stack_active = true;
    }
    else if (outside)
    {
        this->stack_fault_report(sp);
        // Disarm so execution can be resumed from a front-end without
        // re-faulting on every SP write; the checks re-arm once SP comes back
        // inside the declared stack
        this->stack_active = false;
        return;
    }

    this->stack_usage_event.event_real(this->stack_top - sp);
}

void Regfile::stack_fault_report(iss_reg_t sp)
{
    char message[256];
    snprintf(message, sizeof(message), "SP set outside declared stack "
        "(sp: 0x%" PRIxFULLREG ", stack: [0x%" PRIxFULLREG ", 0x%" PRIxFULLREG
        "], pc: 0x%" PRIxFULLREG ")",
        sp, this->stack_base, this->stack_top,
        (iss_reg_t)this->iss.exec.current_insn);

    // Fill the structured fault record so front-ends (GUI, console) can render
    // a report and navigate to the fault, like the memcheck faults
    vp::MemCheck *mc = this->iss.get_memcheck();
    mc->fault.valid = true;
    mc->fault.time = this->iss.time.get_time();
    mc->fault.core = this->iss.get_path();
    mc->fault.pc = this->iss.exec.current_insn;
    mc->fault.addr = sp;
    mc->fault.size = 0;
    mc->fault.is_write = true;
    mc->fault.kind = "stack-overflow";
    mc->fault.buffer_id = 0;
    mc->fault.message = message;

    if (this->iss.gdbserver.is_enabled())
    {
        // Halt the core and notify GDB with a bus error so the fault can be
        // inspected and execution resumed, like the memcheck faults
        this->trace.force_warning_no_error("%s\n", message);
        if (!this->iss.exec.halted.get())
        {
            this->iss.exec.retain_inc();
            this->iss.exec.halted.set(true);
        }
        this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver,
            vp::Gdbserver_engine::SIGNAL_BUS);
    }
    else if (mc->fault_stop())
    {
        // A front-end is attached (GUI or console), the simulation pauses on
        // the fault like on a watchpoint hit; execution can be resumed there
        this->trace.force_warning_no_error("%s\n", message);
    }
    else
    {
        // Batch mode: report and apply the werror policy
        this->trace.force_warning("%s\n", message);
    }
}

#endif
