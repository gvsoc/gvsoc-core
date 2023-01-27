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

    if (this->iss.csr.mhartid == 9)
    if (this->gdbserver)
    {
        this->gdbserver->register_core(this);
    }
}

void Gdbserver::reset(bool active)
{
    if (this->gdbserver && !active)
    {
        this->iss.exec.stalled_inc();
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
    this->iss.top.get_time_engine()->lock();
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Setting register from gdbserver (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);

    if (reg == 32)
    {
        this->iss.exec.pc_set(*(uint32_t *)value);
    }
    else
    {
        this->trace.msg(vp::trace::LEVEL_ERROR, "Setting invalid register (reg: %d, value: 0x%x)\n", reg, *(uint32_t *)value);
    }
    this->iss.top.get_time_engine()->unlock();

    return 0;
}

int Gdbserver::gdbserver_reg_get(int reg, uint8_t *value)
{
    fprintf(stderr, "UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
    return 0;
}

int Gdbserver::gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value)
{
    this->iss.top.get_time_engine()->lock();
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
    this->iss.top.get_time_engine()->unlock();

    return 0;
}

int Gdbserver::gdbserver_stop()
{
    this->iss.top.get_time_engine()->lock();
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Received stop request\n");

    if (!this->iss.exec.halted.get())
    {
        this->iss.exec.stalled_inc();
        this->iss.exec.halted.set(true);
    }
    this->gdbserver->signal(this);
    this->iss.top.get_time_engine()->unlock();
    return 0;
}

int Gdbserver::gdbserver_cont()
{
    this->iss.top.get_time_engine()->lock();
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Received cont request\n");

    if (this->iss.exec.halted.get())
    {
        this->iss.exec.stalled_dec();
        this->iss.exec.halted.set(false);
    }
    this->iss.top.get_time_engine()->unlock();

    return 0;
}

int Gdbserver::gdbserver_stepi()
{
    this->iss.top.get_time_engine()->lock();
    this->iss.exec.step_mode.set(true);
    this->iss.exec.switch_to_full_mode();
    this->iss.top.get_time_engine()->unlock();
    return 0;
}

int Gdbserver::gdbserver_state()
{
    return vp::Gdbserver_core::state::running;
}
