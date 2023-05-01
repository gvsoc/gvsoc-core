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
    this->event = this->iss.top.event_new(this, &Gdbserver::handle_pending_io_access_stub);
    this->io_itf.set_resp_meth(&Gdbserver::data_response);
    this->iss.top.new_master_port(this, "data_debug", &this->io_itf);
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
    return this->id;
}

void Gdbserver::gdbserver_set_id(int i)
{
    this->id = i;
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
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Getting registers\n");
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

    this->iss.exec.skip_irq_check = true;
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

static inline iss_reg_t breakpoint_check_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->exec.stalled_inc();
    iss->exec.halted.set(true);
    iss->gdbserver.gdbserver->signal(&iss->gdbserver, vp::Gdbserver_engine::SIGNAL_TRAP, "hwbreak");
    return pc;
}

void Gdbserver::enable_breakpoint(iss_addr_t addr)
{
    // TODO the information stored in the instruction could disappear in case
    // of cache flush. The decode function should also check if they are active breakpoints
    // for the instruction being decoded
    iss_insn_t *insn = insn_cache_get(&this->iss, addr);

    if (insn_cache_is_decoded(&this->iss, insn))
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
    if (insn_cache_is_decoded(&this->iss, insn))
    {
        insn->handler = insn->breakpoint_saved_handler;
        insn->fast_handler = insn->breakpoint_saved_fast_handler;
        insn->breakpoint_saved_handler = NULL;
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

bool Gdbserver::watchpoint_check(bool is_write, iss_addr_t addr, int size)
{
    std::list<Watchpoint *> &watchpoints = is_write ? this->write_watchpoints : this->read_watchpoints;
    for (auto wp: watchpoints)
    {
        if (addr + size > wp->addr && addr < wp->addr + wp->size)
        {
            this->trace.msg(vp::trace::LEVEL_DEBUG, "Hit watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
                addr, size, is_write);
            this->iss.exec.stalled_inc();
            this->iss.exec.halted.set(true);
            uint32_t hit_addr = std::max(wp->addr, addr);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver,
                vp::Gdbserver_engine::SIGNAL_TRAP, is_write ? "watch" : "rwatch", hit_addr);
            return true;
        }
    }
    return false;
}

void Gdbserver::gdbserver_watchpoint_insert(bool is_write, uint64_t addr, int size)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Inserting watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
        addr, size, is_write);

    std::list<Watchpoint *> &watchpoints = is_write ? this->write_watchpoints : this->read_watchpoints;
    watchpoints.push_back(new Watchpoint(addr, size));
}

void Gdbserver::gdbserver_watchpoint_remove(bool is_write, uint64_t addr, int size)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Removing watchpoint (addr: 0x%x, size: 0x%x, is_write: %d)\n",
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

void Gdbserver::handle_pending_io_access_stub(void *__this, vp::clock_event *event)
{
    // Just forward to the common handle so that it either continue the full request or notify
    // the end
    Gdbserver *_this = (Gdbserver *)__this;
    _this->handle_pending_io_access();
}

void Gdbserver::data_response(void *__this, vp::io_req *req)
{
    // Just forward to the common handle so that it either continue the full request or notify
    // the end
    Gdbserver *_this = (Gdbserver *)__this;
    _this->handle_pending_io_access();
}

void Gdbserver::handle_pending_io_access()
{
    if (this->io_pending_size > 0)
    {
        vp::io_req *req = &this->io_req;

        // Compute the size of the request since the core can only do aligned accesses of its
        // register width.
        iss_addr_t addr = this->io_pending_addr;
        iss_addr_t addr_aligned = addr & ~(ISS_REG_WIDTH / 8 - 1);
        int size = addr_aligned + ISS_REG_WIDTH/8 - addr;
        if (size > this->io_pending_size)
        {
            size = this->io_pending_size;
        }

        this->trace.msg(vp::trace::LEVEL_DEBUG, "Sending request to interface (addr: 0x%lx, size: 0x%x, is_write: %d)\n",
            addr, size, this->io_pending_is_write);

        // Initialize the request
        req->init();
        req->set_addr(addr);
        req->set_size(size);
        req->set_is_write(this->io_pending_is_write);
        req->set_data(this->io_pending_data);

        // Update the full request for the next iteration
        this->io_pending_data += size;
        this->io_pending_size -= size;
        this->io_pending_addr += size;

        // Send the request to the interface
        int err = this->io_itf.req(req);
        if (err == vp::IO_REQ_OK)
        {
            // Always handle it through an event to simplify since we should make sure
            // we don't enqueue another request before the latency of this one is over.
            this->event->enqueue(this->io_req.get_latency() + 1);
        }
        else if (err == vp::IO_REQ_INVALID)
        {
            // Stop here if we got an error
            this->trace.msg(vp::trace::LEVEL_DEBUG, "End of data request\n");
            std::unique_lock<std::mutex> lock(this->mutex);
            this->waiting_io_response = false;
            this->io_retval = 1;
            this->cond.notify_all();
            lock.unlock();
        }
        else
        {
            // Nothing today for asynchronous reply since the callback will take care of continuing
            // the whole access
        }
    }
    else
    {
        // We reached the end of the whole request, notify the waiting thread
        this->trace.msg(vp::trace::LEVEL_DEBUG, "End of data request\n");
        std::unique_lock<std::mutex> lock(this->mutex);
        this->waiting_io_response = false;
        this->io_retval = 0;
        this->cond.notify_all();
        lock.unlock();
    }
}

int Gdbserver::gdbserver_io_access(uint64_t addr, int size, uint8_t *data, bool is_write)
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

    // Since the whole request may need to be processed in several small requests,
    // we setup an internal FSM that will inform us when the whole request is over.
    this->io_pending_addr = addr;
    this->io_pending_size = size;
    this->io_pending_data = data;
    this->io_pending_is_write = is_write;
    this->waiting_io_response = true;

    // Trigger the first access, with engine locked, since we come from an external thread
    this->iss.top.get_time_engine()->lock();
    this->handle_pending_io_access();
    this->iss.top.get_time_engine()->unlock();

    // Then wait until the FSM is over
    std::unique_lock<std::mutex> lock(this->mutex);
    while (this->waiting_io_response)
    {
        this->cond.wait(lock);
    }
    lock.unlock();

    return this->io_retval;
}