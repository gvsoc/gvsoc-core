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
#include <vp/itf/clk.hpp>
#include <vp/trace/trace_engine.hpp>
#include <vector>
#include <thread>
#include <set>
#include <string.h>


void vp::TraceEngine::check_trace_active(vp::Trace *trace, int event)
{
    std::string full_path = trace->get_full_path();

    trace->set_event_active(false);
    trace->set_active(false);

    if (event)
    {
        std::string file_path = this->active_events[full_path];
        if (file_path != "")
        {
            vp::Event_trace *event_trace;
            if (trace->is_real)
                event_trace = event_dumper.get_trace_real(full_path, file_path);
            else if (trace->is_string)
                event_trace = event_dumper.get_trace_string(full_path, file_path);
            else
                event_trace = event_dumper.get_trace(full_path, file_path, trace->width);
            trace->set_event_active(true);
            trace->event_trace = event_trace;
        }
        else
        {
            for (auto &x : events_path_regex)
            {
                if ((x.second->is_path && x.second->path == full_path) || regexec(x.second->regex, full_path.c_str(), 0, NULL, 0) == 0)
                {
                    std::string file_path = x.second->file_path;
                    vp::Event_trace *event_trace;
                    if (trace->is_real)
                        event_trace = event_dumper.get_trace_real(full_path, file_path);
                    else if (trace->is_string)
                        event_trace = event_dumper.get_trace_string(full_path, file_path);
                    else
                        event_trace = event_dumper.get_trace(full_path, file_path, trace->width);
                    trace->set_event_active(true);
                    trace->event_trace = event_trace;
                }
            }
        }

        for (auto &x : this->events_exclude_path_regex)
        {
            if (regexec(x.second->regex, full_path.c_str(), 0, NULL, 0) == 0)
            {
                trace->set_event_active(false);
            }
        }
    }
    else
    {
        for (auto &x : this->trace_regexs)
        {
            if (regexec(x.second->regex, full_path.c_str(), 0, NULL, 0) == 0)
            {
                std::string file_path = x.second->file_path;
                if (file_path == "")
                {
                    trace->trace_file = stdout;
                }
                else
                {
                    if (this->trace_files.count(file_path) == 0)
                    {
                        FILE *file = fopen(file_path.c_str(), "w");
                        if (file == NULL)
                            throw std::logic_error("Unable to open file: " + file_path);
                        this->trace_files[file_path] = file;
                    }
                    trace->trace_file = this->trace_files[file_path];
                }
                trace->set_active(true);
            }
        }

        for (auto &x : this->trace_exclude_regexs)
        {
            if (regexec(x.second->regex, full_path.c_str(), 0, NULL, 0) == 0)
            {
                if (event)
                    trace->set_event_active(false);
                else
                    trace->set_active(false);
            }
        }
    }
}

void vp::TraceEngine::check_traces()
{
    for (auto x : this->traces_array)
    {
        this->check_trace_active(x, x->is_event);
    }
}

void vp::TraceEngine::reg_trace(vp::Trace *trace, int event, string path, string name)
{
    this->traces_array.push_back(trace);
    trace->id = this->traces_array.size() - 1;

    int len = path.size() + name.size() + 1;

    if (len > max_path_len)
        max_path_len = len;

    string full_path;

    if (name[0] != '/')
        full_path = path + "/" + name;
    else
        full_path = name;

    traces_map[full_path] = trace;
    trace->set_full_path(full_path);
    trace->is_event = event;

    trace->trace_file = stdout;

    this->check_trace_active(trace, event);

    if (trace->get_event_active())
    {
        fprintf(this->trace_file, "%s %s ", path.c_str(), name.c_str());
        if (trace->is_real || trace->width <= 1)
        {
            fprintf(this->trace_file, "%s\n", name.c_str());
        }
        else
        {
            fprintf(this->trace_file, "%s[%d:0]\n", name.c_str(), trace->width-1);
        }
    }
}

vp::TraceEngine::TraceEngine(js::Config *config)
    : config(config), event_dumper(config), first_trace_to_dump(NULL), vcd_user(NULL)
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    for (int i = 0; i < TRACE_EVENT_NB_BUFFER; i++)
    {
        event_buffers.push(new char[TRACE_EVENT_BUFFER_SIZE]);
    }
    current_buffer = event_buffers.front();
    event_buffers.pop();
    current_buffer_size = 0;
    this->first_pending_event = NULL;
    this->use_external_dumper = config->get_child_bool("events/use-external-dumper");

    if (this->use_external_dumper)
    {
        thread = new std::thread(&TraceEngine::vcd_routine_external, this);
    }
    else
    {
        thread = new std::thread(&TraceEngine::vcd_routine, this);
    }

    this->trace_format = TRACE_FORMAT_LONG;
    std::string path = "trace_file.txt";
    this->trace_file = fopen(path.c_str(), "w");
    if (this->trace_file == NULL)
    {
        throw std::runtime_error("Error while opening VCD file (path: " + path + ", error: " + strerror(errno) + ")\n");
    }

    for (auto x : config->get("traces/include_regex")->get_elems())
    {
        std::string trace_path = x->get_str();
        this->add_trace_path(0, trace_path);
    }
    for (auto x : config->get("events/include_regex")->get_elems())
    {
        std::string trace_path = x->get_str();
        this->add_trace_path(1, trace_path);
    }
    for (auto x : config->get("events/include_raw")->get_elems())
    {
        std::string file_path, trace_path;
    
        std::string path = x->get_str();
        int pos = path.find('@');
        if (pos != std::string::npos)
        {
            file_path = path.substr(pos + 1);
            trace_path = path.substr(0, pos);
        }
        else
        {
            file_path = "all.vcd";
            trace_path = path;
        }

        this->active_events[x->get_str()] = std::string(file_path);
    }

    this->werror = config->get_child_bool("werror");
    this->set_trace_level(config->get_child_str("traces/level").c_str());

    this->active_warnings.resize(vp::Trace::WARNING_TYPE_UNCONNECTED_DEVICE + 1);
    this->active_warnings[vp::Trace::WARNING_TYPE_UNCONNECTED_DEVICE] = config->get_child_bool("wunconnected-device");

    this->active_warnings.resize(vp::Trace::WARNING_TYPE_UNCONNECTED_PADFUN + 1);
    this->active_warnings[vp::Trace::WARNING_TYPE_UNCONNECTED_PADFUN] = config->get_child_bool("wunconnected-padfun");

    string format = config->get_child_str("traces/format");

    if (format == "short")
    {
        this->trace_format = TRACE_FORMAT_SHORT;
    }
    else
    {
        this->trace_format = TRACE_FORMAT_LONG;
    }
}

void vp::TraceEngine::init(vp::Component *top)
{
    this->top = top;
    auto vcd_traces = config->get("events/traces");

    if (vcd_traces != NULL)
    {
        for (auto x : vcd_traces->get_childs())
        {
            std::string type = x.second->get_child_str("type");
            std::string path = x.second->get_child_str("path");

            if (type == "string")
            {
                vp::Trace *trace = new vp::Trace();
                top->traces.new_trace_event_string(path, trace);
            }
            else if (type == "int")
            {
                vp::Trace *trace = new vp::Trace();
                top->traces.new_trace_event(path, trace, 32);
                this->init_traces.push_back(trace);
            }
        }
    }
}

void vp::TraceEngine::start()
{
    if (this->use_external_dumper)
    {
        for (Trace *trace: this->traces_array)
        {
            if (trace->is_event)
            {
                if (trace->comp->clock.get_engine())
                {
                    trace->clock_trace = &trace->comp->clock.get_engine()->clock_trace;
                }

                std::string clock_trace_name = "";
                if (trace->clock_trace != NULL)
                {
                    clock_trace_name = trace->clock_trace->path;
                }
                gv::Vcd_event_type trace_type = trace->is_real ? gv::Vcd_event_type_real : trace->is_string ? gv::Vcd_event_type_string : gv::Vcd_event_type_logical;
                int width = trace->is_real ? 8 : trace->is_string ? 0 : trace->width;
                this->vcd_user->event_register(trace->id, trace->get_full_path(), trace_type, width, clock_trace_name);
            }
        }
    }
    for (auto x : this->init_traces)
    {
        x->event(NULL);
    }
}



void vp::TraceEngine::add_exclude_path(int events, const char *path)
{
    regex_t *regex = new regex_t();

    if (events)
    {
        char *delim = (char *)::index(path, '@');
        if (delim)
        {
            *delim = 0;
        }

        if (this->events_path_regex.count(path) > 0)
        {
            delete this->events_path_regex[path];
            this->events_path_regex.erase(path);
        }
        else
        {
            this->events_exclude_path_regex[path] = new trace_regex(path, regex, "");
        }
    }
    else
    {
        char *delim = (char *)::index(path, ':');
        if (delim)
        {
            *delim = 0;
        }
        if (this->trace_regexs.count(path) > 0)
        {
            delete this->trace_regexs[path];
            this->trace_regexs.erase(path);
        }
        else
        {
            this->trace_exclude_regexs[path] = new trace_regex(path, regex, "");
        }
    }

    regcomp(regex, path, 0);
}



void vp::TraceEngine::add_path(int events, const char *path, bool is_path)
{
    regex_t *regex = new regex_t();

    if (events)
    {
        const char *file_path = "all.vcd";
        char *delim = (char *)::index(path, '@');
        if (delim)
        {
            *delim = 0;
            file_path = delim + 1;
        }

        if (this->events_exclude_path_regex.count(path) > 0)
        {
            delete this->events_exclude_path_regex[path];
            this->events_exclude_path_regex.erase(path);
        }

        this->events_path_regex[path] = new trace_regex(path, regex, file_path, is_path);
    }
    else
    {
        std::string file_path = "";
        char *dup_path = strdup(path);
        char *sep = strchr(dup_path, ':');
        if (sep)
        {
            *sep = 0;
            file_path = sep + 1;
            path = dup_path;
        }

        if (this->trace_exclude_regexs.count(path) > 0)
        {
            delete this->trace_exclude_regexs[path];
            this->trace_exclude_regexs.erase(path);
        }

        this->trace_regexs[path] = new trace_regex(path, regex, file_path);
    }

    regcomp(regex, path, 0);
}

void vp::TraceEngine::conf_trace(int event, std::string path_str, bool enabled)
{
    const char *file_path = "all.vcd";
    const char *path = path_str.c_str();
    char *delim = (char *)::index(path, '@');

    if (delim)
    {
        *delim = 0;
        file_path = delim + 1;
    }

    std::vector<vp::Trace *> traces;

    vp::Trace *trace = this->get_trace_from_path(path);

    if (trace != NULL)
    {
        traces.push_back(trace);
    }
    else
    {
        this->top->get_trace_from_path(traces, path_str);
    }


    for (vp::Trace *trace: traces)
    {
        if (event && trace->is_event)
        {
            if (enabled)
            {
                vp::Event_trace *event_trace;
                if (trace->is_real)
                    event_trace = event_dumper.get_trace_real(trace->get_full_path(), file_path);
                else if (trace->is_string)
                    event_trace = event_dumper.get_trace_string(trace->get_full_path(), file_path);
                else
                    event_trace = event_dumper.get_trace(trace->get_full_path(), file_path, trace->width);
                trace->event_trace = event_trace;
            }
            trace->set_event_active(enabled);
        }
        else if (!event && !trace->is_event)
        {
            trace->set_active(enabled);
        }
    }
}

void vp::TraceEngine::add_trace_path(int events, std::string path)
{
    this->add_path(events, path.c_str());
}

void vp::TraceEngine::add_exclude_trace_path(int events, std::string path)
{
    this->add_exclude_path(events, path.c_str());
}

void vp::TraceEngine::add_paths(int events, int nb_path, const char **paths)
{
    for (int i = 0; i < nb_path; i++)
    {
        add_path(events, paths[i]);
    }
}



void vp::TraceEngine::set_trace_level(const char *trace_level)
{
    if (strcmp(trace_level, "error") == 0)
    {
        this->trace_level = vp::ERROR;
    }
    else if (strcmp(trace_level, "warning") == 0)
    {
        this->trace_level = vp::WARNING;
    }
    else if (strcmp(trace_level, "info") == 0)
    {
        this->trace_level = vp::INFO;
    }
    else if (strcmp(trace_level, "debug") == 0)
    {
        this->trace_level = vp::DEBUG;
    }
    else if (strcmp(trace_level, "trace") == 0)
    {
        this->trace_level = vp::TRACE;
    }
}
