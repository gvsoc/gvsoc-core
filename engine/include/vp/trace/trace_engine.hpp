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

namespace vp {

  #define TRACE_EVENT_BUFFER_SIZE (1<<16)
  #define TRACE_EVENT_NB_BUFFER   4

  #define TRACE_FORMAT_LONG  0
  #define TRACE_FORMAT_SHORT 1

  class trace_engine : public component
  {
  public:
    trace_engine(js::config *config);

    void stop();

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

};

#endif
