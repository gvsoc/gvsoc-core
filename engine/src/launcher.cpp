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
#include <algorithm>

#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/launcher.hpp>
#include <vp/proxy_client.hpp>
#include "vp/top.hpp"

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
void *gv::GvsocLauncher::signal_routine(void *__this)
{
    GvsocLauncher *launcher = (GvsocLauncher *)__this;
    sigset_t sigs_to_catch;
    int caught;
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGINT);
    do
    {
        sigwait(&sigs_to_catch, &caught);
        launcher->handler->get_time_engine()->quit(-1);
    } while (1);
    return NULL;
}

gv::GvsocLauncher::GvsocLauncher(gv::GvsocConf *conf)
{
    this->conf = conf;
    this->is_async = conf->api_mode == gv::Api_mode::Api_mode_async;
}

void gv::GvsocLauncher::open(GvsocLauncherClient *client)
{
    this->handler = new vp::Top(conf->config_path, this->is_async, this);

    this->instance = this->handler->top_instance;
    this->instance->set_launcher(this);

    js::Config *gv_config = this->handler->gv_config;

    this->proxy = NULL;
    if (gv_config->get_child_bool("proxy/enabled"))
    {
        int in_port = gv_config->get_child_int("proxy/port");
        int out_port;
        this->proxy = new GvProxy(this->handler->get_time_engine(), instance, this, this->is_async);

        if (this->proxy->open(in_port, &out_port))
        {
            throw runtime_error("Failed to start proxy");
        }

        this->conf->proxy_socket = out_port;
    }

    this->handler->get_time_engine()->critical_enter();

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

        this->engine_thread = new std::thread(&gv::GvsocLauncher::engine_routine, this);
#ifndef __APPLE__
        pthread_setname_np(this->engine_thread->native_handle(), "engine");
#endif
    }
    else
    {
        this->retain(client);
    }
}

void gv::GvsocLauncher::bind(gv::Gvsoc_user *user, GvsocLauncherClient *client)
{
    this->user = user;
    this->handler->get_time_engine()->bind_to_launcher(user);
}

void gv::GvsocLauncher::start(GvsocLauncherClient *client)
{
    this->instance->build_all();
    this->handler->start();
    this->instance->reset_all(true);
    this->instance->reset_all(false);
}

void gv::GvsocLauncher::close(GvsocLauncherClient *client)
{
    this->instance->stop_all();

    vp::Top *top = (vp::Top *)this->handler;

    delete top;
}

void gv::GvsocLauncher::run_internal(bool main_controller)
{
    if (this->is_async)
    {
        this->handler->get_time_engine()->lock();
        // Mark the engine now as running to block wait_stopped until engine has been run
        this->running = true;
        this->run_req = true;
        this->handler->get_time_engine()->unlock();
    }
    else
    {
        this->handler->get_time_engine()->run(main_controller);
    }
}

void gv::GvsocLauncher::run(GvsocLauncherClient *client)
{
    this->run_internal(true);
}


void gv::GvsocLauncher::flush(GvsocLauncherClient *client)
{
    this->handler->flush();
}

void gv::GvsocLauncher::sim_finished(int status)
{
    this->is_sim_finished = true;
    this->retval = status;

    for (gv::GvsocLauncherClient *client: this->clients)
    {
        client->sim_finished(status);
    }
}

int gv::GvsocLauncher::join(GvsocLauncherClient *client)
{
    if (this->is_async)
    {
        this->handler->get_time_engine()->critical_enter();
        while(!this->is_sim_finished)
        {
            this->handler->get_time_engine()->critical_wait();
        }
        this->handler->get_time_engine()->critical_exit();
    }

    int retval = this->retval;

    std::unique_lock<std::mutex> lock(this->mutex);

    for (gv::GvsocLauncherClient *client: this->clients)
    {
        while (!client->has_quit)
        {
            this->cond.wait(lock);
        }

        if (client->status != 0)
        {
            retval = client->status;
        }
    }

    lock.unlock();

    return this->retval;
}

int64_t gv::GvsocLauncher::stop(GvsocLauncherClient *client)
{
    if (this->is_async)
    {
        this->handler->get_time_engine()->lock();
        this->run_req = false;
        this->handler->get_time_engine()->pause();
        this->handler->get_time_engine()->unlock();

        return this->handler->get_time_engine()->get_time();
    }
    return -1;
}

void gv::GvsocLauncher::wait_stopped(GvsocLauncherClient *client)
{
    if (this->is_async)
    {
        this->handler->get_time_engine()->critical_enter();
        while(this->running)
        {
            this->handler->get_time_engine()->critical_wait();
        }
        this->handler->get_time_engine()->critical_exit();
    }
}

int64_t gv::GvsocLauncher::step_internal(int64_t duration, GvsocLauncherClient *client, bool main_controller)
{
    return this->step_until(this->handler->get_time_engine()->get_time() + duration, client);
}

int64_t gv::GvsocLauncher::step(int64_t duration, GvsocLauncherClient *client)
{
    return this->step_internal(duration, client, true);
}

int64_t gv::GvsocLauncher::step_until_internal(int64_t end_time, bool main_controller)
{
    int64_t time;
    if (this->is_async)
    {
        this->handler->get_time_engine()->lock();
        this->handler->get_time_engine()->step_register(end_time);
        // Mark the engine now as running to block wait_stopped until engine has been run
        this->running = true;
        this->run_req = true;
        this->handler->get_time_engine()->unlock();

        return end_time;
    }
    else
    {
        for (auto x: this->exec_notifiers)
        {
            x->notify_run(this->handler->get_time_engine()->get_time());
        }

        time = this->handler->get_time_engine()->run_until(end_time, main_controller);

        for (auto x: this->exec_notifiers)
        {
            x->notify_stop(this->handler->get_time_engine()->get_time());
        }
    }
    return time;
}

int64_t gv::GvsocLauncher::step_until(int64_t end_time, GvsocLauncherClient *client)
{
    return this->step_until_internal(end_time, true);
}

int64_t gv::GvsocLauncher::step_and_wait(int64_t duration, GvsocLauncherClient *client)
{
    return this->step_until_and_wait(this->handler->get_time_engine()->get_time() + duration, client);
}

int64_t gv::GvsocLauncher::step_until_and_wait(int64_t timestamp, GvsocLauncherClient *client)
{
    int64_t end_time = this->step_until(timestamp, client);

    if (this->is_async)
    {
        while (this->handler->get_time_engine()->get_time() < timestamp)
        {
            this->wait_stopped(client);
        }
    }

    return end_time;
}

void gv::GvsocLauncher::client_quit(gv::GvsocLauncherClient *client)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->cond.notify_all();
    lock.unlock();
}

gv::Io_binding *gv::GvsocLauncher::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name, GvsocLauncherClient *client)
{
    return (gv::Io_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

gv::Wire_binding *gv::GvsocLauncher::wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name, GvsocLauncherClient *client)
{
    return (gv::Wire_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

void gv::GvsocLauncher::vcd_bind(gv::Vcd_user *user, GvsocLauncherClient *client)
{
    this->instance->traces.get_trace_engine()->set_vcd_user(user);
}

void gv::GvsocLauncher::vcd_enable(GvsocLauncherClient *client)
{
    this->instance->traces.get_trace_engine()->set_global_enable(1);
}

void gv::GvsocLauncher::vcd_disable(GvsocLauncherClient *client)
{
    this->instance->traces.get_trace_engine()->set_global_enable(0);
}

void gv::GvsocLauncher::event_add(std::string path, bool is_regex, GvsocLauncherClient *client)
{
    this->instance->traces.get_trace_engine()->conf_trace(1, path, 1);
}

void gv::GvsocLauncher::event_exclude(std::string path, bool is_regex, GvsocLauncherClient *client)
{
    this->instance->traces.get_trace_engine()->conf_trace(1, path, 0);
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



void gv::GvsocLauncher::update(int64_t timestamp, GvsocLauncherClient *client)
{
    this->handler->get_time_engine()->update(timestamp);
}



void *gv::GvsocLauncher::get_component(std::string path, GvsocLauncherClient *client)
{
    return this->instance->get_block_from_path(split(path, '/'));
}



void gv::GvsocLauncher::engine_routine()
{
    while(1)
    {
        // Wait until we receive a run request
        while (!this->run_req)
        {
            this->handler->get_time_engine()->critical_wait();
            this->handler->get_time_engine()->handle_locks();
        }

        // Switch to runnning state and properly notify it
        this->running = true;

        for (auto x: this->exec_notifiers)
        {
            x->notify_run(this->handler->get_time_engine()->get_time());
        }

        this->handler->get_time_engine()->critical_notify();

        // Run until we are ask to stop or simulation is over
        while (this->running)
        {
            // Clear the run request so that we can receive a new one while running
            this->run_req = false;

            // The engine will return -1 if it receives a stop request.
            // Leave only if we have not receive a run request meanwhile which would then cancel
            // the stop request.
            if (this->handler->get_time_engine()->run(true) == -1 && (!this->run_req || this->handler->get_time_engine()->finished_get()))
            {
                this->running = false;
                this->handler->get_time_engine()->critical_notify();
            }
        }

        // Properly notify the stop and finish state
        for (auto x: this->exec_notifiers)
        {
            x->notify_stop(this->handler->get_time_engine()->get_time());
        }

        if (this->handler->get_time_engine()->finished_get())
        {
            this->handler->get_time_engine()->critical_notify();

            while(1)
            {
                this->handler->get_time_engine()->critical_wait();
                this->handler->get_time_engine()->handle_locks();
            }
        }
    }
}



void gv::GvsocLauncher::register_exec_notifier(GvsocLauncher_notifier *notifier)
{
    this->exec_notifiers.push_back(notifier);
}


void gv::GvsocLauncher::retain(GvsocLauncherClient *client)
{
    this->handler->get_time_engine()->retain_inc(1);
}

int gv::GvsocLauncher::retain_count(GvsocLauncherClient *client)
{
    return this->handler->get_time_engine()->retain_count();
}

void gv::GvsocLauncher::lock(GvsocLauncherClient *client)
{
    this->handler->get_time_engine()->critical_enter();
}

void gv::GvsocLauncher::unlock(GvsocLauncherClient *client)
{
    this->handler->get_time_engine()->critical_exit();
}

void gv::GvsocLauncher::release(GvsocLauncherClient *client)
{
    this->handler->get_time_engine()->retain_inc(-1);
}

double gv::GvsocLauncher::get_instant_power(double &dynamic_power, double &static_power, GvsocLauncherClient *client)
{
    return this->instance->power.get_instant_power(dynamic_power, static_power);
}

double gv::GvsocLauncher::get_average_power(double &dynamic_power, double &static_power, GvsocLauncherClient *client)
{
    return this->instance->power.get_average_power(dynamic_power, static_power);
}

void gv::GvsocLauncher::report_start(GvsocLauncherClient *client)
{
    this->instance->power.get_engine()->start_capture();
}

void gv::GvsocLauncher::report_stop(GvsocLauncherClient *client)
{
    this->instance->power.get_engine()->stop_capture();
}

gv::PowerReport *gv::GvsocLauncher::report_get(GvsocLauncherClient *client)
{
    return this->instance->power.get_report();
}



gv::Gvsoc *gv::gvsoc_new(gv::GvsocConf *conf)
{
    if (conf->proxy_socket != -1)
    {
        return new Gvsoc_proxy_client(conf);
    }
    else
    {
        return new gv::GvsocLauncherClient(conf);
    }
}

void gv::GvsocLauncher::register_client(GvsocLauncherClient *client)
{
    this->clients.push_back(client);
}

void gv::GvsocLauncher::unregister_client(GvsocLauncherClient *client)
{
    this->clients.erase(
        std::remove(this->clients.begin(), this->clients.end(), client),
        this->clients.end());
}
