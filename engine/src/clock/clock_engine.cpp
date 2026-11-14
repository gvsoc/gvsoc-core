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
#include <vp/stats/stats_engine.hpp>
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
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <sys/stat.h>

vp::ClockEvent *vp::ClockEngine::enable(vp::ClockEvent *event)
{
    if (!event->enqueued)
    {
        if (event->pending_disable)
        {
            // The event has been disabled but not yet removed by the engine.
            // Just cancel the removal.
            event->pending_disable = false;
            event->enqueued = true;
            if (event->stall_cycle == 0)
            {
                event->meth = event->meth_saved;
                event->meth_saved = NULL;
            }
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
                this->permanent_first->prev = event;
            }
            else
            {
                this->permanent_last = event;
            }
            event->next = this->permanent_first;
            event->prev = NULL;
            this->permanent_first = event;

            event->enqueued = true;
            event->cycle = -1;
        }
    }

    return event;
}

void vp::ClockEngine::stalled_event_handler(vp::Block *__this, ClockEvent *event)
{
    vp::ClockEngine *_this = (vp::ClockEngine *)event->clock;

    if (event->pending_disable)
    {
        // Case where the event has been disabled. The disable was just flagged so
        // that we can propertly remove it from here.
        event->pending_disable = false;

        if (event->stall_cycle == 0)
        {
            event->meth = event->meth_saved;
            event->meth_saved = NULL;
        }

        if (event->prev)
        {
            event->prev->next = event->next;
        }
        if (event->next)
        {
            event->next->prev = event->prev;
        }

        if (_this->permanent_first == event)
        {
            _this->permanent_first = event->next;
        }

        if (_this->permanent_last == event)
        {
            _this->permanent_last = event->prev;
        }
    }
    else
    {
        event->stall_cycle--;

        if (event->stall_cycle == 0)
        {
            event->meth = event->meth_saved;
            event->meth_saved = NULL;
        }
    }
}

void vp::ClockEngine::disable(vp::ClockEvent *event)
{
    if (event->enqueued)
    {
        // Since the event is enqueued in a list which may be being browsed, we cannot directly
        // remove it. Mark it as removed and the engine will take care of removing it.
        event->pending_disable = true;
        event->enqueued = false;

        if (event->meth_saved == NULL)
        {
            event->meth_saved = event->meth;
            event->meth = &vp::ClockEngine::stalled_event_handler;
        }
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

void vp::ClockEngine::apply_frequency_handler(vp::Block *__this, vp::ClockEvent *event)
{
    vp::ClockEngine *_this = (vp::ClockEngine *)__this;

    bool enqueue_to_engine = _this->period == 0;

    _this->period = 1e12 / _this->frequency_to_be_applied;
    _this->change_frequency(_this->frequency_to_be_applied);

    if (enqueue_to_engine)
    {
        // Case where the clock engine was clock-gated
        // We need to reenqueue the engine in case it has pending events
        if (_this->has_events())
        {
            // Compute the time of the next event based on the new frequency
            _this->time.next_event_time = _this->time.get_time() + (_this->get_next_event()->get_cycle() - _this->get_cycles()) * _this->period;

            _this->reenqueue_to_engine();
        }
    }
}

void vp::ClockEngine::apply_frequency(int64_t frequency)
{
    // Update the number of cycles so that we update the event cycle only on the cycles after the frequency change
    this->update();

    if (frequency > 0)
    {
        // The frequency change must be applied carefully in order to keep plain cycles, as this
        // can be called from another clock engine in the middle of a cycle.
        // If the engine is currently clock-gated, we applied it immediately as the new cycle can
        // start immediately.
        // Otherwise, we delay the frequency change to the next cycle, by enqueueing an event, this
        // way the new frequency will be applied at the start of a cycle.
        this->frequency_to_be_applied = frequency;
        if (this->period == 0)
        {
            this->update();
            this->apply_frequency_handler(this, NULL);
        }
        else
        {
            this->enqueue(&this->apply_frequency_event, 1);
        }
    }
    else if (frequency == 0)
    {
        this->dequeue_from_engine();
        this->period = 0;
        this->change_frequency(0);
    }
}

void vp::ClockEngine::update()
{
    if (this->period == 0)
    {
        this->stop_time = this->time.get_time();
        return;
    }

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
    // That should not be needed but in practice, lots of models are pushing from one
    // clock engine to another without synchronizing, creating timing issues.
    // This probably comes from models manipulating 2 clock domains at the same time.
    if (unlikely(!this->time.is_running()))
    {
        this->sync();
    }

    int64_t full_cycle = cycle + get_cycles();

    if (unlikely(event->is_enqueued()))
    {
        // If the event is already enqueued, ignore the enqueue if the existing
        // cycle count is below the new one
        if (event->cycle <= full_cycle)
        {
            return event;
        }

        // Otherwise cancel it, so that we can enqueue it with new lower cycle count
        this->cancel(event);
    }

    vp_assert(!event->enqueued, 0, "Enqueueing already enqueued event\n");
    // vp_assert(cycles > 0, 0, "Enqueueing event with 0 or negative cycles\n");

    event->enqueued = true;
    event->clock = this;

    if (unlikely(!this->time.is_running()))
    {
        if (this->period != 0 && !this->permanent_first)
        {
            // The enqueue may come inline from another clock domain in the
            // middle of one of our cycles (stop_time is the last whole-cycle
            // boundary, which can be in the past): align the wake-up on the
            // next edge at or after the current time, as the hardware would
            // sample the request on the next edge of this clock.
            int64_t full_time = this->stop_time + cycle * period;
            int64_t now = this->time.get_time();
            if (unlikely(full_time < now))
            {
                full_time += (now - full_time + period - 1) / period * period;
            }
            time.enqueue_to_engine(full_time);
        }
    }

    vp::ClockEvent *current = delayed_queue, *prev = NULL;

    while (current && current->cycle <= full_cycle)
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

    this->next_delayed_cycle = this->delayed_queue->cycle;

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

    this->next_delayed_cycle = this->delayed_queue ? this->delayed_queue->cycle : INT64_MAX;

    if (!this->has_events())
        this->dequeue_from_engine();
}

void vp::ClockEngine::dump_traces()
{
#ifdef CONFIG_GVSOC_EVENT_ACTIVE
    // Go through the list of traces to be automatically flushed at beginning of next cycle,
    // flush them and clear the list
    vp::Event *event = this->trace_flush_head;
    this->trace_flush_head = NULL;
    uint64_t min_cyclestamp = UINT64_MAX;
    while (event)
    {
        vp::Event *next = event->next_get();
        if (event->dump_next_values())
        {
            event->next_set(this->trace_flush_head);
            this->trace_flush_head = event;
            if ((uint64_t)event->next_value_cyclestamp_get() < min_cyclestamp)
            {
                min_cyclestamp = event->next_value_cyclestamp_get();
            }
        }
        event = next;
    }

    // Some deferred values are still pending (future cyclestamp): keep the
    // flush event armed so the engine really executes at that cycle (a sparse
    // clock would otherwise jump past it and dump them late).
    if (this->trace_flush_head != NULL && this->period != 0)
    {
        int64_t cycles = (int64_t)(min_cyclestamp - this->get_cycles());
        if (cycles < 1)
        {
            cycles = 1;
        }
        this->enqueue(&this->trace_flush_event, cycles);
    }
#endif
}

#ifdef CONFIG_GVSOC_EVENT_ACTIVE
void vp::ClockEngine::trace_flush_handler(vp::Block *__this, vp::ClockEvent *event)
{
    // Nothing to do: the event only forces the engine to execute at the
    // deferred trace values' cyclestamp; dump_traces(), called at the start of
    // the cycle, does the actual flush.
}
#endif

int64_t vp::ClockEngine::exec()
{
    if (unlikely(this->cycle_synced))
    {
        // sync() already counted the cycle we are about to execute. The permanent events path
        // below counts it by incrementing, so give it back to count it exactly once. The other
        // paths either set an absolute cycle count or leave it as it is, which is then right.
        this->cycle_synced = false;
        if (this->permanent_first != NULL)
        {
            this->cycles--;
        }
    }

    this->cycles_trace.event_real(this->cycles);

    ClockEvent *current = this->permanent_first;

    // Also remember the current time in order to resynchronize the clock engine
    // in case we enqueue and event from another engine.
    this->stop_time = this->time.get_time();

    if (likely(current != NULL))
    {
        while(1)
        {
            this->cycles++;

            #ifdef CONFIG_GVSOC_EVENT_ACTIVE
            this->dump_traces();
            #endif

            do
            {
                ClockEvent *next = current->next;
                current->meth(current->_this, current);
                current = next;
            } while (likely(current != NULL));

            if (likely(this->permanent_first != NULL))
            {
                // Shortcut since when we have a permanent event, we probably
                // don't have a delayed event at the same cycle, since they should be
                // rare.
                if (likely(this->next_delayed_cycle > this->cycles))
                {
                    vp::TimeEngine *engine = this->time_engine;
                    if (likely(time_engine->first_client == NULL && !time_engine->stop_req))
                    {
                        engine->time += period;
                        current = this->permanent_first;
                        continue;
                    }

                    return period;
                }
            }

            break;
        }
    }
    else if (likely(delayed_queue != NULL))
    {
        vp_assert(this->cycles <= delayed_queue->cycle, NULL, "Executing event in the past\n");

        this->cycles = delayed_queue->cycle;

        #ifdef CONFIG_GVSOC_EVENT_ACTIVE
        this->dump_traces();
        #endif
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

        this->next_delayed_cycle = this->delayed_queue ? this->delayed_queue->cycle : INT64_MAX;

        current->meth(current->_this, current);
    }

    // Need to check again if we have a permanent event since it could have been enabled during
    // the execution of a delayed event
    if (likely(this->permanent_first != NULL))
    {
        return period;
    }
    else
    {

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
    _this->apply_frequency(frequency * _this->factor);
}

void vp::ClockEngine::change_frequency(int64_t frequency)
{
    this->freq = frequency;
    this->out.set_frequency(frequency);
    this->clock_trace.set(this->period);
}


void vp::ClockEngine::reorder_permanent_events()
{
    if (this->permanent_first && this->permanent_last != this->permanent_first)
    {
        vp::ClockEvent *event = this->permanent_first;
        this->permanent_last->next = event;
        event->prev = this->permanent_last;
        this->permanent_first = event->next;
        event->next = NULL;
        this->permanent_first->prev = NULL;
        this->permanent_last = event;
    }
}



void vp::ClockEngine::pre_start()
{
    out.reg(this);
}

void vp::ClockEngine::start()
{
    // Set ourself as clock engine so that the trace reports cycles from our engine
    this->clock.set_engine(this);
}

void vp::ClockEngine::reset(bool active)
{
    this->clock_trace.set(this->period);
}

vp::ClockEngine::ClockEngine(vp::ComponentConf &config, int64_t frequency, int64_t factor)
: vp::Component(config), clock_trace(*this, "period", 64, false),
    cycles(0), period(0), freq(0),
    apply_frequency_event(this, &vp::ClockEngine::apply_frequency_handler)
#ifdef CONFIG_GVSOC_EVENT_ACTIVE
    , trace_flush_event(this, &vp::ClockEngine::trace_flush_handler)
#endif
{
    this->time_engine = config.time_engine;
    this->time_engine->register_clock_engine(this);
    delayed_queue = NULL;
    current_cycle = 0;

    this->apply_frequency(frequency);

    new_master_port("out", &out);

    clock_in.set_set_frequency_meth(&ClockEngine::set_frequency);
    new_slave_port("clock_in", &clock_in);

    this->traces.new_trace_event_real("cycles", &this->cycles_trace);

    this->factor = factor;

    if (this->factor == 0)
    {
        this->factor = 1;
    }

    // Register clock domain statistics
    this->stats.register_stat(&this->stat_start_cycle, "start_cycle", "Cycle at stats start");
    this->stats.register_stat(&this->stat_end_cycle, "end_cycle", "Cycle at stats end");
    this->stats.register_stat(&this->stat_duration_cycles, "duration_cycles", "Total cycles during stats window");

    // Register pre-dump callback to snapshot current cycle count
    if (this->stats.get_engine() != nullptr)
    {
        // Snapshot start cycle when stats collection starts
        this->stats.get_engine()->register_start_callback([this]() {
            this->sync();
            this->stat_start_cycle.set(this->get_cycles());
        });

        // Snapshot end cycle and compute duration before dump
        this->stats.get_engine()->register_pre_dump([this]() {
            this->sync();
            this->stat_end_cycle.set(this->get_cycles());
            int64_t start = this->stat_start_cycle.get();
            int64_t end = this->stat_end_cycle.get();
            this->stat_duration_cycles.set(end > start ? end - start : 0);
        });
    }
}


std::string vp::ClockEngine::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
    std::vector<std::string> args, std::string req)
{
    if (args.size() > 0 && args[0] == "info")
    {
        this->sync();
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "period=%ld frequency=%ld cycles=%ld",
            this->get_period(), this->get_frequency(), this->get_cycles());
        return std::string(buffer);
    }
    else if (args.size() == 2 && args[0] == "set_frequency")
    {
        // As a frequency generator would: applied at the next cycle
        int64_t frequency = strtoll(args[1].c_str(), NULL, 0);
        if (frequency <= 0)
        {
            return "invalid frequency";
        }
        this->sync();
        this->apply_frequency(frequency);
        return "ok";
    }

    return "";
}


vp::ClockEvent *vp::ClockEngine::step_cycles(int64_t count, step_cycles_callback_t callback,
    void *callback_arg)
{
    // One event per outstanding cycle-step, so several steps (e.g. from independent clients) can be
    // pending on this domain at once without clobbering each other. The event carries its own
    // callback + argument and is freed when it fires (step_cycles_handler) or is canceled
    // (cancel_step_cycles).
    vp::ClockEvent *event = this->event_new(&ClockEngine::step_cycles_handler);
    event->get_args()[0] = (void *)callback;
    event->get_args()[1] = callback_arg;
    event->enqueue(count);
    return event;
}


void vp::ClockEngine::cancel_step_cycles(vp::ClockEvent *event)
{
    if (event == nullptr)
    {
        return;
    }
    event->cancel();
    // The event destructor unregisters it from this block's clock-event list.
    delete event;
}


void vp::ClockEngine::step_cycles_handler(vp::Block *__this, vp::ClockEvent *event)
{
    vp::ClockEngine *_this = (vp::ClockEngine *)__this;
    // Refresh the cycle-count trace at the exact stop cycle. The trace is otherwise only dumped at
    // exec() entry, and the engine can advance many cycles per exec, so without this the GUI would
    // show a stale (lagging) cycle count after a cycle-step until the next exec refreshes it.
    _this->cycles_trace.event_real(_this->cycles);
    _this->time_engine->pause();

    step_cycles_callback_t callback = (step_cycles_callback_t)event->get_args()[0];
    void *callback_arg = event->get_args()[1];

    // The event is one-shot: free it now (safe here — the clock exec loop saves the next event
    // before invoking this callback, so it does not touch `event` afterwards).
    delete event;

    if (callback)
    {
        callback(callback_arg);
    }
}
