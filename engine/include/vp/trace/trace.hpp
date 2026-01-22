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

#ifndef __VP_TRACE_TRACE_HPP__
#define __VP_TRACE_TRACE_HPP__

#include "vp/trace/event_dumper.hpp"
#include <functional>
#include <stdarg.h>
#include <vector>
#include <string_view>

namespace vp {

	class TraceEngine;
	class Trace;
	class Event;

	using TraceParseCallback = uint8_t *(*)(uint8_t *buffer, bool &unlock);
    using TraceDumpCallback = void (*)(vp::TraceEngine*, vp::Trace*, int64_t, int64_t, uint8_t*, uint8_t*);

    using EventParseCallback = uint8_t *(*)(uint8_t *buffer, bool &unlock);
    using EventDumpCallback = void (*)(vp::Event*, uint8_t*, int64_t, uint8_t*);

#define BUFFER_SIZE (1 << 16)

class TraceEngine;
class Component;
class Trace;
class Block;

#ifdef CONFIG_GVSOC_EVENT_ACTIVE
class Event
{
public:
    Event(vp::Block &parent, const char *name, int width=64,
        gv::Vcd_event_type type=gv::Vcd_event_type_logical);
    inline void dump_value(uint8_t *value, int64_t time_delay=0);
    inline void dump_highz(int64_t time_delay=0);
    void dump_highz_next();
    std::string path_get();
    void enable_set(bool enabled, vp::Event_file *file=NULL);
    inline bool active_get() { return this->dump_callback != NULL; }
    void dump_next();
    void next_set(vp::Event *next) { this->next = next; }
    vp::Event *next_get() { return this->next; }

    gv::Vcd_event_type type;
    int width;
private:
    static void dump_1(vp::Event *event, uint8_t *value, int64_t time_delay, uint8_t *flags);
    static void dump_8(vp::Event *event, uint8_t *value, int64_t time_delay, uint8_t *flags);
    static void dump_16(vp::Event *event, uint8_t *value, int64_t time_delay, uint8_t *flags);
    static void dump_32(vp::Event *event, uint8_t *value, int64_t time_delay, uint8_t *flags);
    static void dump_64(vp::Event *event, uint8_t *value, int64_t time_delay, uint8_t *flags);
    static void next_value_fill_1(vp::Event *event, uint8_t *value, uint8_t *flags);
    static void next_value_fill_8(vp::Event *event, uint8_t *value, uint8_t *flags);
    static void next_value_fill_32(vp::Event *event, uint8_t *value, uint8_t *flags);
    static void next_value_fill_16(vp::Event *event, uint8_t *value, uint8_t *flags);
    static void next_value_fill_64(vp::Event *event, uint8_t *value, uint8_t *flags);
    static uint8_t *parse_1(uint8_t *buffer, bool &unlock);
    static uint8_t *parse_8(uint8_t *buffer, bool &unlock);
    static uint8_t *parse_16(uint8_t *buffer, bool &unlock);
    static uint8_t *parse_32(uint8_t *buffer, bool &unlock);
    static uint8_t *parse_64(uint8_t *buffer, bool &unlock);

    vp::Block &parent;
    const char *name;
    vp::Event_file *file = NULL;
    EventDumpCallback dump_callback = NULL;
    EventParseCallback parse_callback;
    void (*next_value_fill_callback)(vp::Event *event, uint8_t *value, uint8_t *flags) = NULL;
    int64_t id;
    void *external_trace;
    gv::Vcd_user *vcd_user;
    uint8_t *next_value;
    uint8_t *next_flags;
    vp::Event *next;
    bool has_next_value = false;
};
#else
class Event {
public:
    Event(vp::Block &parent, std::string_view name, int width=64,
        gv::Vcd_event_type type=gv::Vcd_event_type_logical) {}
    void dump_value(uint8_t *value, int64_t time_delay=0) {}
    void dump_highz(int64_t time_delay=0) {}
    void dump_highz_next() {}
    std::string path_get() {return "";}
    void enable_set(bool enabled, vp::Event_file *file=NULL) {}
    inline bool active_get() { return false; }

    gv::Vcd_event_type type=gv::Vcd_event_type_logical;
    int width=0;
};
#endif

class Trace {

    friend class BlockTrace;
    friend class TraceEngine;

  public:
    static const int LEVEL_ERROR = 0;
    static const int LEVEL_WARNING = 1;
    static const int LEVEL_INFO = 2;
    static const int LEVEL_DEBUG = 3;
    static const int LEVEL_TRACE = 4;

    typedef enum {
        WARNING_TYPE_UNCONNECTED_DEVICE,
        WARNING_TYPE_UNCONNECTED_PADFUN
    } warning_type_e;

    inline void msg(int level, const char *fmt, ...);
    inline void msg(const char *fmt, ...);
    inline void user_msg(const char *fmt, ...);
    inline void warning(const char *fmt, ...);
    inline void warning(warning_type_e type, const char *fmt, ...);
    void force_warning(const char *fmt, ...);
    void force_warning(warning_type_e type, const char *fmt, ...);
    void force_warning_no_error(const char *fmt, ...);
    void force_warning_no_error(warning_type_e type, const char *fmt, ...);
    inline void fatal(const char *fmt, ...);

    inline void event_highz(int64_t cycle_delay = 0, int64_t time_delay = 0);
    inline void event(uint8_t *value, int64_t cycle_delay = 0, int64_t time_delay = 0);
    inline void event_string(const char *value, bool realloc);
    inline void event_real(double value);

    void register_callback(std::function<void()> callback) { this->callbacks.push_back(callback); }

    inline std::string get_name() { return this->name; }

    void set_full_path(std::string path) { this->full_path = path; }
    std::string get_full_path() { return this->full_path; }

    void dump_header();
    void dump_warning_header();
    void dump_fatal_header();

    void set_active(bool active);
    void set_event_active(bool active, Event_file *file=NULL);
    vp::Trace *next_get() { return this->next; }

#ifndef VP_TRACE_ACTIVE
    inline bool get_active() { return false; }
    inline bool get_active(int level) { return false; }
    inline bool get_event_active() { return false; }
#else
    inline bool get_active() { return is_active; }
    bool get_active(int level);
    inline bool get_event_active() { return this->dump_callback != NULL; }
#endif
    bool is_active = false;

    int width;
    int id;
    void *user_trace = NULL;
    FILE *trace_file = stdout;
    int is_event;
    gv::Vcd_event_type type;

  protected:
    int level;
    Component *comp = NULL;
    TraceDumpCallback dump_callback = NULL;
    TraceParseCallback parse_callback;
    bool is_event_active = false;
    std::string name;
    std::string path;
    Trace *next;
    std::string full_path;
    std::vector<std::function<void()>> callbacks;
    vp::Trace *clock_trace = NULL;
    Event_file *file;
};

// the static_cast<vp_trace&> is here to fix a weird issue with the -Wnonnull
// warning on GCC11. GCC says that trace_ptr is null, but we verified in the if
// condition that it is not null.
// The static_cast is used to avoid disabling the warning completely.
#define vp_assert_always(cond, trace_ptr, msg...)                                                  \
    if (!(cond)) {                                                                                 \
        if (trace_ptr) {                                                                           \
            vp::Trace *trace_p = trace_ptr;                                                        \
            (static_cast<vp::Trace &>(*trace_p)).fatal(msg);                                       \
        } else {                                                                                   \
            fprintf(stdout, "ASSERT FAILED: ");                                                    \
            fprintf(stdout, msg);                                                                  \
            abort();                                                                               \
        }                                                                                          \
    }

#define vp_warning_always(trace_ptr, msg...)                                                       \
    if (trace_ptr)                                                                                 \
        ((vp::Trace *)(trace_ptr))->force_warning(msg);                                            \
    else {                                                                                         \
        fprintf(stdout, "WARNING: ");                                                              \
        fprintf(stdout, msg);                                                                      \
        abort();                                                                                   \
    }

#ifndef VP_TRACE_ACTIVE
#define vp_assert(cond, trace, msg...)
#else
#define vp_assert(cond, trace_ptr, msg...) vp_assert_always(cond, trace_ptr, msg)
#endif

void fatal(const char *fmt, ...);

}; // namespace vp

#endif
