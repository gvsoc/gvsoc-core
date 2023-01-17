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


#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <vp/vp.hpp>
#include <stdio.h>
#include "string.h"
#include <iostream>
#include <sstream>
#include <string>
#include <dlfcn.h>
#include <algorithm>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <regex>
#include <gv/gvsoc.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <vp/time/time_scheduler.hpp>
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <sys/stat.h>

vp::clock_event::clock_event(component_clock *comp, clock_event_meth_t *meth)
    : comp(comp), _this((void *)static_cast<vp::component *>((vp::component_clock *)(comp))), meth(meth),
    enqueued(false), stall_cycle(0)
{
    comp->add_clock_event(this);
    this->clock = comp->get_clock();
}

vp::clock_event::clock_event(component_clock *comp, void *_this, clock_event_meth_t *meth)
: comp(comp), _this(_this), meth(meth), enqueued(false), stall_cycle(0)
{
    comp->add_clock_event(this);
    this->clock = comp->get_clock();
}

vp::clock_event::~clock_event()
{
    comp->remove_clock_event(this);
}
