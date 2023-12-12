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
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <sys/stat.h>

vp::ClockEvent *vp::ClockEngine::enable(vp::ClockEvent *event)
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
            if (unlikely(!this->time.is_running()))
            {
                this->sync();

                if (this->period != 0 && !this->permanent_first)
                {
                    this->time.enqueue_to_engine(this->stop_time + this->period);
                }
            }

            if (this->permanent_first)
            {
                event->next = this->permanent_first;
                event->prev = this->permanent_first->prev;
                this->permanent_first->prev->next = event;
                this->permanent_first->prev = event;
            }
            else
            {
                event->next = event;
                event->prev = event;
            }
            event->enqueued = true;
            event->cycle = -1;

            this->permanent_first = event;
        }
    }

    return event;
}

void vp::ClockEngine::disable(vp::ClockEvent *event)
{
    if (event->enqueued)
    {
        // Since the event is enqueued in a list which may be being browsed, we cannot directly
        // remove it. Mark it as removed and the engine will take care of removing it.
        event->stall_cycle = -1;
        event->enqueued = false;
    }
}

bool vp::ClockEngine::dequeue_from_engine()
{
    if (this->time.is_running() || !this->time.get_is_enqueued())
        return false;

    this->time_engine->dequeue(this);

    return true;
}

void vp::ClockEngine::reenqueue_to_engine()
{
    this->time_engine->enqueue(this, this->time.next_event_time);
}

void vp::ClockEngine::apply_frequency(int frequency)
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
            int64_t cycles = (this->time.next_event_time - this->time.get_time()) / period;
            this->time.next_event_time = this->time.get_time() + cycles * this->period;
            this->reenqueue_to_engine();
        }
        else if (period == 0)
        {
            // Case where the clock engine was clock-gated
            // We need to reenqueue the engine in case it has pending events
            if (this->has_events())
            {
                // Compute the time of the next event based on the new frequency
                this->time.next_event_time = this->time.get_time() + (this->get_next_event()->get_cycle() - this->get_cycles()) * this->period;

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

void vp::ClockEngine::update()
{
    if (this->period == 0)
        return;

    if (this->stop_time + this->period <= this->time.get_time())
    {
        int64_t diff = this->time.get_time() - this->stop_time;
        int64_t cycles = diff / this->period;
        this->stop_time += cycles * this->period;
        this->cycles += cycles;
    }
}

vp::ClockEvent *vp::ClockEngine::enqueue(vp::ClockEvent *event, int64_t cycle)
{
    vp_assert(!event->enqueued, 0, "Enqueueing already enqueued event\n");
    // vp_assert(cycles > 0, 0, "Enqueueing event with 0 or negative cycles\n");

    event->enqueued = true;

    // That should not be needed but in practice, lots of models are pushing from one
    // clock engine to another without synchronizing, creating timing issues.
    // This probably comes from models manipulating 2 clock domains at the same time.
    if (unlikely(!this->time.is_running()))
    {
        this->sync();

        if (this->period != 0 && !this->permanent_first)
        {
            time.enqueue_to_engine(this->stop_time + cycle * period);
        }
    }

    vp::ClockEvent *current = delayed_queue, *prev = NULL;

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

vp::ClockEvent *vp::ClockEngine::get_next_event()
{
    // There is no quick way of getting the next event.
    // We have to first check if there is an event in the circular buffer
    // and if not in the delayed queue

    if (this->permanent_first)
    {
        return this->permanent_first;
    }

    return this->delayed_queue;
}

void vp::ClockEngine::cancel(vp::ClockEvent *event)
{
    if (!event->is_enqueued())
        return;

    // There is no way to know if the event is enqueued into the circular buffer
    // or in the delayed queue so first go through the delayed queue and if it is
    // not found, look in the circular buffer

    // First the delayed queue
    vp::ClockEvent *current = delayed_queue, *prev = NULL;
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

    event->disable();

    // TODO some models are pushing events to a different one that that they belong to
    // vp_assert(0, NULL, "Didn't find event in any queue while canceling event\n");

end:
    event->enqueued = false;

    if (!this->has_events())
        this->dequeue_from_engine();
}

int64_t vp::ClockEngine::exec()
{
    vp_assert(this->has_events(), NULL, "Executing clock engine while it has no event\n");
    vp_assert(this->get_next_event(), NULL, "Executing clock engine while it has no next event\n");

    this->cycles_trace.event_real(this->cycles);

    vp_assert(this->get_next_event(), NULL, "Executing clock engine while it has no next event\n");

    ClockEvent *current = this->permanent_first;

    if (likely(current != NULL))
    {
        this->cycles++;

        do
        {
            ClockEvent *next = current->next;
            if (likely(current->stall_cycle == 0))
            {
                current->meth(current->_this, current);
            }
            else
            {
                if (current->stall_cycle == -1)
                {
                    // Case where the event has been disabled. The disable was just flagged so
                    // that we can propertly remove it from here.
                    current->stall_cycle = 0;

                    current->prev->next = current->next;
                    current->next->prev = current->prev;

                    if (this->permanent_first == current)
                    {
                        if (current->next == current)
                        {
                            this->permanent_first = NULL;
                            break;
                        }
                        else
                        {
                            this->permanent_first = current->next;
                        }
                    }
                }
                else
                {
                    current->stall_cycle--;
                }
            }
            current = next;
        } while (likely(current != this->permanent_first));

        if (this->permanent_first)
        {
            this->permanent_first = this->permanent_first->next;
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

        ClockEvent *current = delayed_queue;
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
        this->stop_time = this->time.get_time();

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

vp::ClockEvent *vp::ClockEngine::reenqueue(vp::ClockEvent *event, int64_t enqueue_cycles)
{
    if (event->is_enqueued())
    {
        cancel(event);
    }

    enqueue(event, enqueue_cycles);

    return event;
}

vp::ClockEvent *vp::ClockEngine::reenqueue_ext(vp::ClockEvent *event, int64_t enqueue_cycles)
{
    this->sync();
    return this->reenqueue(event, enqueue_cycles);
}

void vp::ClockEngine::set_frequency(vp::Block *__this, int64_t frequency)
{
    ClockEngine *_this = (ClockEngine *)__this;
    _this->out.set_frequency(frequency);
    _this->apply_frequency(frequency * _this->factor);
    _this->clock_trace.event_real(_this->period);
}

void vp::ClockEngine::pre_start()
{
    out.reg(this);
}

vp::ClockEngine::ClockEngine(vp::ComponentConf &config)
    : vp::Component(config), cycles(0), period(0), freq(0)
{
    this->time_engine = config.time_engine;
    delayed_queue = NULL;
    current_cycle = 0;

    this->apply_frequency(get_js_config()->get_child_int("frequency"));

    new_master_port("out", &out);

    clock_in.set_set_frequency_meth(&ClockEngine::set_frequency);
    new_slave_port("clock_in", &clock_in);

    this->traces.new_trace_event_real("period", &this->clock_trace);

    this->traces.new_trace_event_real("cycles", &this->cycles_trace);

    this->factor = this->get_js_config()->get_child_int("factor");

    if (this->factor == 0)
    {
        this->factor = 1;
    }
}
