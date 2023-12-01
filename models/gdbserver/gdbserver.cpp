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


Gdb_server::Gdb_server(vp::ComponentConf &conf)
: vp::Component(conf)
{
    js::Config *config = this->get_js_config();
    this->rsp = NULL;

    if (config->get_child_bool("enabled"))
    {
        this->rsp = new Rsp(this);
    }

    this->default_hartid = 0;
    this->active_core = 0;

    if (this->get_js_config()->get_child_bool("enabled"))
    {
        new_service("gdbserver", static_cast<Gdbserver_engine *>(this));
    }

    traces.new_trace("trace", &trace, vp::DEBUG);
}


int Gdb_server::register_core(vp::Gdbserver_core *core)
{
    int id = this->current_id++;
    core->gdbserver_set_id(id);

    this->trace.msg(vp::Trace::LEVEL_INFO, "Registering core (id: %d)\n", id);

    this->cores[id] = core;
    this->cores_list.push_back(core);

    js::Config *config = this->get_js_config();
    if (core->gdbserver_get_name() == config->get_child_str("default_hartid"))
    {
        this->default_hartid = core->gdbserver_get_id();
        this->active_core = core->gdbserver_get_id();
    }

    return 0;
}


void Gdb_server::signal(vp::Gdbserver_core *core, int signal, std::string reason, int info)
{
    this->rsp->signal_from_core(core, signal, reason, info);
}



void Gdb_server::start()
{
    if (this->rsp)
    {
        this->rsp->start(this->get_js_config()->get_child_int("port"));
    }
}


void Gdb_server::exit(int status)
{
    this->rsp->send_exit(status);
}

int Gdb_server::set_active_core(int id)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting active core (id: %d)\n", id);

    if (this->cores[id] != NULL)
    {
        this->active_core = id;
        return 0;
    }

    return -1;
}

int Gdb_server::set_active_core_for_other(int id)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting active core for other (id: %d)\n", id);

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



int Gdb_server::io_access(uint64_t addr, int size, uint8_t *data, bool is_write)
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

void Gdb_server::watchpoint_insert(bool is_write, uint64_t addr, int size)
{
    for (auto x: this->cores)
    {
        x.second->gdbserver_watchpoint_insert(is_write, addr, size);
    }
}

void Gdb_server::watchpoint_remove(bool is_write, uint64_t addr, int size)
{
    for (auto x: this->cores)
    {
        x.second->gdbserver_watchpoint_remove(is_write, addr, size);
    }
}

vp::Gdbserver_core *Gdb_server::get_core_from_name(std::string name)
{
    for (vp::Gdbserver_core *core: this->get_cores())
    {
        if (core->gdbserver_get_name() == name)
        {
            return core;
        }
    }

    return NULL;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &conf)
{
  return new Gdb_server(conf);
}
