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
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    top->new_service("time", static_cast<time_engine *>(this));
}



int64_t vp::time_engine::exec(int64_t end_time)
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
                    if (likely(time <= end_time && !this->stop_req))
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

            if (!current || current->next_event_time > end_time || this->stop_req)
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

    if (this->stop_req)
    {
        pthread_mutex_lock(&mutex);
        this->stop_req = false;

        if (this->lock_req)
        {
            this->locked = true;
            pthread_cond_broadcast(&cond);

            while (locked)
            {
                pthread_cond_wait(&cond, &mutex);
            }
        }

        pthread_mutex_unlock(&mutex);
    }

    return time;
}



void vp::time_engine::wait_for_lock()
{
    pthread_mutex_lock(&mutex);

    if (this->lock_req)
    {
        this->locked = true;
        pthread_cond_broadcast(&cond);

        while (locked)
        {
            pthread_cond_wait(&cond, &mutex);
        }
    }
    else
    {
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex);
}

void vp::time_engine::wait_for_lock_stop()
{
    pthread_cond_broadcast(&cond);
}

int64_t vp::time_engine::run_until(int64_t end_time)
{
    int64_t time;

    while(1)
    {
        time = this->exec(end_time);

        if (time >= end_time || time == -1 || this->finished)
        {
            if (this->finished)
            {
                return end_time;
            }
            break;
        }
    }

    return time;
}



int64_t vp::time_engine::run()
{
    int64_t time;

    while(1)
    {
        time = this->exec(INT64_MAX);

        if (time == -1)
        {
            this->wait_for_lock();
        }

        if (this->pause_req || this->finished)
        {
            this->pause_req = false;
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
