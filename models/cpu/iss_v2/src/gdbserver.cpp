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

#include <algorithm>
#include <vp/controller.hpp>



Gdbserver::Gdbserver(Iss &iss)
    : iss(iss)
{
    this->iss.traces.new_trace("gdbserver", &this->trace, vp::DEBUG);
}



void Gdbserver::start()
{
    // Locate the backdoor through the component the LSU data port talks to,
    // so that targets get debug memory accesses without any dedicated wiring.
    // This is a read-only walk of the binding graph; no request is ever
    // issued on the data port, all accesses go through the out-of-band
    // debug_mem interface.
    std::vector<vp::SlavePort *> finals = this->iss.lsu.data.get_final_ports();
    if (!finals.empty() && finals[0]->get_owner() != nullptr)
    {
        this->debug_mem = finals[0]->get_owner()->debug_mem_if();
    }

    this->gdbserver = (vp::Gdbserver_engine *)this->iss.get_service("gdbserver");

    if (this->gdbserver)
    {
        this->gdbserver->register_core(this);

        if (this->debug_mem == nullptr)
        {
            this->trace.msg(vp::Trace::LEVEL_WARNING,
                "No debug-memory backdoor behind LSU data port, gdb memory accesses will fail\n");
        }
    }

    this->halt_on_reset = this->gdbserver;
}

void Gdbserver::reset(bool active)
{
    if (active)
    {
        // The platform is started with all cores halted so that they start only when gdb is connected.
        if (this->gdbserver && this->halt_on_reset)
        {
            this->iss.exec.halted.set(true);
        }

        // In case gdb got connected while we were under reset, we need to stall the core
        // before releasing the reset
        if (this->iss.exec.halted.get())
        {
            this->iss.exec.retain_inc();
        }
    }

}



int Gdbserver::gdbserver_get_id()
{
    return this->id;
}



void Gdbserver::gdbserver_set_id(int i)
{
    this->id = i;
}



std::string Gdbserver::gdbserver_get_name()
{
    return this->iss.get_name();
}



int Gdbserver::gdbserver_reg_set(int reg, uint8_t *value)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting register from gdbserver (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);

    if (reg < 32)
    {
        this->iss.regfile.set_reg(reg, *(iss_reg_t *)value);
    }
    else if (reg == 32)
    {
        this->iss.exec.pc_set(*(iss_addr_t *)value);
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting invalid register (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);
        return -1;
    }

    return 0;
}



int Gdbserver::gdbserver_reg_get(int reg, uint8_t *value)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Getting register from gdbserver (reg: %d)\n", reg);

    if (reg < 32)
    {
        *(iss_reg_t *)value = this->iss.regfile.get_reg_untimed(reg);
    }
    else if (reg == 32)
    {
        *(iss_reg_t *)value = this->iss.exec.current_insn;
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Getting invalid register (reg: %d)\n", reg);
        return -1;
    }

    return 0;
}



int Gdbserver::gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Getting registers\n");
    if (nb_regs)
    {
        *nb_regs = 33;
    }

    if (reg_size)
    {
        *reg_size = sizeof(iss_reg_t);
    }

    if (value)
    {
        iss_reg_t *regs = (iss_reg_t *)value;
        for (int i = 0; i < 32; i++)
        {
            regs[i] = this->iss.regfile.get_reg_untimed(i);
        }

        if (this->iss.exec.current_insn)
        {
            regs[32] = this->iss.exec.current_insn;
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
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received stop request\n");

    if (!this->iss.exec.halted.get())
    {
        this->iss.exec.retain_inc();
        this->iss.exec.halted.set(true);
    }
    this->gdbserver->signal(this, vp::Gdbserver_engine::SIGNAL_STOP);
    return 0;
}



int Gdbserver::gdbserver_cont()
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received cont request\n");

    this->halt_on_reset = false;

    if (this->iss.exec.halted.get())
    {
        this->iss.exec.retain_dec();
        this->iss.exec.halted.set(false);
    }

    return 0;
}



int Gdbserver::gdbserver_stepi()
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received stepi request\n");

    this->halt_on_reset = false;

    this->iss.exec.skip_irq_check = true;
    this->iss.exec.step_mode.set(true);
    this->iss.exec.switch_to_full_mode();
    if (this->iss.exec.halted.get())
    {
        this->iss.exec.retain_dec();
        this->iss.exec.halted.set(false);
    }
    return 0;
}



int Gdbserver::gdbserver_state()
{
    return this->iss.exec.halted.get() ? vp::Gdbserver_core::state::stopped : vp::Gdbserver_core::state::running;
}



static inline iss_reg_t breakpoint_check_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (std::count(insn->breakpoints.begin(), insn->breakpoints.end(), pc) > 0)
    {
        iss->exec.retain_inc();
        iss->exec.halted.set(true);
        if (iss->gdbserver.is_enabled())
        {
            iss->gdbserver.gdbserver->signal(&iss->gdbserver, vp::Gdbserver_engine::SIGNAL_TRAP, "hwbreak");
        }
        else
        {
            iss->gdbserver.breakpoint_hit = true;
            iss->gdbserver.breakpoint_hit_addr = pc;
        }
        return pc;
    }
    else
    {
        return iss->exec.insn_exec(insn, pc);
    }
}



void Gdbserver::breakpoint_stub_insert(iss_insn_t *insn, iss_reg_t pc)
{
    if (insn->breakpoints.size() == 0)
    {
        insn->breakpoint_saved_handler = insn->handler;
        insn->breakpoint_saved_fast_handler = insn->fast_handler;
        insn->handler = breakpoint_check_exec;
        insn->fast_handler = breakpoint_check_exec;
    }

    insn->breakpoints.push_back(pc);
}



void Gdbserver::breakpoint_stub_remove(iss_insn_t *insn, iss_reg_t pc)
{
    insn->breakpoints.erase(std::remove(insn->breakpoints.begin(), insn->breakpoints.end(), pc), insn->breakpoints.end());

    if (insn->breakpoints.size() == 0)
    {
        insn->handler = insn->breakpoint_saved_handler;
        insn->fast_handler = insn->breakpoint_saved_fast_handler;
    }
}



bool Gdbserver::breakpoint_check_pc(iss_addr_t pc)
{
    for (auto x: this->breakpoints)
    {
        if (x == pc)
        {
            return true;
        }
    }

    return false;
}



void Gdbserver::decode_insn(iss_insn_t *insn, iss_addr_t pc)
{
    if (this->breakpoint_check_pc(pc))
    {
        this->breakpoint_stub_insert(insn, pc);
    }
}



void Gdbserver::enable_breakpoint(iss_addr_t addr)
{
    // Enabling the breakpoint is done by inserting a stub as the instruction handler
    // so that it checks the breakpoint and call the real handler.
    // If the instruction is already decoded, we have to insert the stub, otherwise,
    // the decoder will call us when the instruction is decoded to add it.
    // If the cache returns NULL, it means it is currently translating the virtual address,
    // which means it is not decoded yet.
    iss_insn_t *insn = this->iss.insn_cache.get_insn(addr);

    if (insn != NULL && this->iss.decode.is_decoded(insn))
    {
        this->breakpoint_stub_insert(insn, addr);
    }
}



void Gdbserver::disable_breakpoint(iss_addr_t addr)
{
    iss_insn_t *insn = this->iss.insn_cache.get_insn(addr);
    if (this->iss.decode.is_decoded(insn))
    {
        this->breakpoint_stub_remove(insn, addr);
    }
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
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Inserting breakpoint (addr: 0x%x)\n", addr);

    this->breakpoints.push_back((iss_addr_t)addr);

    this->enable_breakpoint((iss_addr_t)addr);
}



void Gdbserver::gdbserver_breakpoint_remove(uint64_t addr)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Removing breakpoint (addr: 0x%x)\n", addr);

    this->breakpoints.remove(addr);

    this->disable_breakpoint((iss_addr_t)addr);
}



bool Gdbserver::watchpoint_check(bool is_write, iss_addr_t addr, int size)
{
    std::list<Watchpoint *> &watchpoints = is_write ? this->write_watchpoints : this->read_watchpoints;
    for (auto wp: watchpoints)
    {
        if (addr + size > wp->addr && addr < wp->addr + wp->size)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Hit watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
                addr, size, is_write);
            this->iss.exec.retain_inc();
            this->iss.exec.halted.set(true);
            iss_reg_t hit_addr = std::max(wp->addr, addr);
            if (this->is_enabled())
            {
                this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver,
                    vp::Gdbserver_engine::SIGNAL_TRAP, is_write ? "watch" : "rwatch", hit_addr);
            }
            else
            {
                this->watchpoint_hit = true;
                this->watchpoint_hit_addr = hit_addr;
                this->watchpoint_hit_is_write = is_write;
            }
            return true;
        }
    }
    return false;
}



void Gdbserver::gdbserver_watchpoint_insert(bool is_write, uint64_t addr, int size)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Inserting watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
        addr, size, is_write);

    std::list<Watchpoint *> &watchpoints = is_write ? this->write_watchpoints : this->read_watchpoints;
    watchpoints.push_back(new Watchpoint(addr, size));
}



void Gdbserver::gdbserver_watchpoint_remove(bool is_write, uint64_t addr, int size)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Removing watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
        addr, size, is_write);

    std::list<Watchpoint *> &watchpoints = is_write ? this->write_watchpoints : this->read_watchpoints;

    auto it = watchpoints.begin();
    while (it != watchpoints.end())
    {
        if (addr + size >= (*it)->addr && addr < (*it)->addr + (*it)->size)
        {
            it = watchpoints.erase(it);
        }
        else
        {
            ++it;
        }
    }
}



int Gdbserver::gdbserver_io_access(uint64_t addr, int size, uint8_t *data, bool is_write)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

    if (this->debug_mem == nullptr)
    {
        this->trace.force_warning(
            "Debug memory access without debug-memory backdoor (addr: 0x%lx, size: 0x%x, is_write: %d)\n",
            addr, size, is_write);
        return 1;
    }

    // The access completes inline under the engine lock we already hold,
    // with no timing and no simulation advance.
    return this->debug_mem->debug_mem_access(addr, data, size, is_write) ? 1 : 0;
}
