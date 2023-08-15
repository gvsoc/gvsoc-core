/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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


#include <pthread.h>
#include <signal.h>

#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/launcher.hpp>

static pthread_t sigint_thread;

// Global signal handler to catch sigint when we are in C world and after
// the engine has started.
// Just few pthread functions are signal-safe so just forward the signal to
// the sigint thread so that he can properly stop the engine
static void sigint_handler(int s)
{
    pthread_kill(sigint_thread, SIGINT);
}

// This thread takes care of properly stopping the engine when ctrl C is hit
// so that the python world can properly close everything
void *Gvsoc_launcher::signal_routine(void *__this)
{
    Gvsoc_launcher *launcher = (Gvsoc_launcher *)__this;
    sigset_t sigs_to_catch;
    int caught;
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGINT);
    do
    {
        sigwait(&sigs_to_catch, &caught);
        launcher->engine->quit(-1);
    } while (1);
    return NULL;
}

Gvsoc_launcher::Gvsoc_launcher(gv::GvsocConf *conf)
{
    this->conf = conf;
    this->is_async = conf->api_mode == gv::Api_mode::Api_mode_async;
}

void Gvsoc_launcher::open()
{
    this->handler = new vp::top(conf->config_path, this->is_async);

    this->instance = ((vp::top *)this->handler)->top_instance;
    this->instance->set_launcher(this);
    this->engine = ((vp::top *)this->handler)->time_engine;

    if (instance->get_vp_config()->get_child_bool("proxy/enabled"))
    {
        int in_port = instance->get_vp_config()->get_child_int("proxy/port");
        int out_port;
        this->proxy = new Gv_proxy(this->engine, instance, this);

        if (this->proxy->open(in_port, &out_port))
        {
            instance->throw_error("Failed to start proxy");
        }

        this->conf->proxy_socket = out_port;
    }

    if (this->is_async)
    {
        // Create the sigint thread so that we can properly close simulation
        // in case ctrl C is hit.
        sigset_t sigs_to_block;
        sigemptyset(&sigs_to_block);
        sigaddset(&sigs_to_block, SIGINT);
        pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
        pthread_create(&sigint_thread, NULL, signal_routine, (void *)this);

        signal(SIGINT, sigint_handler);

        this->engine_thread = new std::thread(&Gvsoc_launcher::engine_routine, this);
    }
}

void Gvsoc_launcher::bind(gv::Gvsoc_user *user)
{
    this->user = user;
    this->engine->bind_to_launcher(user);
}

void Gvsoc_launcher::start()
{
    this->instance->build_new();
    this->instance->reset_all(true);
    this->instance->reset_all(false);
}

void Gvsoc_launcher::close()
{
    if (proxy)
    {
        proxy->stop(this->retval);
    }

    this->instance->stop_all();

    vp::top *top = (vp::top *)this->handler;

    delete top;
}

void Gvsoc_launcher::run()
{
    if (this->is_async)
    {
        this->engine->lock();
        this->running = true;
        this->run_req = true;
        this->engine->unlock();
    }
    else
    {
        this->engine->run();
    }
}

int Gvsoc_launcher::join()
{
    if (this->is_async)
    {
        this->engine->critical_enter();
        while(!this->engine->finished_get())
        {
            this->engine->critical_wait();
        }
        this->engine->critical_exit();
    }

    this->retval = this->engine->status_get();

    return this->retval;
}

int64_t Gvsoc_launcher::stop()
{
    if (this->is_async)
    {
        this->engine->lock();
        this->run_req = false;
        this->engine->pause();
        this->engine->unlock();

        return this->engine->get_time();
    }
    return -1;
}

void Gvsoc_launcher::wait_stopped()
{
    if (this->is_async)
    {
        this->engine->critical_enter();
        while(this->running)
        {
            this->engine->critical_wait();
        }
        this->engine->critical_exit();
    }
}

int64_t Gvsoc_launcher::step(int64_t duration)
{
    return this->step_until(this->engine->get_time() + duration);
}

int64_t Gvsoc_launcher::step_until(int64_t end_time)
{
    int64_t time;
    if (this->is_async)
    {

        this->engine->lock();
        this->engine->step_register(end_time);
        this->run_req = true;
        this->engine->unlock();

        return end_time;
    }
    else
    {
        this->engine->step_register(end_time);
        this->engine->run();
    }
    return time;
}

gv::Io_binding *Gvsoc_launcher::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name)
{
    return (gv::Io_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

gv::Wire_binding *Gvsoc_launcher::wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name)
{
    return (gv::Wire_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

void Gvsoc_launcher::vcd_bind(gv::Vcd_user *user)
{
    this->instance->traces.get_trace_manager()->set_vcd_user(user);
}

void Gvsoc_launcher::event_add(std::string path, bool is_regex)
{
    this->instance->traces.get_trace_manager()->conf_trace(1, path, 1);
}

void Gvsoc_launcher::event_exclude(std::string path, bool is_regex)
{
    this->instance->traces.get_trace_manager()->conf_trace(1, path, 0);
}


static std::vector<std::string> split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}



void Gvsoc_launcher::update(int64_t timestamp)
{
    this->engine->update(timestamp);
}



void *Gvsoc_launcher::get_component(std::string path)
{
    return this->instance->get_component(split(path, '/'));
}



void Gvsoc_launcher::engine_routine()
{
    this->engine->critical_enter();

    while(1)
    {
        // Wait until we receive a run request
        while (!this->run_req)
        {
            this->engine->critical_wait();
            this->engine->handle_locks();
        }

        // Switch to runnning state and properly notify it
        this->running = true;

        for (auto x: this->exec_notifiers)
        {
            x->notify_run(this->engine->get_time());
        }

        this->engine->critical_notify();

        // Run until we are ask to stop or simulation is over
        while (this->running)
        {
            // Clear the run request so that we can receive a new one while running
            this->run_req = false;

            // The engine will return -1 if it receives a stop request.
            // Leave only if we have not receive a run request meanwhile which would then cancel
            // the stop request.
            if (this->engine->run() == -1 && (!this->run_req || this->engine->finished_get()))
            {
                this->running = false;
                this->engine->critical_notify();
            }
        }

        // Properly notify the stop and finish state
        for (auto x: this->exec_notifiers)
        {
            x->notify_stop(this->engine->get_time());
        }

        if (this->engine->finished_get())
        {
            this->engine->critical_notify();

            while(1)
            {
                this->engine->critical_wait();
                this->engine->handle_locks();
            }
        }
    }
}



void Gvsoc_launcher::register_exec_notifier(vp::Notifier *notifier)
{
    this->exec_notifiers.push_back(notifier);
}
