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

#ifndef __VP_TRACE_EVENT_DUMPER_HPP__
#define __VP_TRACE_EVENT_DUMPER_HPP__

#include <stdio.h>
#include <unordered_map>
#include <gv/gvsoc.hpp>
#include "vp/json.hpp"
#include <string>

namespace vp {

  class Event_dumper;

  class Event_file 
  {
  public:
    virtual void dump(int64_t timestamp, int id, uint8_t *event, int width, bool is_real, bool is_string, uint8_t flags, uint8_t *flag_mask) {}
    virtual void close() {}
    virtual void add_trace(std::string name, int id, int width, bool is_real, bool is_string) {}

  protected:

    int64_t last_timestamp = -1;
    FILE *file;
    bool header_dumped = false;
  };

  class Event_trace
  {
  public:
    Event_trace(std::string trace_name, Event_file *file, int width, bool is_real, bool is_string);
    void reg(int64_t timestamp, uint8_t *event, int width, uint8_t flags, uint8_t *flag_mask);
    inline void dump(int64_t timestamp) { if (this->buffer) file->dump(timestamp, id, this->buffer, this->width, this->is_real, this->is_string, this->flags, this->flags_mask); }
    std::string trace_name;
    bool is_real = false;
    bool is_string;
    Event_trace *next;
    bool is_enqueued;
    int width;
    int bytes;
    int id;
    uint8_t *buffer;
    uint8_t flags;
    void set_vcd_user(gv::Vcd_user *user);
    int64_t cycles;

  private:
    Event_file *file;
    uint8_t *flags_mask;

  };

  class Event_dumper
  {
  public:
    Event_dumper(js::Config *config) : config(config) { this->user_vcd = NULL; }
    Event_trace *get_trace(std::string trace_name, std::string file_name, int width, bool is_real=false, bool is_string=false);
    Event_trace *get_trace_real(std::string trace_name, std::string file_name);
    Event_trace *get_trace_string(std::string trace_name, std::string file_name);
    void close();
    void set_vcd_user(gv::Vcd_user *user, bool is_external_dumper);

  private:
    std::map<std::string, Event_trace *> event_traces;
    std::map<std::string, Event_file *> event_files;
    gv::Vcd_user *user_vcd;
    js::Config *config;
    bool is_external_dumper = false;
  };

  class Vcd_file : public Event_file
  {
  public:
    Vcd_file(Event_dumper *dumper, std::string path);
    void close();
    void add_trace(std::string name, int id, int width, bool is_real, bool is_string);
    void dump(int64_t timestamp, int id, uint8_t *event, int width, bool is_real, bool is_string, uint8_t flags, uint8_t *flag_mask);

  private:
    std::string parse_path(std::string path, bool begin);
  };

  class Lxt2_file : public Event_file
  {
  public:
    Lxt2_file(Event_dumper *dumper, std::string path);
    void close();
    void add_trace(std::string name, int id, int width, bool is_real, bool is_string);
    void dump(int64_t timestamp, int id, uint8_t *event, int width, bool is_real, bool is_string, uint8_t flags, uint8_t *flag_mask);

  private:
    struct lxt2_wr_trace *trace;
    std::vector<struct lxt2_wr_symbol *> symbols;
  };


  class Fst_file : public Event_file
  {
  public:
    Fst_file(Event_dumper *dumper, std::string path);
    void close();
    void add_trace(std::string name, int id, int width, bool is_real, bool is_string);
    void dump(int64_t timestamp, int id, uint8_t *event, int width, bool is_real, bool is_string, uint8_t flags, uint8_t *flag_mask);

  private:
    std::string parse_path(std::string path, bool begin);

    void *writer;
    std::vector<uint32_t> vars;
  };


  class Raw_file : public Event_file
  {
  public:
    Raw_file(Event_dumper *dumper, std::string path);
    void close();
    void add_trace(std::string name, int id, int width, bool is_real, bool is_string);
    void dump(int64_t timestamp, int id, uint8_t *event, int width, bool is_real, bool is_string, uint8_t flags, uint8_t *flag_mask);

  private:
    void *dumper;
    std::unordered_map<int, void *> traces;
  };

};

#endif
