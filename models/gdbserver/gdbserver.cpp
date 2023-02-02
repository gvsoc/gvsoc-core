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
    this->out.set_resp_meth(&Gdb_server::io_response);
    this->out.set_grant_meth(&Gdb_server::io_grant);
    this->new_master_port("out", &this->out);
    this->event = this->event_new(&Gdb_server::handle);
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


vp::Gdbserver_core *Gdb_server::get_core(int id)
{
    if (id == -1)
    {
        id = this->active_core;
    }

    return this->cores[id];
}


void Gdb_server::handle(void *__this, vp::clock_event *event)
{
    Gdb_server *_this = (Gdb_server *)__this;

    std::unique_lock<std::mutex> lock(_this->mutex);
    _this->waiting_io_response = false;
    _this->cond.notify_all();
    lock.unlock();
}


void Gdb_server::io_grant(void *__this, vp::io_req *req)
{
}

void Gdb_server::io_response(void *__this, vp::io_req *req)
{
    Gdb_server *_this = (Gdb_server *)__this;

    int64_t latency = _this->io_req.get_latency();
    _this->event_enqueue(_this->event, latency + 1);
}

int Gdb_server::io_access(uint32_t addr, int size, uint8_t *data, bool is_write)
{
    int retval = 0;
    this->io_req.init();
    this->io_req.set_addr(addr);
    this->io_req.set_size(size);
    this->io_req.set_data(data);
    this->io_req.set_is_write(is_write);

    this->get_time_engine()->lock();
    int err = this->out.req(&this->io_req);
    if (err == vp::IO_REQ_OK)
    {
        this->waiting_io_response = true;
        int64_t latency = this->io_req.get_latency();
        this->event_enqueue(this->event, latency + 1);
        this->get_time_engine()->unlock();

        std::unique_lock<std::mutex> lock(this->mutex);

        while (this->waiting_io_response)
        {
            this->cond.wait(lock);
        }

        lock.unlock();

    }
    else if (err == vp::IO_REQ_INVALID)
    {
        this->io_retval = 1;
        this->get_time_engine()->unlock();
    }
    else
    {
        this->get_time_engine()->unlock();

        std::unique_lock<std::mutex> lock(this->mutex);

        while (this->waiting_io_response)
        {
            this->cond.wait(lock);
        }

        lock.unlock();
    }

    return this->io_retval;
}

void Gdb_server::breakpoint_insert(uint64_t addr)
{
    if (this->active_core != -1)
    {
        vp::Gdbserver_core *core = this->cores[this->active_core];
        core->gdbserver_breakpoint_insert(addr);
    }
    else
    {
        for (auto x: this->cores)
        {
            x.second->gdbserver_breakpoint_insert(addr);
        }
    }
}

void Gdb_server::breakpoint_remove(uint64_t addr)
{
    if (this->active_core != -1)
    {
        vp::Gdbserver_core *core = this->cores[this->active_core];
        core->gdbserver_breakpoint_remove(addr);
    }
    else
    {
        for (auto x: this->cores)
        {
            x.second->gdbserver_breakpoint_remove(addr);
        }
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