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


#ifndef __VP_TRACE_IMPLEMENTATION_HPP__
#define __VP_TRACE_IMPLEMENTATION_HPP__

#include "vp/trace/trace_engine.hpp"
#include "vp/clock/clock_engine.hpp"

namespace vp {

  inline TraceEngine *BlockTrace::get_trace_engine()
  {
    return this->engine;
  }

  inline void vp::Trace::event(uint8_t *value)
  {
  #ifdef VP_TRACE_ACTIVE

    void (*callback)(vp::TraceEngine *engine, vp::Trace *trace, int64_t time, int64_t cycles, uint8_t *value, int bytes) = this->dump_event_callback;

    if (callback)
    {
      callback(this->comp->traces.get_trace_engine(), this, comp->time.get_time(), comp->clock.get_engine() ? comp->clock.get_cycles() : -1, value, bytes);
    }
  #endif
  }

  inline void vp::Trace::event_string(std::string value)
  {
  #ifdef VP_TRACE_ACTIVE

    void (*callback)(vp::TraceEngine *engine, vp::Trace *trace, int64_t time, int64_t cycles, uint8_t *value, int bytes) = this->dump_event_callback;

    if (callback)
    {
      callback(this->comp->traces.get_trace_engine(), this, comp->time.get_time(), comp->clock.get_engine() ? comp->clock.get_cycles() : -1, (uint8_t *)value.c_str(), value.length() + 1);
    }
  #endif
  }

  inline void vp::Trace::event_real(double value)
  {
  #ifdef VP_TRACE_ACTIVE

    void (*callback)(vp::TraceEngine *engine, vp::Trace *trace, int64_t time, int64_t cycles, uint8_t *value, int bytes) = this->dump_event_callback;

    if (callback)
    {
      callback(this->comp->traces.get_trace_engine(), this, comp->time.get_time(), comp->clock.get_engine() ? comp->clock.get_cycles() : -1, (uint8_t *)&value, 8);
    }
  #endif
  }


  inline void vp::Trace::user_msg(const char *fmt, ...) {
    #if 0
    fprintf(trace_file, "%ld: %ld: [\033[34m%-*.*s\033[0m] ", comp->clock.get_engine()->time.get_time(), comp->clock.get_engine()->get_cycles(), max_trace_len, max_trace_len, comp->get_path());
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(trace_file, format, ap) < 0) {}
    va_end(ap);  
    #endif
  }

  inline void vp::Trace::fatal(const char *fmt, ...)
  {
    dump_fatal_header();
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(this->trace_file, fmt, ap) < 0) {}
    va_end(ap);
    exit(1);
  }

  inline void vp::Trace::warning(const char *fmt, ...) {
  #ifdef VP_TRACE_ACTIVE
  	if (is_active && comp->traces.get_trace_engine()->get_trace_level() >= this->level)
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
  #endif
  }

  inline void vp::Trace::warning(vp::Trace::warning_type_e type, const char *fmt, ...) {
  #ifdef VP_TRACE_ACTIVE
  	if (is_active && comp->traces.get_trace_engine()->get_trace_level() >= vp::Trace::LEVEL_WARNING)
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
  #endif
  }

  inline void vp::Trace::msg(const char *fmt, ...) 
  {
  #ifdef VP_TRACE_ACTIVE
  	if (is_active && comp->traces.get_trace_engine()->get_trace_level() >= this->level)
    {
      dump_header();
      va_list ap;
      va_start(ap, fmt);
      if (vfprintf(this->trace_file, fmt, ap) < 0) {}
      va_end(ap);  
    }
  #endif
  }

  inline void vp::Trace::msg(int level, const char *fmt, ...) 
  {
  #ifdef VP_TRACE_ACTIVE
    if (is_active && comp->traces.get_trace_engine()->get_trace_level() >= level)
    {
      dump_header();
      if (level == vp::Trace::LEVEL_ERROR)
      {
        fprintf(this->trace_file, "\033[31m");
      }
      else if (level == vp::Trace::LEVEL_WARNING)
      {
        fprintf(this->trace_file, "\033[33m");
      }
      va_list ap;
      va_start(ap, fmt);
      if (vfprintf(this->trace_file, fmt, ap) < 0) {}
      va_end(ap);  
      if (level == vp::Trace::LEVEL_ERROR || level == vp::Trace::LEVEL_WARNING)
      {
        fprintf(this->trace_file, "\033[0m");
      }
    }
  #endif
  }


};

#endif
