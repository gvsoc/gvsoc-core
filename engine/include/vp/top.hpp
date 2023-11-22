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

#include "vp/component.hpp"
#include "vp/time/time_engine.hpp"

namespace vp {

  class top
  {
  public:
      top(std::string config_path, bool is_async);
      ~top();

      Component *top_instance;
      js::Config *gv_config;

      vp::TimeEngine *get_time_engine() { return this->time_engine; };
      vp::TraceEngine *get_trace_engine() { return this->trace_engine; };
      vp::PowerEngine *get_power_engine() { return this->power_engine; };


  private:
      vp::TimeEngine *time_engine;
      vp::TraceEngine *trace_engine;
      vp::PowerEngine *power_engine;
  };

};
