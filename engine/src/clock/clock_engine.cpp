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


#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <vp/vp.hpp>
#include <stdio.h>
#include "string.h"
#include <iostream>
#include <sstream>
#include <string>
#include <dlfcn.h>
#include <algorithm>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <regex>
#include <gv/gvsoc_proxy.hpp>
#include <gv/gvsoc.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <vp/time/time_scheduler.hpp>
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <sys/stat.h>

vp::clock_event *vp::clock_engine::enable(vp::clock_event *event)
{
    if (!event->enqueued)
    {
        if (event->stall_cycle == -1)
        {
            // The event has been disabled but not yet removed by the engine.
            // Just cancel the removal.
            event->stall_cycle = 0;
            event->enqueued = true;
        }
        else
        {
            // That should not be needed but in practice, lots of models are pushing from one
            // clock engine to another without synchronizing, creating timing issues.
            // This probably comes from models manipulating 2 clock domains at the same time.
            if (unlikely(!this->is_running()))
            {
                this->sync();

                if (this->period != 0 && !this->permanent_first)
                {
                    this->enqueue_to_engine(this->stop_time + this->period);
                }
            }

            if (this->permanent_first)
            {
                this->permanent_first->prev = event;
            }
            event->next = this->permanent_first;
            event->prev = NULL;
            event->enqueued = true;
            event->cycle = -1;

            this->permanent_first = event;
        }
    }

    return event;
}


void vp::clock_engine::disable(vp::clock_event *event)
{
    if (event->enqueued)
    {
        // Since the event is enqueued in a list which may be being traveled, we cannot directly remove it.
        // Mark it as removed and the engine will take care of removing it.
        event->stall_cycle = -1;
        event->enqueued = false;
    }
}

bool vp::clock_engine::dequeue_from_engine()
{
    if (this->is_running() || !this->is_enqueued)
        return false;

    this->engine->dequeue(this);

    return true;
}

void vp::clock_engine::reenqueue_to_engine()
{
    this->engine->enqueue(this, this->next_event_time);
}

void vp::clock_engine::apply_frequency(int frequency)
{
    // Update the number of cycles so that we update the event cycle only on the cycles after the frequency change
    // TODO this is breaking benchmarks
    // this->update();

    if (frequency > 0)
    {
        bool reenqueue = this->dequeue_from_engine();
        int64_t period = this->period;

        this->freq = frequency;
        this->period = 1e12 / this->freq;

        if (reenqueue && period > 0)
        {
            int64_t cycles = (this->next_event_time - this->get_time()) / period;
            this->next_event_time = this->get_time() + cycles * this->period;
            this->reenqueue_to_engine();
        }
        else if (period == 0)
        {
            // Case where the clock engine was clock-gated
            // We need to reenqueue the engine in case it has pending events
            if (this->has_events())
            {
                // Compute the time of the next event based on the new frequency
                this->next_event_time = this->get_time() + (this->get_next_event()->get_cycle() - this->get_cycles()) * this->period;

                this->reenqueue_to_engine();
            }
        }
    }
    else if (frequency == 0)
    {
        this->dequeue_from_engine();
        this->period = 0;
    }
}

void vp::clock_engine::update()
{
    if (this->period == 0)
        return;

#ifdef __VP_USE_SYSTEMC
    if ((int64_t)sc_time_stamp().to_double() > this->get_time())
        diff = (int64_t)sc_time_stamp().to_double() - this->stop_time;

    engine->update((int64_t)sc_time_stamp().to_double());
#endif

    if (this->stop_time + this->period <= this->get_time())
    {
        int64_t diff = this->get_time() - this->stop_time;
        int64_t cycles = diff / this->period;
        this->stop_time += cycles * this->period;
        this->cycles += cycles;
    }
}

vp::clock_event *vp::clock_engine::enqueue(vp::clock_event *event, int64_t cycle)
{
    vp_assert(!event->enqueued, 0, "Enqueueing already enqueued event\n");
    // vp_assert(cycles > 0, 0, "Enqueueing event with 0 or negative cycles\n");

    event->enqueued = true;

    // That should not be needed but in practice, lots of models are pushing from one
    // clock engine to another without synchronizing, creating timing issues.
    // This probably comes from models manipulating 2 clock domains at the same time.
    if (unlikely(!this->is_running()))
    {
        this->sync();

        if (this->period != 0 && !this->permanent_first)
        {
            enqueue_to_engine(this->stop_time + cycle * period);
        }
    }

    vp::clock_event *current = delayed_queue, *prev = NULL;
    int64_t full_cycle = cycle + get_cycles();

    while (current && current->cycle < full_cycle)
    {
        prev = current;
        current = current->next;
    }
    if (prev)
        prev->next = event;
    else
        delayed_queue = event;
    event->next = current;
    event->cycle = full_cycle;

    return event;
}

vp::clock_event *vp::clock_engine::get_next_event()
{
    // There is no quick way of getting the next event.
    // We have to first check if there is an event in the circular buffer
    // and if not in the delayed queue

    if (this->permanent_first)
    {
        return this->permanent_first;
    }

    if (this->nb_enqueued_to_cycle)
    {
        for (int i = 0; i < CLOCK_EVENT_QUEUE_SIZE; i++)
        {
            int cycle = (current_cycle + i) & CLOCK_EVENT_QUEUE_MASK;
            vp::clock_event *event = event_queue[cycle];
            if (event)
            {
                return event;
            }
        }
        // vp_assert(false, 0, "Didn't find any event in circular buffer while it is not empty\n");
    }

    return this->delayed_queue;
}

void vp::clock_engine::cancel(vp::clock_event *event)
{
    if (!event->is_enqueued())
        return;

    // There is no way to know if the event is enqueued into the circular buffer
    // or in the delayed queue so first go through the delayed queue and if it is
    // not found, look in the circular buffer

    // First the delayed queue
    vp::clock_event *current = delayed_queue, *prev = NULL;
    while (current)
    {
        if (current == event)
        {
            if (prev)
                prev->next = event->next;
            else
                delayed_queue = event->next;

            goto end;
        }

        prev = current;
        current = current->next;
    }

    // Then in the circular buffer
    for (int i = 0; i < CLOCK_EVENT_QUEUE_SIZE; i++)
    {
        vp::clock_event *current = event_queue[i], *prev = NULL;
        while (current)
        {
            if (current == event)
            {
                if (prev)
                    prev->next = event->next;
                else
                    event_queue[i] = event->next;

                this->nb_enqueued_to_cycle--;

                goto end;
            }

            prev = current;
            current = current->next;
        }
    }

    // TODO some models are pushing events to a different one that that they belong to
    //vp_assert(0, NULL, "Didn't find event in any queue while canceling event\n");

end:
    event->enqueued = false;

    if (!this->has_events())
        this->dequeue_from_engine();
}

int64_t vp::clock_engine::exec()
{
    vp_assert(this->has_events(), NULL, "Executing clock engine while it has no event\n");
    vp_assert(this->get_next_event(), NULL, "Executing clock engine while it has no next event\n");

    this->cycles_trace.event_real(this->cycles);

    vp_assert(this->get_next_event(), NULL, "Executing clock engine while it has no next event\n");

    clock_event *current = this->permanent_first;

    if (likely(current != NULL))
    {
        this->cycles++;

        while(likely(current != NULL))
        {
            clock_event *next = current->next;
            if (likely(current->stall_cycle == 0))
            {
                current->meth(current->_this, current);
            }
            else
            {
                if (current->stall_cycle == -1)
                {
                    current->stall_cycle = 0;

                    if (current->prev)
                    {
                        current->prev->next = current->next;
                    }
                    else
                    {
                        this->permanent_first = current->next;
                    }

                    if (current->next)
                    {
                        current->next->prev = current->prev;
                    }
                }
                else
                {
                    current->stall_cycle--;
                }
            }
            current = next;
        }
    }
    else
    {
        this->cycles = delayed_queue->cycle;
    }

    while (delayed_queue)
    {
        if (delayed_queue->cycle > this->get_cycles())
        {
            break;
        }

        clock_event *current = delayed_queue;
        current->enqueued = false;
        delayed_queue = delayed_queue->next;
        current->meth(current->_this, current);
    }

    if (likely(this->permanent_first != NULL))
    {
        return period;
    }
    else
    {

        // Also remember the current time in order to resynchronize the clock engine
        // in case we enqueue and event from another engine.
        this->stop_time = this->get_time();

        if (delayed_queue)
        {
            int64_t cycle_diff = delayed_queue->cycle - get_cycles();
            int64_t time_diff = cycle_diff * period;
            return time_diff;
        }
        else
        {
            return -1;
        }
    }
}


vp::clock_event *vp::clock_engine::reenqueue(vp::clock_event *event, int64_t enqueue_cycles)
{
  if (event->is_enqueued())
  {
    cancel(event);
  }

  enqueue(event, enqueue_cycles);

  return event;
}



vp::clock_event *vp::clock_engine::reenqueue_ext(vp::clock_event *event, int64_t enqueue_cycles)
{
  this->sync();
  return this->reenqueue(event, enqueue_cycles);
}
