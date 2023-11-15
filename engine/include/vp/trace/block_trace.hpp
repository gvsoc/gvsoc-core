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

#ifndef __VP_TRACE_block_trace_HPP__
#define __VP_TRACE_block_trace_HPP__

#include "vp/trace/trace.hpp"

using namespace std;

namespace vp {

  class trace;
  class TraceEngine;
  class Block;

  typedef enum {
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    TRACE
  } TraceLevel;

  class BlockTrace
  {

  public:

    BlockTrace(Block &top);

    void new_trace(std::string name, Trace *trace, TraceLevel level);

    void new_trace_event(std::string name, Trace *trace, int width);

    void new_trace_event_string(std::string name, Trace *trace);

    void new_trace_event_real(std::string name, Trace *trace);

    inline TraceEngine *get_trace_manager();

    std::map<std::string, Trace *> traces;
    std::map<std::string, Trace *> trace_events;
    
  protected:

  private:
    void reg_trace(Trace *trace, int event);

    Block &top;

  };

};  


extern vp::TraceEngine *trace_manager;


#endif
