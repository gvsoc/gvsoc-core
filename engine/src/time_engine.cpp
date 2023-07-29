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

#include <vp/vp.hpp>
#include "vp/time/time_engine.hpp"
#include "vp/time/time_scheduler.hpp"
#include <pthread.h>
#include <signal.h>

static pthread_t sigint_thread;

vp::time_domain::time_domain(vp::component *top, js::config *config)
    : vp::time_engine(top, config)
{
    top->new_service("time", static_cast<time_engine *>(this));

    this->is_async = top->get_gv_conf()->is_async;
}

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
static void *signal_routine(void *arg)
{
    vp::time_engine *engine = (vp::time_engine *)arg;
    sigset_t sigs_to_catch;
    int caught;
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGINT);
    do
    {
        sigwait(&sigs_to_catch, &caught);
        engine->stop_engine(-1, true);
    } while (1);
    return NULL;
}

// Routine executed by the thread running the global time engine.
// Just switch to C++ world.

void set_sc_main_entry(void *(*entry)(void *), void *arg);

static void *engine_routine(void *arg)
{
    vp::time_engine *engine = (vp::time_engine *)arg;
    // Create the sigint thread so that we can properly close simulation
    // in case ctrl C is hit.
    sigset_t sigs_to_block;
    sigemptyset(&sigs_to_block);
    sigaddset(&sigs_to_block, SIGINT);
    pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
    pthread_create(&sigint_thread, NULL, signal_routine, (void *)engine);

    signal(SIGINT, sigint_handler);

    engine->run_loop();
    return NULL;
}

vp::time_engine::time_engine(vp::component *top, js::config *config)
    : first_client(NULL), top(top), config(config)
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_lock(&mutex);

    run_req = false;
    stop_req = false;
    pause_req = false;
}



// This is called by the python thread once he wants to start the time engine.
// This for now just takes care of stopping the engine when it is asked
// and in this case returns to python world so that everything is closed.
// This function can be called several time in case the python world decides
// to continue running the engine.
void vp::time_engine::run()
{
    if (this->is_async)
    {
        pthread_mutex_lock(&mutex);

        while (locked)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        run_req = true;
        pause_req = false;
        pthread_cond_broadcast(&cond);

        pthread_mutex_unlock(&mutex);
    }
    else
    {
        this->exec();
    }
}



void vp::time_engine::quit(int status)
{
    this->get_time_engine()->lock();
    this->stop_engine(status, true, true);
    this->get_time_engine()->unlock();
}


int vp::time_engine::join()
{
    if (this->is_async)
    {
        int result = -1;

        pthread_mutex_lock(&mutex);

        // Wait until we get a stop request
        while (!stop_req && !finished)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        // This has been commented out as the engine is stuck if the simulation ends
        // because there is no more events
        // if (finished)
        //     goto end;

        // In case we get a stop request, first try to kindly stop the engine.
        // Then if it is still running after 100ms, we kill it. This can happen
        // because this is a cooperative engine.
        // if (stop_req)
        {
            if (running)
            {
                pthread_cond_broadcast(&cond);

                int rc = 0;
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 1;

                while (running && rc == 0)
                {
                    rc = pthread_cond_timedwait(&cond, &mutex, &ts);
                }

                if (running)
                {
                    running = false;
                    pthread_cancel(run_thread);
                    stop_status = -1;
                }
            }
        }

        result = stop_status;

        pthread_mutex_unlock(&mutex);

    end:
        return result;
    }
    else
    {
        while(run_req)
        {
            this->exec();
        }
        return stop_status;
    }
}



void vp::time_engine::start()
{
    js::config *item_conf = this->config->get("**/no_exit");
    bool sa_mode = this->config->get_child_bool("**/sa-mode");
    this->no_exit = item_conf != NULL && item_conf->get_bool();

    if (this->no_exit)
    {
        // In case the vp is connected to an external bridge, prevent the platform
        // from exiting in case there is no more events.
        retain_count++;
    }

    this->stop_event = new vp::Time_engine_stop_event(this->top, this);

    if (this->is_async)
    {
        if (sa_mode)
        {
            pthread_create(&run_thread, NULL, engine_routine, (void *)this);
        }
    }
    else
    {
        this->run_req = true;
        pthread_mutex_unlock(&mutex);
    }
}

void vp::time_engine::wait_ready()
{
    while (!first_client)
    {
    }
}



vp::Time_engine_stop_event::Time_engine_stop_event(vp::component *top, time_engine *engine) : vp::time_scheduler(NULL), top(top), engine(engine)
{
    this->engine = (vp::time_engine*)top->get_service("time");

    this->build_instance("stop_event", top);
}

int64_t vp::Time_engine_stop_event::step(int64_t duration)
{
    vp::time_event *event = this->time_event_new(this->event_handler);
    this->enqueue(event, duration);
    return 0;
}

void vp::Time_engine_stop_event::event_handler(void *__this, vp::time_event *event)
{
    Time_engine_stop_event *_this = (Time_engine_stop_event *)__this;
    _this->engine->stop_exec();
    _this->time_event_del(event);
}


int64_t vp::time_engine::step(int64_t duration)
{
    if (this->is_async)
    {
        int64_t timestamp;
        this->get_time_engine()->lock();
        timestamp = this->get_time();
        if (duration > 0)
        {
            this->stop_event->step(duration);
        }
        this->get_time_engine()->unlock();
        this->run();
        return timestamp + duration;
    }
    else
    {
        if (duration == 0)
        {
            this->exec();
            return this->get_time();
        }
        else
        {
            int64_t end_time = this->get_time() + duration;
            this->step_until(end_time);
            return end_time;
        }
    }
}


void vp::time_engine::exec()
{
    time_engine_client *current = first_client;

    if (current)
    {
        first_client = current->next;
        current->is_enqueued = false;

        // Update the global engine time with the current event time
        this->time = current->next_event_time;

        while (1)
        {
            current->running = true;

            int64_t time = current->exec();

            time_engine_client *next = first_client;

            // Shortcut to quickly continue with the same client
            if (likely(time > 0))
            {
                time += this->time;
                if (likely((!next || next->next_event_time >= time)))
                {
                    if (likely(run_req))
                    {
                        this->time = time;
                        continue;
                    }
                    else
                    {
                        current->next = first_client;
                        first_client = current;
                        current->next_event_time = time;
                        current->is_enqueued = true;
                        current->running = false;
                        break;
                    }
                }
            }

            // Otherwise remove it, reenqueue it and continue with the next one.
            // We can optimize a bit the operation as we already know
            // who to schedule next.

            if (time > 0)
            {
                current->next_event_time = time;
                time_engine_client *client = next->next, *prev = next;
                while (client && client->next_event_time < time)
                {
                    prev = client;
                    client = client->next;
                }
                current->next = client;
                prev->next = current;
                current->is_enqueued = true;
            }

            current->running = false;

            if (!run_req)
                break;

            current = first_client;
            if (current)
            {
                vp_assert(first_client->next_event_time >= get_time(), NULL, "event time is before vp time\n");

                first_client = current->next;
                current->is_enqueued = false;
            }

            if (!current)
                break;

            // Update the global engine time with the current event time
            this->time = current->next_event_time;
        }
    }
}


int64_t vp::time_engine::step_until(int64_t end_time)
{

    time_engine_client *current = first_client;

    if (current && current->next_event_time <= end_time)
    {
        first_client = current->next;
        current->is_enqueued = false;

        // Update the global engine time with the current event time
        this->time = current->next_event_time;

        while (1)
        {
            current->running = true;

            int64_t time = current->exec();

            time_engine_client *next = first_client;

            // Shortcut to quickly continue with the same client
            if (likely(time > 0))
            {
                time += this->time;
                if (likely((!next || next->next_event_time > time)))
                {
                    if (likely(time < end_time && run_req))
                    {
                        this->time = time;
                        continue;
                    }
                    else
                    {
                        current->next = first_client;
                        first_client = current;
                        current->next_event_time = time;
                        current->is_enqueued = true;
                        current->running = false;
                        break;
                    }
                }
            }

            // Otherwise remove it, reenqueue it and continue with the next one.
            // We can optimize a bit the operation as we already know
            // who to schedule next.

            if (time > 0)
            {
                current->next_event_time = time;
                time_engine_client *client = next->next, *prev = next;
                while (client && client->next_event_time < time)
                {
                    prev = client;
                    client = client->next;
                }
                current->next = client;
                prev->next = current;
                current->is_enqueued = true;
            }

            current->running = false;

            if (time > end_time || !run_req)
                break;

            current = first_client;
            if (current)
            {
                vp_assert(first_client->next_event_time >= get_time(), NULL, "event time is before vp time\n");

                first_client = current->next;
                current->is_enqueued = false;
            }

            if (!current)
                break;

            // Update the global engine time with the current event time
            this->time = current->next_event_time;
        }
    }

    if (this->first_client)
    {
        return this->first_client->next_event_time;
    }

    return -1;
}



void vp::time_engine::run_loop()
{
    pthread_mutex_unlock(&mutex);

    while (1)
    {
        pthread_mutex_lock(&mutex);

        // Wait here until we are asked to run and there is no pause request
        while (!run_req || pause_req)
        {
            if (pause_req)
            {
                for (auto x: this->exec_notifiers)
                {
                    x->notify_stop();
                }
            }
            running = false;
            pthread_cond_broadcast(&cond);
            pthread_cond_wait(&cond, &mutex);
        }
        running = true;

        for (auto x: this->exec_notifiers)
        {
            x->notify_run();
        }

        pthread_mutex_unlock(&mutex);





        time_engine_client *current = first_client;

        if (current)
        {
            first_client = current->next;
            current->is_enqueued = false;

            // Update the global engine time with the current event time
            this->time = current->next_event_time;

            while (1)
            {
                current->running = true;

                int64_t time = current->exec();

                time_engine_client *next = first_client;

                // Shortcut to quickly continue with the same client
                if (likely(time > 0))
                {
                    time += this->time;
                    if (likely((!next || next->next_event_time >= time)))
                    {
                        if (likely(run_req))
                        {
                            this->time = time;
                            continue;
                        }
                        else
                        {
                            current->next = first_client;
                            first_client = current;
                            current->next_event_time = time;
                            current->is_enqueued = true;
                            current->running = false;
                            break;
                        }
                    }
                }

                // Otherwise remove it, reenqueue it and continue with the next one.
                // We can optimize a bit the operation as we already know
                // who to schedule next.

                if (time > 0)
                {
                    current->next_event_time = time;
                    time_engine_client *client = next->next, *prev = next;
                    while (client && client->next_event_time < time)
                    {
                        prev = client;
                        client = client->next;
                    }
                    current->next = client;
                    prev->next = current;
                    current->is_enqueued = true;
                }

                current->running = false;

                if (!run_req)
                    break;

                current = first_client;
                if (current)
                {
                    vp_assert(first_client->next_event_time >= get_time(), NULL, "event time is before vp time\n");

                    first_client = current->next;
                    current->is_enqueued = false;
                }

                if (!current)
                    break;

                // Update the global engine time with the current event time
                this->time = current->next_event_time;
            }
        }

        pthread_mutex_lock(&mutex);

        running = false;

        while (!first_client && retain_count && !locked)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        current = first_client;

        if (first_client == NULL && !locked && !retain_count)
        {
            finished = true;
        }
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
}






void vp::time_engine::req_stop_exec()
{
    this->pause();
    this->get_time_engine()->lock();
    this->flush_all();
    fflush(NULL);
    this->get_time_engine()->unlock();
}

void vp::time_engine::register_exec_notifier(Notifier *notifier)
{
    this->exec_notifiers.push_back(notifier);
}

void vp::time_engine::stop_exec()
{
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    this->pause_req = true;
    this->run_req = false;
    pthread_mutex_unlock(&mutex);
}

void vp::time_engine::wait_stopped()
{
    pthread_mutex_lock(&mutex);
    while(run_req)
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}


void vp::time_engine::bind_to_launcher(gv::Gvsoc_user *launcher)
{
    this->launcher = launcher;
}

static void init_sigint_handler(int s)
{
    raise(SIGTERM);
}
