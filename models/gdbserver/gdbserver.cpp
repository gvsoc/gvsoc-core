/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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
 * Authors: Andreas Traber
 */

#include "gdbserver.hpp"
#include <stdarg.h>
#include "rsp.hpp"


Gdb_server::Gdb_server(js::config *config)
: vp::component(config)
{
    this->rsp = NULL;

    if (config->get_child_bool("enabled"))
    {
        this->rsp = new Rsp(this);
    }

    this->active_core = 9;
}


int Gdb_server::register_core(vp::Gdbserver_core *core)
{
    int id = core->gdbserver_get_id();

    this->trace.msg(vp::trace::LEVEL_INFO, "Registering core (id: %d)\n", id);

    this->cores[id] = core;
    this->cores_list.push_back(core);

    return 0;
}


void Gdb_server::signal(vp::Gdbserver_core *core, int signal)
{
    this->rsp->signal_from_core(core, signal);
}


void Gdb_server::pre_pre_build()
{
    if (this->get_js_config()->get_child_bool("enabled"))
    {
        new_service("gdbserver", static_cast<Gdbserver_engine *>(this));
    }
}


int Gdb_server::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);
    return 0;
}


void Gdb_server::start()
{
    if (this->rsp)
    {
        this->clock->retain();
        this->rsp->start(this->get_js_config()->get_child_int("port"));
    }
}


void Gdb_server::exit(int status)
{
    this->rsp->send_exit(status);
}

int Gdb_server::set_active_core(int id)
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Setting active core (id: %d)\n", id);

    if (this->cores[id] != NULL)
    {
        this->active_core = id;
        return 0;
    }

    return -1;
}

int Gdb_server::set_active_core_for_other(int id)
{
    this->trace.msg(vp::trace::LEVEL_DEBUG, "Setting active core for other (id: %d)\n", id);

    if (this->cores[id] != NULL)
    {
        this->active_core_for_other = id;
        return 0;
    }

    return -1;
}

vp::Gdbserver_core *Gdb_server::get_core(int id)
{
    if (id == -1)
    {
        id = this->active_core;
    }

    return this->cores[id];
}

vp::Gdbserver_core *Gdb_server::get_active_core()
{
    return this->get_core(this->active_core);
}

vp::Gdbserver_core *Gdb_server::get_active_core_for_other()
{
    return this->get_core(this->active_core_for_other);
}



int Gdb_server::io_access(uint32_t addr, int size, uint8_t *data, bool is_write)
{
    return this->get_active_core_for_other()->gdbserver_io_access(addr, size, data, is_write);
}

void Gdb_server::breakpoint_insert(uint64_t addr)
{
    for (auto x: this->cores)
    {
        x.second->gdbserver_breakpoint_insert(addr);
    }
}

void Gdb_server::breakpoint_remove(uint64_t addr)
{
    for (auto x: this->cores)
    {
        x.second->gdbserver_breakpoint_remove(addr);
    }
}


extern "C" vp::component *vp_constructor(js::config *config)
{
  return new Gdb_server(config);
}

#if 0

void Gdb_server::start() {
    target = std::make_shared<Target>(this);

    bkp = std::make_shared<Breakpoints>(this);

  
}
 
void Gdb_server::target_update_power() {
    target->update_power();
}

void Gdb_server::signal_exit(int status) {
    rsp->signal_exit(status);
}

void Gdb_server::refresh_target()
{
    target->reinitialize();
    target->update_power();
    bkp->enable_all();
}

int Gdb_server::stop()
{
    if (rsp != NULL)
    {
        rsp->stop();
        rsp = NULL;
    }
    return 1;
}

void Gdb_server::abort()
{
    if (rsp != NULL)
    {
        rsp->abort();
        rsp = NULL;
    }
}

#endif