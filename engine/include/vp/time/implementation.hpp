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


#include <map>
#include <list>
#include <string>
#include <vector>

#include "vp/vp.hpp"
#include "vp/clock/implementation.hpp"


inline vp::TimeEngine *vp::BlockTime::get_engine()
{
    return this->time_engine;
}

inline void vp::TimeEngine::update(int64_t time)
{
    if (time > this->time)
        this->time = time;
}

inline int64_t vp::TimeEngine::get_next_event_time()
{
    return this->first_client ? this->first_client->time.next_event_time : -1;
}

inline bool vp::BlockTime::enqueue_to_engine(int64_t time)
{
    return this->get_engine()->enqueue(&this->top, time);
}

inline bool vp::BlockTime::dequeue_from_engine()
{
    return this->get_engine()->dequeue(&this->top);
}

inline int64_t vp::BlockTime::get_time()
{
    return this->get_engine()->get_time();
}
