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


// #define DIRECT_DUMP 1

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
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;
    trace->type = gv::Vcd_event_type_logical;

    this->reg_trace(trace, 1);
}

void vp::BlockTrace::new_trace_event_real(std::string name, Trace *trace)
{
    trace_events[name] = trace;
    trace->width = 64;
    trace->type = gv::Vcd_event_type_real;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;

    this->reg_trace(trace, 1);
}

void vp::BlockTrace::new_trace_event_string(std::string name, Trace *trace)
{
    trace_events[name] = trace;
    trace->comp = static_cast<vp::Component *>(&top);
    trace->name = name;
    trace->path = top.get_path() + "/" + name;

    trace->width = 0;
    trace->type = gv::Vcd_event_type_string;

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

void vp::Trace::force_warning_no_error(const char *fmt, ...)
{
    dump_warning_header();
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(this->trace_file, fmt, ap) < 0) {}
    va_end(ap);
}


void vp::Trace::force_warning_no_error(vp::Trace::warning_type_e type, const char *fmt, ...)
{
    if (comp->traces.get_trace_engine()->is_warning_active(type))
    {
        dump_warning_header();
        va_list ap;
        va_start(ap, fmt);
        if (vfprintf(this->trace_file, fmt, ap) < 0) {}
        va_end(ap);
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

void vp::Trace::set_event_active(bool active, Event_file *file)
{
    this->is_event_active = active;

    if (active)
    {
        if (this->type == gv::Vcd_event_type_string)
        {
            this->dump_callback = &vp::TraceEngine::dump_event_string;
            this->parse_callback = &vp::TraceEngine::parse_event_string;
        }
        else if (this->type == gv::Vcd_event_type_real)
        {
            this->dump_callback = &vp::TraceEngine::dump_event_real;
            this->parse_callback = &vp::TraceEngine::parse_event_real;
        }
        else
        {
            if (this->width <= 1)
            {
                this->dump_callback = &vp::TraceEngine::dump_event_1;
                this->parse_callback = &vp::TraceEngine::parse_event_1;
            }
            else if (this->width <= 8)
            {
                this->dump_callback = &vp::TraceEngine::dump_event_8;
                this->parse_callback = &vp::TraceEngine::parse_event_8;
            }
            else if (this->width <= 16)
            {
                this->dump_callback = &vp::TraceEngine::dump_event_16;
                this->parse_callback = &vp::TraceEngine::parse_event_16;
            }
            else if (this->width <= 32)
            {
                this->dump_callback = &vp::TraceEngine::dump_event_32;
                this->parse_callback = &vp::TraceEngine::parse_event_32;
            }
            else if (this->width <= 64)
            {
                this->dump_callback = &vp::TraceEngine::dump_event_64;
                this->parse_callback = &vp::TraceEngine::parse_event_64;
            }
            else
            {
                this->dump_callback = &vp::TraceEngine::dump_event;
                this->parse_callback = &vp::TraceEngine::parse_event;
            }
        }

        this->file = file;
        if (file)
        {
            file->add_trace(this->full_path, this->id, this->width, this->type);
        }
    }
    else
    {
        this->dump_callback = NULL;
    }

    for (auto x : this->callbacks)
    {
        x();
    }
}

void vp::TraceEngine::get_new_buffer()
{
    pthread_mutex_lock(&this->mutex);

    // Since the depacker does not know the size of the events, but only that first element is
    // always the trace, push a NULL trace if there is enough room so that it knowns when to
    // stop.
    // If there is not enough room, it will also correctly detect it
    if (this->current_buffer_remaining_size >= sizeof(vp::Trace *))
    {
        *(vp::Trace **)this->current_buffer_event = NULL;
    }

    this->ready_event_buffers.push(this->current_buffer);
    pthread_cond_broadcast(&this->cond);

    while (this->event_buffers.empty())
    {
        pthread_cond_wait(&this->cond, &this->mutex);
    }

    this->current_buffer = this->event_buffers.front();
    this->current_buffer_event = this->current_buffer;
    this->event_buffers.pop();
    this->current_buffer_remaining_size = TRACE_EVENT_BUFFER_SIZE;
    pthread_mutex_unlock(&this->mutex);
}

vp::TraceEngine::~TraceEngine()
{
    this->flush();
    pthread_mutex_lock(&mutex);
    this->end = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
    this->thread->join();

    for (auto file: this->trace_files)
    {
        fflush(file.second);
    }

    while (!this->event_buffers.empty())
    {
        delete[] this->event_buffers.front();
        this->event_buffers.pop();
    }

    if (this->current_buffer)
    {
        delete[] this->current_buffer;
    }
}

void vp::TraceEngine::flush()
{
    // First flush current buffer
    if (this->current_buffer_remaining_size != TRACE_EVENT_BUFFER_SIZE)
    {
        this->get_new_buffer();
    }

    // Then wait until all pending buffers has been handled by dumper
    pthread_mutex_lock(&this->mutex);

    // We should get max number of buffers minus 1 as one has been taken as current buffer
    while(this->event_buffers.size() != TRACE_EVENT_NB_BUFFER - 1)
    {
        pthread_cond_wait(&this->cond, &this->mutex);
    }

    pthread_mutex_unlock(&this->mutex);
}

void vp::TraceEngine::dump_event(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    // TODO
}

using ParseCallback = uint8_t *(*)(uint8_t *buffer, bool &unlock);

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    double value;
} __attribute__((packed)) event_real_t;

void vp::TraceEngine::dump_event_real(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_real_t *buff_event = (event_real_t *)_this->get_event_buffer(sizeof(event_real_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(double *)event;
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    uint64_t value;
    uint64_t flags;
} __attribute__((packed)) event_64_t;

void vp::TraceEngine::dump_event_64(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_64_t *buff_event = (event_64_t *)_this->get_event_buffer(sizeof(event_64_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(uint64_t *)event;
    buff_event->flags = *(uint64_t *)flags;
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    uint8_t value;
    uint8_t flags;
} __attribute__((packed)) event_1_t;

void vp::TraceEngine::dump_event_1(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_1_t *buff_event = (event_1_t *)_this->get_event_buffer(sizeof(event_1_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(uint8_t *)event | (((*(uint8_t *)flags) & 1) << 1);
    buff_event->flags = *(uint64_t *)flags;
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    uint8_t value;
    uint8_t flags;
} __attribute__((packed)) event_8_t;

void vp::TraceEngine::dump_event_8(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_8_t *buff_event = (event_8_t *)_this->get_event_buffer(sizeof(event_8_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(uint8_t *)event;
    buff_event->flags = *(uint8_t *)flags;
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    uint16_t value;
    uint16_t flags;
} __attribute__((packed)) event_16_t;

void vp::TraceEngine::dump_event_16(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_16_t *buff_event = (event_16_t *)_this->get_event_buffer(sizeof(event_16_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(uint16_t *)event;
    buff_event->flags = *(uint16_t *)flags;
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    uint32_t value;
    uint32_t flags;
} __attribute__((packed)) event_32_t;

void vp::TraceEngine::dump_event_32(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, uint8_t *flags)
{
    if (!_this->global_enable)
        return;

    event_32_t *buff_event = (event_32_t *)_this->get_event_buffer(sizeof(event_32_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->value = *(uint32_t *)event;
    buff_event->flags = *(uint32_t *)flags;
}


uint8_t *vp::TraceEngine::parse_event(uint8_t *buffer, bool &unlock)
{
    // TODO
    return buffer;
}

uint8_t *vp::TraceEngine::parse_event_real(uint8_t *buffer, bool &unlock)
{
    event_real_t *event = (event_real_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_real(event->timestamp, event->cycles,
            trace->user_trace, event->value);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, 0, NULL);
    }

    return buffer + sizeof(event_real_t);
}

uint8_t *vp::TraceEngine::parse_event_64(uint8_t *buffer, bool &unlock)
{
    event_64_t *event = (event_64_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_logical(event->timestamp, event->cycles,
            trace->user_trace, event->value, event->flags);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_64_t);
}

uint8_t *vp::TraceEngine::parse_event_1(uint8_t *buffer, bool &unlock)
{
    event_1_t *event = (event_1_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_logical(event->timestamp, event->cycles,
            trace->user_trace, event->value, event->flags);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_1_t);
}

uint8_t *vp::TraceEngine::parse_event_8(uint8_t *buffer, bool &unlock)
{
    event_8_t *event = (event_8_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_logical(event->timestamp, event->cycles,
            trace->user_trace, event->value, event->flags);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_8_t);
}

uint8_t *vp::TraceEngine::parse_event_16(uint8_t *buffer, bool &unlock)
{
    event_16_t *event = (event_16_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_logical(event->timestamp, event->cycles,
            trace->user_trace, event->value, event->flags);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_16_t);
}

uint8_t *vp::TraceEngine::parse_event_32(uint8_t *buffer, bool &unlock)
{
    event_32_t *event = (event_32_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_logical(event->timestamp, event->cycles,
            trace->user_trace, event->value, event->flags);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)&event->value, trace->width,
            trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_32_t);
}

typedef struct
{
    ParseCallback callback;
    vp::Trace *trace;
    int64_t timestamp;
    int64_t cycles;
    const char *value;
    uint64_t flags;
} __attribute__((packed)) event_string_t;

void vp::TraceEngine::dump_event_string(vp::TraceEngine *_this, vp::Trace *trace, int64_t timestamp,
    int64_t cycles, uint8_t *event, uint8_t *flags)
{
    event_string_t *buff_event = (event_string_t *)_this->get_event_buffer(sizeof(event_string_t));

    buff_event->callback = trace->parse_callback;
    buff_event->trace = trace;
    buff_event->timestamp = timestamp;
    buff_event->cycles = cycles;
    buff_event->flags = flags != NULL ? *(uint64_t *)flags : event == (uint8_t *)1 ? -1 : 0;
    if (buff_event->flags == 0)
    {
        buff_event->value = _this->get_string((const char *)event);
    }
    else
    {
        buff_event->value = NULL;
    }
}

uint8_t *vp::TraceEngine::parse_event_string(uint8_t *buffer, bool &unlock)
{
    event_64_t *event = (event_64_t *)buffer;
    vp::Trace *trace = event->trace;

    // User trace can be NULL if some components are dumping traces during construction
    if (trace->user_trace)
    {
        unlock = trace->comp->traces.get_trace_engine()->vcd_user->event_update_string(event->timestamp, event->cycles,
            trace->user_trace, (const char *)event->value, event->flags, false);
    }
    else
    {
        trace->file->dump(event->timestamp, trace->id, (uint8_t *)event->value,
            event->value ? (strlen((char *)event->value) + 1) * 8 : 0, trace->type, event->flags, NULL);
    }

    return buffer + sizeof(event_64_t);
}


vp::Trace *vp::TraceEngine::get_trace_from_path(std::string path)
{
    return this->traces_map[path];
}

vp::Event *vp::TraceEngine::get_event_from_path(std::string path)
{
    return this->events[path];
}

vp::Trace *vp::TraceEngine::get_trace_from_id(int id)
{
    if (id >= (int)this->traces_array.size())
        return NULL;

    return this->traces_array[id];
}

vp::Trace *vp::TraceEngine::get_trace_event_from_id(int id)
{
    if (id >= (int)this->events_array.size())
        return NULL;

    if (this->is_event[id])
    {
        return NULL;
    }
    else
    {
        return this->events_array[id];
    }
}

vp::Event *vp::TraceEngine::get_event_from_id(int id)
{
    if (id >= (int)this->events_array.size())
        return NULL;

    if (this->is_event[id])
    {
        return (vp::Event *)this->events_array[id];
    }
    else
    {
        return NULL;
    }
}

void vp::TraceEngine::set_vcd_user(gv::Vcd_user *user)
{
    this->vcd_user = user;
}

void vp::TraceEngine::vcd_routine()
{
    while (1)
    {
        uint8_t *event_buffer, *event_buffer_start;

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
        event_buffer = (uint8_t *)ready_event_buffers.front();
        event_buffer_start = event_buffer;
        ready_event_buffers.pop();

        pthread_mutex_unlock(&this->mutex);

        if (this->vcd_user)
        {
            this->vcd_user->lock();
        }

        // And go through the events to unpack them
        while (event_buffer - event_buffer_start < (size_t)(TRACE_EVENT_BUFFER_SIZE - sizeof(vp::Trace *)))
        {
            using ParseCallback = uint8_t *(*)(uint8_t *buffer, bool &unlock);
            ParseCallback callback = *reinterpret_cast<ParseCallback *>(event_buffer);

            if (callback == NULL)
                break;

            bool unlock;
            event_buffer = callback(event_buffer, unlock);

            if (unlock)
            {
                if (this->vcd_user)
                {
                    this->vcd_user->unlock();
                    this->vcd_user->lock();
                }
            }
        }

        if (this->vcd_user)
        {
            this->vcd_user->unlock();
        }

        // Now push back the buffer of events into the list of free buffers
        pthread_mutex_lock(&this->mutex);
        event_buffers.push((char *)event_buffer_start);
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&this->mutex);
    }

    if (!this->use_external_dumper)
    {
        for (auto &x: event_files)
        {
            x.second->close();
        }
    }
}
