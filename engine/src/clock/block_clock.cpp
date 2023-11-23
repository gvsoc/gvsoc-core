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

#include <vp/vp.hpp>


void vp::BlockClock::add_clock_event(ClockEvent *event)
{
    this->events.push_back(event);
}

void vp::BlockClock::remove_clock_event(ClockEvent *event)
{
    for (unsigned i=0; i<this->events.size(); ++i)
    {
        if (this->events[i] == event)
        {
            this->events.erase(this->events.begin() + i);
            break;
        }
    }
}
