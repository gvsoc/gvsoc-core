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

#include "vp/component.hpp"
#include "vp/trace/trace.hpp"
#include "gv/gvsoc.hpp"
#include <pthread.h>
#include <thread>
#include <regex.h>
#include <queue>

namespace vp {


    #define TRACE_EVENT_BUFFER_SIZE (1<<20)
    #define TRACE_EVENT_NB_BUFFER   256

    #define TRACE_FORMAT_LONG  0
    #define TRACE_FORMAT_SHORT 1

    class trace_regex
    {
    public:
        trace_regex(std::string path, regex_t *regex, std::string file_path, bool is_path=false) : is_path(is_path), path(path), regex(regex), file_path(file_path) {}

        bool is_path;
        std::string path;
        regex_t *regex;
        std::string file_path;
    };

    class TraceEngine
    {

    public:
        TraceEngine(js::Config *config);
        ~TraceEngine();

        int get_format() { return this->trace_format; }
        
        void set_vcd_user(gv::Vcd_user *user);

        static void dump_event(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_string(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);

        static void dump_event_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_8_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_16_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_32_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_64_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);
        static void dump_event_string_external(vp::TraceEngine *__this, vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes);

        static uint8_t *parse_event(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);
        static uint8_t *parse_event_8(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);
        static uint8_t *parse_event_16(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);
        static uint8_t *parse_event_32(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);
        static uint8_t *parse_event_64(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);
        static uint8_t *parse_event_string(vp::TraceEngine *__this, vp::Trace *trace, uint8_t *buffer);

        void set_global_enable(bool enable) { this->global_enable = enable; }

        Event_dumper event_dumper;

        vp::Trace *get_trace_from_path(std::string path);

        vp::Trace *get_trace_from_id(int id);

        inline bool get_werror() { return this->werror; }
        inline bool is_warning_active(vp::Trace::warning_type_e type) { return this->active_warnings[type]; }

        void init(vp::Component *top);
        void set_trace_level(const char *trace_level);
        void add_paths(int events, int nb_path, const char **paths);
        void add_path(int events, const char *path, bool is_path=false);
        void add_exclude_path(int events, const char *path);
        void add_trace_path(int events, std::string path);
        void conf_trace(int event, std::string path, bool enabled);
        void add_exclude_trace_path(int events, std::string path);
        void reg_trace(vp::Trace *trace, int event, string path, string name);

        void start();
        void check_traces();

        int get_max_path_len() { return max_path_len; }

        int exchange_max_path_len(int max_len)
        {
            if (max_len > max_path_len)
                max_path_len = max_len;
            return max_path_len;
        }

        int get_trace_level() { return this->trace_level; }

        bool use_external_dumper;

        void flush();

    protected:
        std::map<std::string, Trace *> traces_map;
        std::vector<Trace *> traces_array;
        std::vector<bool> active_warnings;
        int trace_format;
        bool werror;

    private:
        void check_trace_active(vp::Trace *trace, int event = 0);

        std::unordered_map<std::string, trace_regex *> trace_regexs;
        std::unordered_map<std::string, trace_regex *> trace_exclude_regexs;
        std::unordered_map<std::string, trace_regex *> events_path_regex;
        std::unordered_map<std::string, trace_regex *> events_exclude_path_regex;
        int max_path_len = 0;
        vp::TraceLevel trace_level = vp::TRACE;
        std::vector<vp::Trace *> init_traces;
        std::unordered_map<std::string, FILE *> trace_files;
        std::unordered_map<std::string, std::string> active_events;

        FILE *trace_file;
        vp::Component *top;
        js::Config *config;

        void enqueue_pending(vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event);
        char *get_event_buffer(int bytes);
        void vcd_routine();
        void vcd_routine_external();
        void check_pending_events(int64_t timestamp);
        void dump_event_to_buffer(vp::Trace *trace, int64_t timestamp, int64_t cycles, uint8_t *event, int bytes, bool include_size=false);

        // This can be called to flush all the pending traces which have been registered for the
        // specified timestamp.
        // This mechanism is used to merged different values of the same trace dumped during
        // the same timestamp.
        void flush_event_traces(int64_t timestamp);

        std::queue<char *> event_buffers;
        std::queue<char *> ready_event_buffers;
        char *current_buffer;
        int current_buffer_size;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int end = 0;
        std::thread *thread;
        Trace *first_pending_event;

        Event_trace *first_trace_to_dump;
        bool global_enable = true;
        gv::Vcd_user *vcd_user;
        int64_t last_event_timestamp;
    };
};

#endif
