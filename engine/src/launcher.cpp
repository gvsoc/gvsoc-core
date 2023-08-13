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
        this->engine_thread = new std::thread(&Gvsoc_launcher::engine_routine, this);
    }

    if (1)
    {
        // Create the sigint thread so that we can properly close simulation
        // in case ctrl C is hit.
        sigset_t sigs_to_block;
        sigemptyset(&sigs_to_block);
        sigaddset(&sigs_to_block, SIGINT);
        pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
        pthread_create(&sigint_thread, NULL, signal_routine, (void *)this);

        signal(SIGINT, sigint_handler);
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
        std::unique_lock<std::mutex> lock(this->mutex);
        if (this->engine_state == ENGINE_STATE_IDLE)
        {
            this->requests.push(new Launcher_request(ENGINE_REQ_RUN));
            this->engine->wait_for_lock_stop();
        }
        lock.unlock();
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
        std::unique_lock<std::mutex> lock(this->mutex);
        while(this->engine_state != ENGINE_STATE_FINISHED)
        {
            this->cond.wait(lock);
        }
        lock.unlock();
    }

    this->retval = this->engine->status_get();

    return this->retval;
}

int64_t Gvsoc_launcher::stop()
{
    if (this->is_async)
    {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->engine_state == ENGINE_STATE_RUNNING)
        {
            lock.unlock();
            this->engine->lock();
            this->engine->pause();
            this->engine->unlock();
            lock.lock();

            // while (this->engine_state == ENGINE_STATE_RUNNING)
            // {
            //     this->cond.wait(lock);
            // }
        }

        lock.unlock();
        return this->engine->get_time();
    }
    return -1;
}

void Gvsoc_launcher::wait_stopped()
{
    if (this->is_async)
    {
        std::unique_lock<std::mutex> lock(this->mutex);

        while (this->engine_state == ENGINE_STATE_RUNNING)
        {
            this->cond.wait(lock);
        }

        lock.unlock();
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
        std::unique_lock<std::mutex> lock(this->mutex);
        while (this->engine_state != ENGINE_STATE_IDLE)
        {
            lock.unlock();
            this->engine->lock();
            this->engine->pause();
            this->engine->unlock();
            lock.lock();

            if (this->engine_state != ENGINE_STATE_IDLE)
            {
                this->cond.wait(lock);
            }
        }

        if (this->engine_state == ENGINE_STATE_FINISHED)
        {
            time = end_time;
        }
        else
        {

            this->requests.push(new Launcher_request(ENGINE_REQ_RUN_UNTIL, end_time));
            this->engine->wait_for_lock_stop();
            time = end_time;
        }
        lock.unlock();
    }
    else
    {
        time = this->engine->run_until(end_time);
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
    while(1)
    {
        std::unique_lock<std::mutex> lock(this->mutex);

        while(this->requests.empty())
        {
            lock.unlock();
            this->engine->wait_for_lock();
            lock.lock();

            if (this->engine->finished_get())
            {
                this->engine_state = ENGINE_STATE_FINISHED;
                this->cond.notify_all();
            }
        }

        Launcher_request *req = this->requests.front();
        this->requests.pop();

        switch (req->type)
        {
            case ENGINE_REQ_RUN:
            case ENGINE_REQ_RUN_UNTIL:
                if (this->engine_state == ENGINE_STATE_IDLE)
                {
                    int64_t time;
                    this->engine_state = ENGINE_STATE_RUNNING;

                    for (auto x: this->exec_notifiers)
                    {
                        x->notify_run(req->time);
                    }
 
                    this->cond.notify_all();
                    lock.unlock();
                    if (req->type == ENGINE_REQ_RUN_UNTIL)
                    {
                        time = this->engine->run_until(req->time);
                    }
                    else
                    {
                        time = this->engine->run();
                    }
                    lock.lock();

                    this->engine_state = this->engine->finished_get() ? ENGINE_STATE_FINISHED : ENGINE_STATE_IDLE;
                    this->cond.notify_all();

                    for (auto x: this->exec_notifiers)
                    {
                        x->notify_stop(time);
                    }

                    lock.unlock();
                }
                break;
        }
    }
}



void Gvsoc_launcher::register_exec_notifier(vp::Notifier *notifier)
{
    this->exec_notifiers.push_back(notifier);
}
