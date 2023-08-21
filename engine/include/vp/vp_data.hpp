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


#ifndef __VP_VP_DATA_HPP__
#define __VP_VP_DATA_HPP__

#include "vp/component.hpp"
#include "vp/clock/clock_event.hpp"
#include "vp/clock/clock_engine.hpp"
#include "gv/power.hpp"

namespace vp {

  class top
  {
  public:
      top(std::string config_path, bool is_async);
      ~top();

      vp::time_engine *time_engine_get() { return this->time_engine; }

      component *top_instance;
      power::engine *power_engine;
      vp::trace_domain *trace_engine;
      vp::time_engine *time_engine;
  private:
  };

};

#endif
