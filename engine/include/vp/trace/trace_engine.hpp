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

#ifndef __VP_TRACE_ENGINE_HPP__
#define __VP_TRACE_ENGINE_HPP__

#include "vp/vp_data.hpp"
#include "vp/component.hpp"
#include "vp/trace/trace.hpp"
#include "gv/gvsoc.hpp"
#include <pthread.h>
#include <thread>
#include <regex.h>

namespace vp {

  #define TRACE_EVENT_BUFFER_SIZE (1<<16)
  #define TRACE_EVENT_NB_BUFFER   4

  #define TRACE_FORMAT_LONG  0
  #define TRACE_FORMAT_SHORT 1

  class trace_engine
  {
  public:
    trace_engine(js::config *config);
    ~trace_engine();

    virtual void reg_trace(vp::trace *trace, int event, string path, string name) = 0;

    virtual int get_max_path_len() = 0;
    virtual int get_trace_level() = 0;
    virtual void set_trace_level(const char *trace_level) = 0;

    int get_format() { return this->trace_format; }
    
    void set_vcd_user(gv::Vcd_user *user)
    {
        this->event_dumper.set_vcd_user(user);
        this->vcd_user = user;
    }

    void dump_event(vp::trace *trace, int64_t timestamp, uint8_t *event, int width);

    void dump_event_string(vp::trace *trace, int64_t timestamp, uint8_t *event, int width);

    void dump_event_pulse(vp::trace *trace, int64_t timestamp, int64_t end_timestamp, uint8_t *pulse_event, uint8_t *event, int width);

    void dump_event_delayed(vp::trace *trace, int64_t timestamp, uint8_t *event, int width);

    void set_global_enable(bool enable) { this->global_enable = enable; }

    Event_dumper event_dumper;

    vp::trace *get_trace_from_path(std::string path);

    vp::trace *get_trace_from_id(int id);

    virtual void add_trace_path(int events, std::string path) {}
    virtual void conf_trace(int event, std::string path, bool enabled) {}
    virtual void add_exclude_trace_path(int events, std::string path) {}
    virtual void check_traces() {}

    inline bool get_werror() { return this->werror; }
    inline bool is_warning_active(vp::trace::warning_type_e type) { return this->active_warnings[type]; }

    vp::time_engine *time_engine;

  protected:
    std::map<std::string, trace *> traces_map;
    std::vector<trace *> traces_array;
    std::vector<bool> active_warnings;
    int trace_format;
    bool werror;

  private:
    void enqueue_pending(vp::trace *trace, int64_t timestamp, uint8_t *event);
    char *get_event_buffer(int bytes);
    void vcd_routine();
    void flush();
    void check_pending_events(int64_t timestamp);
    void dump_event_to_buffer(vp::trace *trace, int64_t timestamp, uint8_t *event, int bytes, bool include_size=false);

    // This can be called to flush all the pending traces which have been registered for the
    // specified timestamp.
    // This mechanism is used to merged different values of the same trace dumped during
    // the same timestamp.
    void flush_event_traces(int64_t timestamp);

    vector<char *> event_buffers;
    vector<char *> ready_event_buffers;
    char *current_buffer;
    int current_buffer_size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int end = 0;
    std::thread *thread;
    trace *first_pending_event;

    Event_trace *first_trace_to_dump;
    bool global_enable = true;
    gv::Vcd_user *vcd_user;
  };

class trace_regex
{
public:
    trace_regex(std::string path, regex_t *regex, std::string file_path, bool is_path=false) : is_path(is_path), path(path), regex(regex), file_path(file_path) {}

    bool is_path;
    std::string path;
    regex_t *regex;
    std::string file_path;
};

class trace_domain : public vp::trace_engine
{

public:
    trace_domain(vp::component *top, js::config *config);

    void set_trace_level(const char *trace_level);
    void add_paths(int events, int nb_path, const char **paths);
    void add_path(int events, const char *path, bool is_path=false);
    void add_exclude_path(int events, const char *path);
    void add_trace_path(int events, std::string path);
    void conf_trace(int event, std::string path, bool enabled);
    void add_exclude_trace_path(int events, std::string path);
    void reg_trace(vp::trace *trace, int event, string path, string name);

    void pre_pre_build();
    int build();
    void start();
    void run();
    void quit(int status);
    void pause();
    void stop_exec();
    void req_stop_exec();
    void wait_stopped();
    void register_exec_notifier(vp::Notifier *notifier);
    void bind_to_launcher(gv::Gvsoc_user *launcher);

    int join();

    int64_t step(int64_t timestamp);
    int64_t step_until(int64_t timestamp);

    void check_traces();

    int get_max_path_len() { return max_path_len; }

    int exchange_max_path_len(int max_len)
    {
        if (max_len > max_path_len)
            max_path_len = max_len;
        return max_path_len;
    }

    int get_trace_level() { return this->trace_level; }

private:
    void check_trace_active(vp::trace *trace, int event = 0);

    std::unordered_map<std::string, trace_regex *> trace_regexs;
    std::unordered_map<std::string, trace_regex *> trace_exclude_regexs;
    std::unordered_map<std::string, trace_regex *> events_path_regex;
    std::unordered_map<std::string, trace_regex *> events_exclude_path_regex;
    int max_path_len = 0;
    vp::trace_level_e trace_level = vp::TRACE;
    std::vector<vp::trace *> init_traces;
    std::unordered_map<std::string, FILE *> trace_files;
    std::unordered_map<std::string, std::string> active_events;

    FILE *trace_file;
    vp::component *top;
};

};

#endif
