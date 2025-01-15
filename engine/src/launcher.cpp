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

#include <stdexcept>
#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/controller.hpp>
#include <vp/proxy_client.hpp>
#include <vp/top.hpp>

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
void *gv::Controller::signal_routine(void *__this)
{
    Controller *launcher = (Controller *)__this;
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

gv::Controller::Controller()
: logger("LAUNCHER")
{

}

void gv::Controller::syscall_stop_handle()
{
    for (gv::ControllerClient *client: this->clients)
    {
        if (client->user)
        {
            client->user->handle_syscall_stop();
        }
    }
}

void gv::Controller::init(gv::GvsocConf *conf)
{
    if (!this->is_init)
    {
        this->is_init = true;
        this->conf = conf;
        this->is_async = conf == NULL || conf->api_mode == gv::Api_mode::Api_mode_async;
        pthread_mutex_init(&this->mutex, NULL);
        pthread_mutex_init(&this->lock_mutex, NULL);
        pthread_cond_init(&this->cond, NULL);

        if (conf)
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
                this->proxy = new GvProxy(this->handler->get_time_engine(), instance);

                if (this->proxy->open(in_port, &out_port))
                {
                    throw runtime_error("Failed to start proxy");
                }

                this->conf->proxy_socket = out_port;
            }

            this->step_block = new vp::Block(NULL, "stepper", this->handler->get_time_engine(),
                this->handler->get_trace_engine(), this->handler->get_power_engine());
        }
    }
}

void gv::Controller::open(ControllerClient *client)
{
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

        // In asynchronous mode, a dedicated thread is running the time engine.
        this->engine_thread = new std::thread(&gv::Controller::engine_routine, this);
#ifndef __APPLE__
        pthread_setname_np(this->engine_thread->native_handle(), "engine");
#endif
    }
    else
    {
        // It is also considered always running since anyway the engine is making progress
        // only when the external loop is running it.
        this->client_run(client);
    }
}

void gv::Controller::bind(gv::Gvsoc_user *user, ControllerClient *client)
{
    this->user = user;
    this->handler->get_time_engine()->bind_to_launcher(user);
}

void gv::Controller::start(ControllerClient *client)
{
    this->instance->build_all();
    this->handler->start();
    this->instance->reset_all(true);
    this->instance->reset_all(false);

    // Now that all initialization are done, wait for at least one proxy connection before
    // running.
    if (this->proxy)
    {
        this->proxy->wait_connected();
    }

    if (!this->is_async)
    {
        // The main loop, either internally in asynchronous or externally in synchronous mode,
        // is by default having the lock so that any client must take it to stop the engine.
        // The main loop will release the lock after a stop request and when it is waiting that
        // that the simulation becomes runnable again.
        pthread_mutex_lock(&this->mutex);
    }

    // Now simulation can start
    this->check_run();
}

void gv::Controller::close(ControllerClient *client)
{
    this->instance->stop_all();

    vp::Top *top = (vp::Top *)this->handler;

    delete top;
}

int64_t gv::Controller::run_sync()
{
    // Since the stop mechanism is lock-free, we need to issue memory barrier to see latest
    // version of running field
    __sync_synchronize();

    // Wait until the simulation becomes runnable.
    // Simulation can be stopped if any client stopped it or locked it.
    while (!this->running)
    {
        pthread_cond_wait(&this->cond, &this->mutex);
    }

    this->logger.info("Running time engine\n");

    // Now run the engine, this will execute events until a stop request is issued.
    // The engine will then execute events until the current timestamp is over and will then return.
    int64_t time = this->handler->get_time_engine()->run();

    // We handle the simulation termination only once to notify everyone.
    // Then simulation can freely continue if needed
    if (!this->notified_finish && this->handler->get_time_engine()->finished_get())
    {
        this->notified_finish = true;
        this->sim_finished(this->handler->get_time_engine()->stop_status);
    }

    return time;
}

void gv::Controller::run_async(ControllerClient *client)
{
    this->client_run(client);
}

void gv::Controller::client_run(ControllerClient *client)
{
    if (!client->running)
    {
        client->running = true;
        this->run_count++;
        this->logger.info("Turn client to running (client: %s, run_count: %d)\n",
            client->name.c_str(), this->run_count);
        this->check_run();
    }
}

void gv::Controller::client_stop(ControllerClient *client)
{
    if (client->running)
    {
        client->running = false;
        this->run_count--;
        this->logger.info("Turn client to stopped (client: %s, run_count: %d)\n",
            client->name.c_str(), this->run_count);
        this->check_run();
    }
}


void gv::Controller::flush(ControllerClient *client)
{
    this->handler->flush();
}

void gv::Controller::sim_finished(int status)
{
    this->logger.info("Simulation has finished\n");

    // When simulation is over, we record the status, mark simulation as finished and notified
    // every client.
    if (!this->is_sim_finished)
    {
        this->is_sim_finished = true;
        this->retval = status;


        // Cancel any pending step
        while (this->step_block->time.first_event)
        {
            vp::TimeEvent *event = this->step_block->time.first_event;
            event->exec();
            event->cancel();
        }

        for (gv::ControllerClient *client: this->clients)
        {
            client->sim_finished(status);
        }

        pthread_cond_broadcast(&this->cond);
    }
}

int gv::Controller::join(ControllerClient *client)
{
    // Make the calling client runnable so that it does not prevent simulation from running for
    // other clients
    this->client_run(client);

    // Wait simulation is over
    while(!this->is_sim_finished)
    {
        if (this->is_async)
        {
            pthread_cond_wait(&this->cond, &this->mutex);
        }
        else
        {
            this->run_sync();
        }
    }

    // Wait for each client to finish and collect status from everyone
    int retval = this->retval;
    for (gv::ControllerClient *wait_client: this->clients)
    {
        while (!wait_client->has_quit)
        {
            pthread_cond_wait(&this->cond, &this->mutex);
        }

        if (wait_client->status != 0)
        {
            retval = wait_client->status;
        }
    }

    return retval;
}

int64_t gv::Controller::stop(ControllerClient *client)
{
    this->client_stop(client);

    // Since this method must be called with the engine locked, we are sure it is already stopped
    // thus we can safely return the current time as the time where it was stopped.
    return this->handler->get_time_engine()->get_time();
}

int64_t gv::Controller::step_sync(int64_t duration, ControllerClient *client)
{
    return this->step_until_sync(this->handler->get_time_engine()->get_time() + duration, client);
}

int64_t gv::Controller::step_async(int64_t duration, ControllerClient *client, bool wait, void *request)
{
    return this->step_until_async(this->handler->get_time_engine()->get_time() + duration, client, wait, request);
}

void gv::Controller::step_async_handler(vp::Block *__this, vp::TimeEvent *event)
{
    Controller *_this = (Controller *)event->get_args()[0];
    ControllerClient *client = (ControllerClient *)event->get_args()[1];
    _this->logger.info("Step handler\n");
    _this->client_stop(client);

    for (gv::ControllerClient *client: _this->clients)
    {
        if (client->user)
        {
            client->user->handle_step_end(event->get_args()[2]);
        }
    }

    delete event;
}

void gv::Controller::step_sync_handler(vp::Block *__this, vp::TimeEvent *event)
{
    Controller *_this = (Controller *)event->get_args()[0];
    ControllerClient *client = (ControllerClient *)event->get_args()[1];
    _this->logger.info("Step handler\n");
    _this->handler->get_time_engine()->pause();
}

int64_t gv::Controller::step_until_sync(int64_t end_time, ControllerClient *client)
{
    // Only go a step if the end time is not the present time
    if (end_time > this->instance->time.get_engine()->get_time())
    {
        // The idea to implement the step is to enqueue a time event which will stop the engine
        // when the step timestamp is reached.
        int64_t time = -1;

        // Enqueue the event which will stop the engine exactly when we need
        client->step_event->enqueue(end_time - this->instance->time.get_engine()->get_time());

        // Now let the engine run until we reach the end of step.
        // We may stop several times if other clients ask to, so we need to iterate as long
        // as needed.
        while (this->handler->get_time_engine()->get_time() < end_time && !this->is_sim_finished)
        {
            time = this->run_sync();

        }

        return time;
    }
    else
    {
        return this->handler->get_time_engine()->get_next_event_time();
    }
}

int64_t gv::Controller::step_until_async(int64_t end_time, ControllerClient *client, bool wait, void *request)
{
    vp::TimeEvent *event = new vp::TimeEvent(this->step_block);
    event->set_callback(this->step_async_handler);
    event->get_args()[0] = this;
    event->get_args()[1] = client;
    event->get_args()[2] = request;

    event->enqueue(end_time - this->instance->time.get_engine()->get_time());

    this->client_run(client);

    if (wait)
    {
        while (this->handler->get_time_engine()->get_time() < end_time && !this->is_sim_finished)
        {
            pthread_cond_wait(&this->cond, &this->mutex);
        }
    }

    return end_time;
}

void gv::Controller::wait_runnable()
{
    while (!this->is_runnable())
    {
        pthread_cond_wait(&this->cond, &this->mutex);
    }
}

bool gv::Controller::is_runnable()
{
    return this->run_count == this->clients.size() && this->lock_count == 0;
}

void gv::Controller::check_run()
{
    pthread_mutex_lock(&this->lock_mutex);
    bool should_run = this->run_count == this->clients.size() && this->lock_count == 0;

    this->logger.info("Checking engine (should_run: %d, running: %d, run_count: %d, nb_clients: %d, lock_count: %d)\n",
        should_run, this->running, this->run_count, this->clients.size(), this->lock_count);

    if (should_run != this->running)
    {
        if (should_run)
        {
            this->logger.info("Enqueue running\n");
            this->running = true;
        }
        else
        {
            this->logger.info("Enqueue stop\n");
            this->running = false;
            // Since the mechanism is lock-free and to make sure the engine thread sees the engine
            // not running after the stop, we need to issue memory barrier
            __sync_synchronize();
            this->handler->get_time_engine()->pause();
        }
        pthread_cond_broadcast(&this->cond);
    }
    pthread_mutex_unlock(&this->lock_mutex);
}

void gv::Controller::client_quit(gv::ControllerClient *client)
{
    // Just need to wake other clients, the launcher client code takes care of the rest
    pthread_cond_broadcast(&this->cond);
}

gv::Io_binding *gv::Controller::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name, ControllerClient *client)
{
    return (gv::Io_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

gv::Wire_binding *gv::Controller::wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name, ControllerClient *client)
{
    return (gv::Wire_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

void gv::Controller::vcd_bind(gv::Vcd_user *user, ControllerClient *client)
{
    this->instance->traces.get_trace_engine()->set_vcd_user(user);
}

void gv::Controller::vcd_enable(ControllerClient *client)
{
    this->instance->traces.get_trace_engine()->set_global_enable(1);
}

void gv::Controller::vcd_disable(ControllerClient *client)
{
    this->instance->traces.get_trace_engine()->set_global_enable(0);
}

void gv::Controller::event_add(std::string path, bool is_regex, ControllerClient *client)
{
    this->instance->traces.get_trace_engine()->conf_trace(1, path, 1);
}

void gv::Controller::event_exclude(std::string path, bool is_regex, ControllerClient *client)
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



void gv::Controller::update(int64_t timestamp, ControllerClient *client)
{
    this->handler->get_time_engine()->update(timestamp);
}



void *gv::Controller::get_component(std::string path, ControllerClient *client)
{
    return this->instance->get_block_from_path(split(path, '/'));
}

void gv::Controller::engine_routine()
{
    // The main loop, either internally in asynchronous or externally in synchronous mode,
    // is by default having the lock so that any client must take it to stop the engine.
    // The main loop will release the lock after a stop request and when it is waiting that
    // that the simulation becomes runnable again.
    pthread_mutex_lock(&this->mutex);

    while(1)
    {
        this->run_sync();
    }
}


void gv::Controller::lock()
{
    pthread_mutex_lock(&this->mutex);
}

void gv::Controller::unlock()
{
    pthread_mutex_unlock(&this->mutex);
}

void gv::Controller::engine_lock()
{
    this->logger.info("Engine lock (lock_count: %d)\n", this->lock_count);

    pthread_mutex_lock(&this->lock_mutex);

    this->lock_count++;
    this->running = false;
    __sync_synchronize();

    this->handler->get_time_engine()->pause();

    pthread_mutex_unlock(&this->lock_mutex);

    this->lock();
}

void gv::Controller::engine_unlock()
{
    this->logger.info("Engine unlock (lock_count: %d)\n", this->lock_count);

    pthread_mutex_lock(&this->lock_mutex);
    this->lock_count--;
    pthread_mutex_unlock(&this->lock_mutex);

    this->check_run();

    pthread_cond_broadcast(&this->cond);
    this->unlock();
}

double gv::Controller::get_instant_power(double &dynamic_power, double &static_power, ControllerClient *client)
{
    return this->instance->power.get_instant_power(dynamic_power, static_power);
}

double gv::Controller::get_average_power(double &dynamic_power, double &static_power, ControllerClient *client)
{
    return this->instance->power.get_average_power(dynamic_power, static_power);
}

void gv::Controller::report_start(ControllerClient *client)
{
    this->instance->power.get_engine()->start_capture();
}

void gv::Controller::report_stop(ControllerClient *client)
{
    this->instance->power.get_engine()->stop_capture();
}

gv::PowerReport *gv::Controller::report_get(ControllerClient *client)
{
    return this->instance->power.get_report();
}



gv::Gvsoc *gv::gvsoc_new(gv::GvsocConf *conf, std::string name)
{
    if (conf && conf->proxy_socket != -1)
    {
        return new Gvsoc_proxy_client(conf);
    }
    else
    {
        return new gv::ControllerClient(conf, name);
    }
}

void gv::Controller::register_client(ControllerClient *client)
{
    // In synchronous mode, since only one thread is allowed to do the step, we use a single
    // event and preallocate it for performance reason.
    client->step_event = new vp::TimeEvent(this->step_block);
    client->step_event->set_callback(this->step_sync_handler);
    client->step_event->get_args()[0] = this;
    client->step_event->get_args()[1] = client;

    // Add client to list of current clients
    this->clients.push_back(client);

    // And check if simulation must be stopped because of the client we added
    this->check_run();
}

void gv::Controller::unregister_client(ControllerClient *client)
{
    // Stop the client
    this->client_stop(client);

    // Remove it from list of current clients
    this->clients.erase(
        std::remove(this->clients.begin(), this->clients.end(), client),
        this->clients.end());

    // And check if simulation must be resumed because of the client we removed
    this->check_run();
}
