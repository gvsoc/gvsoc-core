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

#pragma once

#include <stdio.h>
#include <gv/gvsoc.hpp>
#include <string>

namespace vp {

  class Event_file
  {
  public:
    virtual void dump(int64_t timestamp, int id, uint8_t *event, int width, gv::Vcd_event_type type,
       uint8_t flags, uint8_t *flag_mask) {}
    virtual void close() {}
    virtual void add_trace(std::string name, int id, int width, gv::Vcd_event_type type) {}

  protected:
  };

  class Vcd_file : public Event_file
  {
  public:
    Vcd_file(std::string path);
    void close();
    void add_trace(std::string name, int id, int width, gv::Vcd_event_type type);
    void dump(int64_t timestamp, int id, uint8_t *event, int width, gv::Vcd_event_type type,
        uint8_t flags, uint8_t *flag_mask);

  private:
    std::string parse_path(std::string path, bool begin);
    FILE *file;
    bool header_dumped = false;
    int64_t last_timestamp = -1;
  };
};
