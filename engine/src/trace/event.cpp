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
#include "vp/trace/event_dumper.hpp"
#include <string.h>
#include <stdexcept>

static int vcd_id = 0;



vp::Event_trace::Event_trace(string trace_name, Event_file *file, int width, bool is_real, bool(is_string)) : trace_name(trace_name), is_real(is_real), is_string(is_string), is_enqueued(false), file(file)
{
  id = vcd_id++;
  if (file)
  {
    file->add_trace(trace_name, id, width, is_real, is_string);
  }
  this->width = width;
  this->bytes = (width+7)/8;
  if (width)
  {
    this->buffer = new uint8_t[width];
    this->flags_mask = new uint8_t[width];
  }
  else
  {
    this->buffer = NULL;
    this->flags_mask = NULL;
  }
}


void vp::Event_trace::reg(int64_t timestamp, uint8_t *event, int width, uint8_t flags, uint8_t *flags_mask)
{
  int bytes = (width+7)/8;
  if (bytes > this->bytes)
  {
    this->bytes = bytes;
    //if (this->buffer)
    //  delete this->buffer;
    this->buffer = new uint8_t[width];
    //if (this->flags_mask)
    //  delete this->flags_mask;
    this->flags_mask = new uint8_t[width];
  }

  memcpy(this->buffer, event, bytes);
  this->flags = flags;
  if (flags == 1)
  {
    memcpy(this->flags_mask, flags_mask, bytes);
  }
}



vp::Event_trace *vp::Event_dumper::get_trace(string trace_name, string file_name, int width, bool is_real, bool is_string)
{
  vp::Event_trace *trace = event_traces[trace_name];

  if (trace == NULL)
  {
    vp::Event_file *event_file = NULL;

    if (!this->user_vcd)
    {
      event_file = event_files[file_name];

      if (event_file == NULL)
      {
        std::string format = this->config->get_child_str("**/events/format");

        if (format == "vcd")
        {
          event_file = new Vcd_file(this, file_name);
        }
        else if (format == "fst")
        {
          event_file = new Fst_file(this, file_name);
        }
        else if (format == "raw")
        {
          event_file = new Raw_file(this, file_name);
        }
        else
        {
          throw std::invalid_argument("Unknown trace format (name: " + format + ")\n");
        }
        event_files[file_name] = event_file;
      }
    }

    trace = new Event_trace(trace_name, event_file, width, is_real, is_string);
    event_traces[trace_name] = trace;

    if (!this->is_external_dumper && this->user_vcd)
    {
      trace->set_vcd_user(this->user_vcd);
    }
  }

  return trace;
}

vp::Event_trace *vp::Event_dumper::get_trace_real(string trace_name, string file_name)
{
  vp::Event_trace *trace = this->get_trace(trace_name, file_name, 8, true);
  return trace;
}

vp::Event_trace *vp::Event_dumper::get_trace_string(string trace_name, string file_name)
{
  vp::Event_trace *trace = this->get_trace(trace_name, file_name, 0, false, true);
  return trace;
}

void vp::Event_trace::set_vcd_user(gv::Vcd_user *user)
{
  user->event_register(this->trace_name, this->is_real ? gv::Vcd_event_type_real : this->is_string ? gv::Vcd_event_type_string : gv::Vcd_event_type_logical, this->width);
}


void vp::Event_dumper::close()
{
  for (auto &x: event_files)
  {
    x.second->close();
  }
}

void vp::Event_dumper::set_vcd_user(gv::Vcd_user *user, bool is_external_dumper)
{
    this->user_vcd = user;
    this->is_external_dumper = is_external_dumper;

    if (!is_external_dumper)
    {
        for (auto const& x : event_traces)
        {
            x.second->set_vcd_user(user);
        }
    }
}
