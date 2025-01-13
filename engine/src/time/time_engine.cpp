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
#include "vp/launcher.hpp"
#include <vp/time/time_event.hpp>


vp::TimeEngine::TimeEngine(js::Config *config)
    : first_client(NULL)
{
}

void vp::TimeEngine::init(vp::Component *top)
{
    this->top = top;
}

int64_t vp::TimeEngine::exec()
{
    vp::Block *current = this->first_client;

    if (current)
    {
        first_client = current->time.next;
        current->time.is_enqueued = false;

        // Update the global engine time with the current event time
        this->time = current->time.next_event_time;

        while (1)
        {
            current->time.running = true;

            int64_t time = current->exec();

            vp::Block *next = this->first_client;

            // Shortcut to quickly continue with the same client
            if (likely(time > 0))
            {
                time += this->time;
                if (likely((!next || next->time.next_event_time > time)))
                {
                    if (likely(!this->stop_req))
                    {
                        this->time = time;
                        continue;
                    }
                    else
                    {
                        this->stop_req = false;
                        current->time.next = first_client;
                        first_client = current;
                        current->time.next_event_time = time;
                        current->time.is_enqueued = true;
                        current->time.running = false;
                        break;
                    }
                }
            }

            // Otherwise remove it, reenqueue it and continue with the next one.
            // We can optimize a bit the operation as we already know
            // who to schedule next.

            if (time > 0)
            {
                current->time.next_event_time = time;
                vp::Block *client = next->time.next, *prev = next;
                while (client && client->time.next_event_time < time)
                {
                    prev = client;
                    client = client->time.next;
                }
                current->time.next = client;
                prev->time.next = current;
                current->time.is_enqueued = true;
            }

            current->time.running = false;

            current = first_client;

            // Leave the loop either if there is no more client to schedule or if there is a stop request.
            // In case of a stop request, always take it into account when time is increased so that teh engine
            // is stopped at the end of the current timestamp. This will ensure the step operation, which is using a
            // stop event, is stepping until the end of the timestamp.
            if (!current || (this->stop_req && current->time.next_event_time > this->time))
            {
                this->stop_req = false;
                break;
            }

            vp_assert(first_client->time.next_event_time >= get_time(), NULL, "event time is before vp time\n");

            first_client = current->time.next;
            current->time.is_enqueued = false;

            // Update the global engine time with the current event time
            this->time = current->time.next_event_time;
        }
    }

    int64_t time = -1;

    if (this->first_client)
    {
        time = this->first_client->time.next_event_time;
    }

    return time;
}

int64_t vp::TimeEngine::run()
{
    return this->exec();
}

void vp::TimeEngine::bind_to_launcher(gv::Gvsoc_user *launcher)
{
    this->launcher = launcher;
}

bool vp::TimeEngine::dequeue(vp::Block *client)
{
    if (!client->time.is_enqueued)
        return false;

    client->time.is_enqueued = false;

    vp::Block *current = this->first_client, *prev = NULL;
    while (current && current != client)
    {
        prev = current;
        current = current->time.next;
    }
    if (prev)
        prev->time.next = client->time.next;
    else
        this->first_client = client->time.next;

    return true;
}

bool vp::TimeEngine::enqueue(vp::Block *client, int64_t full_time)
{
    bool update = false;

    vp_assert(full_time >= get_time(), NULL, "Time must be higher than current time\n");

    if (client->time.is_running())
        return false;

    if (client->time.is_enqueued)
    {
        if (client->time.next_event_time <= full_time)
            return false;
        this->dequeue(client);
    }

    client->time.is_enqueued = true;

    vp::Block *current = first_client, *prev = NULL;
    client->time.next_event_time = full_time;
    while (current && current->time.next_event_time < client->time.next_event_time)
    {
        prev = current;
        current = current->time.next;
    }
    if (prev)
        prev->time.next = client;
    else
    {
        update = true;
        first_client = client;
    }
    client->time.next = current;

    if (update && this->launcher)
    {
        this->launcher->was_updated();
    }

    return true;
}

void vp::TimeEngine::quit(int status)
{
    this->pause();
    this->stop_status = status;
    this->finished = true;
}

void vp::TimeEngine::pause()
{
    this->stop_req = true;
}

// TODO shoud be moved to Top class
void vp::TimeEngine::flush()
{
    this->top->flush_all();
}

void vp::TimeEngine::fatal(const char *fmt, ...)
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


void vp::TimeEngine::flush_all()
{
    this->top->flush_all();
}
