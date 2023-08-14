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



vp::time_engine::time_engine(vp::component *top, js::config *config)
    : first_client(NULL), top(top), config(config)
{
    pthread_mutex_init(&lock_mutex, NULL);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    top->new_service("time", static_cast<time_engine *>(this));

    this->stop_event = new vp::Time_engine_stop_event(this->top, this);
}



int64_t vp::time_engine::exec()
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
                if (likely((!next || next->next_event_time > time)))
                {
                    if (likely(!this->stop_req))
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

            current = first_client;

            // Leave the loop either if there is no more client to schedule or if there is a stop request.
            // In case of a stop request, always take it into account when time is increased so that teh engine
            // is stopped at the end of the current timestamp. This will ensure the step operation, which is using a
            // stop event, is stepping until the end of the timestamp.
            if (!current || (this->stop_req && current->next_event_time > this->time))
            {
                break;
            }

            vp_assert(first_client->next_event_time >= get_time(), NULL, "event time is before vp time\n");

            first_client = current->next;
            current->is_enqueued = false;

            // Update the global engine time with the current event time
            this->time = current->next_event_time;
        }
    }

    int64_t time = -1;

    if (this->first_client)
    {
        time = this->first_client->next_event_time;
    }

    return time;
}



void vp::time_engine::handle_locks()
{
    while (this->lock_req > 0)
    {
        pthread_cond_wait(&cond, &mutex);
    }
}

void vp::time_engine::step_register(int64_t end_time)
{

    this->stop_event->step(end_time);
}



int64_t vp::time_engine::run()
{
    int64_t time;

    while(1)
    {
        // Cancel any pause request which was done before running
        this->pause_req = false;

        time = this->exec();

        // Cancel now the requests that may have stopped us so that anyone can stop us again
        // when locks are handled.
        this->stop_req = false;

        // In case there is no more event, stall the engine until something happens.
        if (time == -1)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        // Checks locks since we may have been stopped by them
        this->handle_locks();

        // In case of a pause request or the simulation is finished, we leave the engine to let
        // the launcher handles it, otherwise we just continue to run events
        if (this->pause_req || this->finished)
        {
            this->pause_req = false;
            time = -1;
            break;
        }
    }

    return time;
}



void vp::time_engine::bind_to_launcher(gv::Gvsoc_user *launcher)
{
    this->launcher = launcher;
}



bool vp::time_engine::dequeue(time_engine_client *client)
{
    if (!client->is_enqueued)
        return false;

    client->is_enqueued = false;

    time_engine_client *current = this->first_client, *prev = NULL;
    while (current && current != client)
    {
        prev = current;
        current = current->next;
    }
    if (prev)
        prev->next = client->next;
    else
        this->first_client = client->next;

    return true;
}



bool vp::time_engine::enqueue(time_engine_client *client, int64_t full_time)
{
    bool update = false;

    vp_assert(full_time >= get_time(), NULL, "Time must be higher than current time\n");

    if (client->is_running())
        return false;

    if (client->is_enqueued)
    {
        if (client->next_event_time <= full_time)
            return false;
        this->dequeue(client);
    }

    client->is_enqueued = true;

    time_engine_client *current = first_client, *prev = NULL;
    client->next_event_time = full_time;
    while (current && current->next_event_time < client->next_event_time)
    {
        prev = current;
        current = current->next;
    }
    if (prev)
        prev->next = client;
    else
    {
        update = true;
        first_client = client;
    }
    client->next = current;

    if (update && this->launcher)
    {
        this->launcher->was_updated();
    }

    return true;
}



void vp::time_engine::quit(int status)
{
    this->pause();
    this->stop_status = status;
    this->finished = true;

    // Notify the condition in case we are waiting for locks, to allow leaving the engine.
    pthread_cond_broadcast(&cond);
}



void vp::time_engine::pause()
{
    this->stop_req = true;
    this->pause_req = true;

    // Notify the condition in case we are waiting for locks, to allow leaving the engine.
    pthread_cond_broadcast(&cond);
}



void vp::time_engine::flush()
{
    this->top->flush_all();
    fflush(NULL);
}


void vp::time_engine::fatal(const char *fmt, ...)
{
    fprintf(stdout, "[\033[31mFATAL\033[0m] ");
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(stdout, fmt, ap) < 0)
    {
    }
    va_end(ap);
    this->quit(-1);
}

vp::Time_engine_stop_event::Time_engine_stop_event(vp::component *top, vp::time_engine *engine) : vp::time_scheduler(NULL), top(top)
{
    this->engine = engine;

    this->build_instance("stop_event", top);
}

int64_t vp::Time_engine_stop_event::step(int64_t time)
{
    vp::time_event *event = this->time_event_new(this->event_handler);
    this->enqueue(event, time - this->engine->get_time());
    return 0;
}

void vp::Time_engine_stop_event::event_handler(void *__this, vp::time_event *event)
{
    Time_engine_stop_event *_this = (Time_engine_stop_event *)__this;
    _this->engine->pause();
    _this->time_event_del(event);
}
