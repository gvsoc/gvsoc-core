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

#include "vp/vp.hpp"
#include "vp/trace/trace.hpp"
#include "vp/trace/trace_engine.hpp"
#include <string.h>
#include <inttypes.h>


vp::BlockTrace::BlockTrace(vp::Block *parent, vp::Block &top, vp::TraceEngine *engine)
    : top(top), engine(engine)
{
    if (this->engine == NULL && parent != NULL)
    {
        this->engine = parent->traces.get_trace_engine();
    }
}

void vp::BlockTrace::reg_trace(Trace *trace, int event)
{
    this->get_trace_engine()->reg_trace(trace, event, top.get_path(), trace->get_name());
}

void vp::BlockTrace::new_trace(std::string name, Trace *trace, TraceLevel level)
{
    traces[name] = trace;
    trace->level = level;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;

    this->reg_trace(trace, 0);
}

void vp::BlockTrace::new_trace_event(std::string name, Trace *trace, int width)
{
    trace_events[name] = trace;
    trace->width = width;
    trace->bytes = (width + 7) / 8;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;
    trace->pending_timestamp = -1;
    trace->buffer = new uint8_t[trace->bytes];
    trace->buffer2 = new uint8_t[trace->bytes];

    this->reg_trace(trace, 1);
}

void vp::BlockTrace::new_trace_event_real(std::string name, Trace *trace)
{
    trace_events[name] = trace;
    trace->width = 64;
    trace->bytes = 8;
    trace->is_real = true;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;
    trace->pending_timestamp = -1;
    trace->buffer = new uint8_t[trace->bytes];
    trace->buffer2 = new uint8_t[trace->bytes];

    this->reg_trace(trace, 1);
}

void vp::BlockTrace::new_trace_event_string(std::string name, Trace *trace)
{
    trace_events[name] = trace;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;

    trace->width = 0;
    trace->is_real = false;
    trace->is_string = true;
    trace->pending_timestamp = -1;
    trace->bytes = 0;
    trace->buffer = NULL;
    trace->buffer2 = NULL;

    this->reg_trace(trace, 1);
}

#ifdef VP_TRACE_ACTIVE
bool vp::Trace::get_active(int level)
{
    return is_active && this->comp->traces.get_trace_engine()->get_trace_level() >= level;
}
#endif

void vp::Trace::dump_header()
{
    int64_t time = -1;
    int64_t cycles = -1;
    if (comp->clock.get_engine())
    {
        cycles = comp->clock.get_engine()->get_cycles();
    }
    if (comp->time.get_engine())
    {
        time = comp->time.get_engine()->get_time();
    }

    int format = comp->traces.get_trace_engine()->get_format();
    if (format == TRACE_FORMAT_SHORT)
    {
        fprintf(this->trace_file, "%" PRId64 "ps %" PRId64 " ", time, cycles);
    }
    else
    {
        int max_trace_len = comp->traces.get_trace_engine()->get_max_path_len();
        fprintf(this->trace_file, "%" PRId64 ": %" PRId64": [\033[34m%-*.*s\033[0m] ", time, cycles, max_trace_len, max_trace_len, path.c_str());
    }
}


void vp::Trace::force_warning(const char *fmt, ...)
{
    dump_warning_header();
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(this->trace_file, fmt, ap) < 0) {}
    va_end(ap);

    if (comp->traces.get_trace_engine()->get_werror())
    {
        exit(1);
    }
}


void vp::Trace::force_warning(vp::Trace::warning_type_e type, const char *fmt, ...)
{
    if (comp->traces.get_trace_engine()->is_warning_active(type))
    {
        dump_warning_header();
        va_list ap;
        va_start(ap, fmt);
        if (vfprintf(this->trace_file, fmt, ap) < 0) {}
        va_end(ap);

        if (comp->traces.get_trace_engine()->get_werror())
        {
            exit(1);
        }
    }
}


void vp::Trace::dump_warning_header()
{
    int max_trace_len = comp->traces.get_trace_engine()->get_max_path_len();
    int64_t cycles = 0;
    int64_t time = 0;
    if (comp->clock.get_engine())
    {
        cycles = comp->clock.get_engine()->get_cycles();
    }
    if (comp->time.get_engine())
    {
        time = comp->time.get_time();
    }
    fprintf(this->trace_file, "%" PRId64 ": %" PRId64 ": [\033[31m%-*.*s\033[0m] ", time, cycles, max_trace_len, max_trace_len, path.c_str());
}

void vp::Trace::dump_fatal_header()
{
    fprintf(this->trace_file, "[\033[31m%s\033[0m] ", path.c_str());
}

void vp::Trace::set_active(bool active)
{
    this->is_active = active;

    for (auto x : this->callbacks)
    {
        x();
    }
}

void vp::Trace::set_event_active(bool active)
{
    this->is_event_active = active;

    for (auto x : this->callbacks)
    {
        x();
    }
}

char *vp::TraceEngine::get_event_buffer(int bytes)
{
    if (current_buffer == NULL || bytes > TRACE_EVENT_BUFFER_SIZE - current_buffer_size)
    {
        pthread_mutex_lock(&mutex);

        if (current_buffer && bytes > TRACE_EVENT_BUFFER_SIZE - current_buffer_size)
        {
            if ((unsigned int)(TRACE_EVENT_BUFFER_SIZE - current_buffer_size) > sizeof(vp::Trace *))
                *(vp::Trace **)(current_buffer + current_buffer_size) = NULL;

            ready_event_buffers.push_back(current_buffer);
            current_buffer = NULL;

            pthread_cond_broadcast(&cond);
        }

        while (event_buffers.size() == 0)
        {
            pthread_cond_wait(&cond, &mutex);
        }
        current_buffer = event_buffers[0];
        event_buffers.erase(event_buffers.begin());
        current_buffer_size = 0;
        pthread_mutex_unlock(&mutex);
    }

    char *result = current_buffer + current_buffer_size;

    current_buffer_size += bytes;

    return result;
}

vp::TraceEngine::~TraceEngine()
{
    this->check_pending_events(-1);
    this->flush();
    pthread_mutex_lock(&mutex);
    this->end = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    this->thread->join();
    fflush(NULL);
}

void vp::TraceEngine::flush()
{
    // Flush only the events until the current timestamp as we may resume
    // the execution right after
    this->check_pending_events(this->top->time.get_engine()->get_time());

    if (current_buffer_size)
    {
        pthread_mutex_lock(&mutex);

        if (current_buffer)
        {
            *(vp::Trace **)(current_buffer + current_buffer_size) = NULL;
            ready_event_buffers.push_back(current_buffer);
            current_buffer = NULL;
        }
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
}

void vp::TraceEngine::dump_event_to_buffer(vp::Trace *trace, int64_t timestamp, uint8_t *event, int bytes, bool include_size)
{
    if (!this->global_enable)
        return;

    uint8_t flags = 0;
    int size = bytes + sizeof(trace) + sizeof(timestamp) + 1;
    if (include_size)
        size += 4;

    if (event == NULL)
    {
        flags = 1;
        size += bytes;
    }

    char *event_buffer = this->get_event_buffer(size);

    *(vp::Trace **)event_buffer = trace;
    event_buffer += sizeof(trace);

    *(uint8_t *)event_buffer = flags;
    event_buffer++;

    // In case the special value 'Z' is inside, give the mask where it applies
    if (flags == 1)
    {
        // For now we just support 'Z' everywhere
        memset((void *)event_buffer, 0xff, bytes);
        event_buffer += bytes;
    }

    *(int64_t *)event_buffer = timestamp;
    event_buffer += sizeof(timestamp);

    if (include_size)
    {
        *(int32_t *)event_buffer = bytes;
        event_buffer += 4;
    }

    if (event)
    {
        if (bytes == 4)
            *(uint32_t *)event_buffer = *(uint32_t *)event;
        else
            memcpy((void *)event_buffer, (void *)event, bytes);
    }
    else
    {
        memset((void *)event_buffer, 0, bytes);
    }
}

void vp::TraceEngine::dump_event(vp::Trace *trace, int64_t timestamp, uint8_t *event, int bytes)
{
    this->check_pending_events(timestamp);

    this->dump_event_to_buffer(trace, timestamp, event, bytes);
}

void vp::TraceEngine::dump_event_string(vp::Trace *trace, int64_t timestamp, uint8_t *event, int bytes)
{
    this->check_pending_events(timestamp);

    this->dump_event_to_buffer(trace, timestamp, event, bytes, true);
}

void vp::TraceEngine::dump_event_pulse(vp::Trace *trace, int64_t timestamp, int64_t end_timestamp, uint8_t *pulse_event, uint8_t *event, int width)
{
    this->check_pending_events(timestamp);

    // Then dequeue the trace if a pending value is already there
    if (trace->pending_timestamp != -1)
    {
        if (trace->prev != NULL)
            trace->prev->next = trace->next;
        else
            this->first_pending_event = trace->next;

        if (trace->next != NULL)
            trace->next->prev = trace->prev;
    }

    this->dump_event_to_buffer(trace, timestamp, pulse_event, width);

    this->enqueue_pending(trace, end_timestamp, event);
}

void vp::TraceEngine::check_pending_events(int64_t timestamp)
{
    vp::Trace *trace = this->first_pending_event;

    while (trace && (timestamp == -1 || trace->pending_timestamp <= timestamp))
    {
        this->dump_event_to_buffer(trace, trace->pending_timestamp, trace->buffer, trace->bytes);
        trace->pending_timestamp = -1;
        trace = trace->next;
    }
    this->first_pending_event = trace;
    if (trace)
        trace->prev = NULL;
}

vp::Trace *vp::TraceEngine::get_trace_from_path(std::string path)
{
    return this->traces_map[path];
}

vp::Trace *vp::TraceEngine::get_trace_from_id(int id)
{
    if (id >= (int)this->traces_array.size())
        return NULL;
    return this->traces_array[id];
}

void vp::TraceEngine::enqueue_pending(vp::Trace *trace, int64_t timestamp, uint8_t *event)
{
    // Eenqueue the trace to the pending queue
    vp::Trace *current = this->first_pending_event, *prev = NULL;

    while (current && current->pending_timestamp < timestamp)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
    {
        prev->next = trace;
    }
    else
    {
        this->first_pending_event = trace;
    }

    trace->prev = prev;
    trace->next = current;
    if (current)
        current->prev = trace;

    trace->pending_timestamp = timestamp;

    // And dump the value to the trace
    memcpy(trace->buffer, event, trace->bytes);
}

// This is called when several values can be dumped for the same trace
// and only the last value must be dumped
void vp::TraceEngine::dump_event_delayed(vp::Trace *trace, int64_t timestamp, uint8_t *event, int bytes)
{
    // First flush pending events
    this->check_pending_events(timestamp);

    this->dump_event_to_buffer(trace, timestamp, event, bytes);
}

void vp::TraceEngine::flush_event_traces(int64_t timestamp)
{
    Event_trace *current = first_trace_to_dump;
    while (current)
    {
        if (this->vcd_user)
        {
            if (current->is_real)
            {
                this->vcd_user->event_update_real(timestamp, current->id, *(double *)current->buffer);
            }
            else if (current->is_string)
            {
                this->vcd_user->event_update_string(timestamp, current->id, (const char *)current->buffer, current->flags);
            }
            else if (current->width > 8)
            {
                if (current->width <= 16)
                {
                    this->vcd_user->event_update_logical(timestamp, current->id, *(uint16_t *)current->buffer, current->flags);
                }
                else if (current->width <= 32)
                {
                    this->vcd_user->event_update_logical(timestamp, current->id, *(uint32_t *)current->buffer, current->flags);
                }
                else if (current->width <= 64)
                {
                    this->vcd_user->event_update_logical(timestamp, current->id, *(uint64_t *)current->buffer, current->flags);
                }
                else
                {
                    // Use bitfield
                }

            }
            else
            {
                uint64_t value = (uint64_t)*(current->buffer);

                this->vcd_user->event_update_logical(timestamp, current->id, value, current->flags);
            }
        }
        else
        {
            current->dump(timestamp);
        }
        current->is_enqueued = false;
        current = current->next;
    }
    first_trace_to_dump = NULL;
}


// This routines runs in a dedicated thread.
// It waits for buffers of events filled by the engine thread, unpack them
// and pass them to the proper back-end.
// This routines is also in charge of merging events dumped several times during the same
// timestamp. For that, it dumps it builds a list of traces to be dumped and flushes
// them when the next timestamp is detected.
void vp::TraceEngine::vcd_routine()
{
    int64_t last_timestamp = -1;

    while (1)
    {
        char *event_buffer, *event_buffer_start;

        pthread_mutex_lock(&this->mutex);

        // Wait for a buffer of event of the end of simulation
        while (this->ready_event_buffers.size() == 0 && !end)
        {
            pthread_cond_wait(&this->cond, &this->mutex);
        }

        // In case of the end of simulation, just leave
        if (this->ready_event_buffers.size() == 0 && end)
        {
            pthread_mutex_unlock(&this->mutex);
            break;
        }

        // Pop the first buffer
        event_buffer = ready_event_buffers[0];
        event_buffer_start = event_buffer;
        ready_event_buffers.erase(ready_event_buffers.begin());

        pthread_mutex_unlock(&this->mutex);

        // And go through the events to unpack them
        while (event_buffer - event_buffer_start < (int)(TRACE_EVENT_BUFFER_SIZE - sizeof(vp::Trace *)))
        {
            int64_t timestamp;
            uint8_t flags;

            // Unpack the trace.
            // Note that the traces are dumped only when the next timestamp is detected
            // so that different values of the same trace during the same timestamp
            // are overwritten by the last value to merge them into one value.
            vp::Trace *trace = *(vp::Trace **)event_buffer;
            if (trace == NULL)
                break;

            event_buffer += sizeof(trace);

            int bytes = trace->bytes;
            uint8_t flags_mask[bytes];

            flags = *(uint8_t *)event_buffer;
            event_buffer++;

            if (flags == 1)
            {
                memcpy((void *)flags_mask, (void *)event_buffer, bytes);
                event_buffer += bytes;
            }

            timestamp = *(int64_t *)event_buffer;

            if (last_timestamp == -1)
                last_timestamp = timestamp;

            // A new timestamp is detected, flush all the traces to start a new set
            // of values for the current timestamp
            if (last_timestamp < timestamp)
            {
                this->flush_event_traces(last_timestamp);
                last_timestamp = timestamp;
            }

            event_buffer += sizeof(timestamp);

            if (trace->is_string && flags != 1)
            {
                bytes = *(int32_t *)event_buffer;
                trace->width = bytes * 8;
                if (trace->event_trace)
                {
                    trace->event_trace->width = trace->width;
                }
                event_buffer += 4;
            }

            uint8_t event[bytes];

            memcpy((void *)&event, (void *)event_buffer, bytes);
            event_buffer += bytes;

            // Check if the event trace is already registered, otherwise register it
            // to dump it when the next timestamp is detected.
            if (trace->event_trace)
            {
                trace->event_trace->reg(timestamp, event, trace->width, flags, flags_mask);
                if (!trace->event_trace->is_enqueued)
                {
                    trace->event_trace->is_enqueued = true;
                    trace->event_trace->next = this->first_trace_to_dump;
                    this->first_trace_to_dump = trace->event_trace;
                }
            }
        }

        // Now push back the buffer of events into the list of free buffers
        pthread_mutex_lock(&this->mutex);
        event_buffers.push_back(event_buffer_start);
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&this->mutex);
    }

    this->flush_event_traces(last_timestamp);
    event_dumper.close();
}
