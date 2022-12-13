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

#include "types.hpp"

inline void Iss::perf_event_incr(unsigned int event, int incr)
{
    static uint64_t zero = 0;
    static uint64_t one = 1;

    if (this->pcer_trace_event[event].get_event_active())
    {
        // TODO this is incompatible with frequency scaling, this should be replaced by an event scheduled with cycles
        this->pcer_trace_event[event].event_pulse(incr*this->get_period(), (uint8_t *)&one, (uint8_t *)&zero);
    }
}

inline int Iss::perf_event_is_active(unsigned int event)
{
  return this->pcer_trace_event[event].get_event_active() && this->ext_counter[event].is_bound();
}
