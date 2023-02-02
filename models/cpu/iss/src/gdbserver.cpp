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

#include <iss.hpp>

Gdbserver::Gdbserver(Iss &iss)
    : iss(iss)
{
}


void Gdbserver::build()
{
    this->iss.top.traces.new_trace("gdbserver", &this->trace, vp::DEBUG);
}

void Gdbserver::start()
{
    this->gdbserver = (vp::Gdbserver_engine *)this->iss.top.get_service("gdbserver");

    if (this->gdbserver)
    {
        this->gdbserver->register_core(this);
    }

    this->halt_on_reset = this->gdbserver;
}

void Gdbserver::reset(bool active)
{
    // The platform is started with all cores halted so that they start only when gdb is connected.
    if (this->gdbserver && !active && this->halt_on_reset)
    {
        // Some cores are still under reset when gdb is connected.
        // Since the stall counter is set to 0 on reset, just remember to stall it when reset is
        // deasserted.
        if (!this->iss.core.reset_get())
        {
            this->iss.exec.stalled_inc();
        }
        this->iss.exec.halted.set(true);
    }
}

int Gdbserver::gdbserver_get_id()
{
    return this->iss.csr.mhartid;
}

std::string Gdbserver::gdbserver_get_name()
{
    return this->iss.top.get_name();
}

int Gdbserver::gdbserver_reg_set(int reg, uint8_t *value)
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Setting register from gdbserver (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);

    if (reg == 32)
    {
        this->iss.exec.pc_set(*(uint32_t *)value);
    }
    else
    {
        this->trace.msg(vp::trace::LEVEL_ERROR, "Setting invalid register (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);
    }

    return 0;
}

int Gdbserver::gdbserver_reg_get(int reg, uint8_t *value)
{
    fprintf(stderr, "UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
    return 0;
}

int Gdbserver::gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value)
{
    if (nb_regs)
    {
        *nb_regs = 33;
    }

    if (reg_size)
    {
        *reg_size = 4;
    }

    if (value)
    {
        uint32_t *regs = (uint32_t *)value;
        for (int i = 0; i < 32; i++)
        {
            regs[i] = this->iss.regfile.get_reg(i);
        }

        if (this->iss.exec.current_insn)
        {
            regs[32] = this->iss.exec.current_insn->addr;
        }
        else
        {
            regs[32] = 0;
        }
    }

    return 0;
}

int Gdbserver::gdbserver_stop()
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Received stop request\n");

    if (!this->iss.exec.halted.get())
    {
        this->iss.exec.stalled_inc();
        this->iss.exec.halted.set(true);
    }
    this->gdbserver->signal(this, vp::Gdbserver_engine::SIGNAL_STOP);
    return 0;
}

int Gdbserver::gdbserver_cont()
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Received cont request\n");

    this->halt_on_reset = false;

    if (this->iss.exec.halted.get())
    {
        this->iss.exec.stalled_dec();
        this->iss.exec.halted.set(false);
    }

    return 0;
}

int Gdbserver::gdbserver_stepi()
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Received stepi request\n");

    this->halt_on_reset = false;

    this->iss.exec.step_mode.set(true);
    this->iss.exec.switch_to_full_mode();
    if (this->iss.exec.halted.get())
    {
        this->iss.exec.stalled_dec();
        this->iss.exec.halted.set(false);
    }
    return 0;
}

int Gdbserver::gdbserver_state()
{
    return this->iss.exec.halted.get() ? vp::Gdbserver_core::state::stopped : vp::Gdbserver_core::state::running;
}

static inline iss_insn_t *breakpoint_check_exec(Iss *iss, iss_insn_t *insn)
{
    iss->exec.stalled_inc();
    iss->exec.halted.set(true);
    iss->gdbserver.gdbserver->signal(&iss->gdbserver, vp::Gdbserver_engine::SIGNAL_TRAP);
    return insn;
}

void Gdbserver::enable_breakpoint(iss_addr_t addr)
{
    iss_insn_t *insn = insn_cache_get(&this->iss, addr);

    if (insn->fetched)
    {
        insn->breakpoint_saved_handler = insn->handler;
        insn->breakpoint_saved_fast_handler = insn->fast_handler;
        insn->handler = breakpoint_check_exec;
        insn->fast_handler = breakpoint_check_exec;
    }
    else
    {
        insn->breakpoint_saved_handler = breakpoint_check_exec;
    }
}

void Gdbserver::disable_breakpoint(iss_addr_t addr)
{
    iss_insn_t *insn = insn_cache_get(&this->iss, addr);

    insn->handler = insn->breakpoint_saved_handler;
    insn->fast_handler = insn->breakpoint_saved_fast_handler;
    insn->breakpoint_saved_handler = NULL;
}

void Gdbserver::enable_all_breakpoints()
{
    for (auto x: this->breakpoints)
    {
        this->enable_breakpoint(x);
    }
}

void Gdbserver::gdbserver_breakpoint_insert(uint64_t addr)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Inserting breakpoint (addr: 0x%x)\n", addr);

    this->breakpoints.push_back((iss_addr_t)addr);

    this->enable_breakpoint((iss_addr_t)addr);
}

void Gdbserver::gdbserver_breakpoint_remove(uint64_t addr)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Removing breakpoint (addr: 0x%x)\n", addr);

    this->breakpoints.remove(addr);

    this->disable_breakpoint((iss_addr_t)addr);
}