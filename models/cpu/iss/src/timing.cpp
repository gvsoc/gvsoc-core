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
#include "iss.hpp"

Timing::Timing(Iss &iss)
    : iss(iss)
{
}

void Timing::ipc_start_gen(bool pulse)
{
    if (!pulse)
        this->iss.ipc_stat_event.event_real((float)this->iss.ipc_stat_nb_insn / 100);
    else
        this->iss.ipc_stat_event.event_real_pulse(this->iss.get_period(), (float)this->iss.ipc_stat_nb_insn / 100, 0);

    this->iss.ipc_stat_nb_insn = 0;
    if (this->iss.ipc_stat_delay == 10)
        this->iss.ipc_stat_delay = 30;
    else
        this->iss.ipc_stat_delay = 100;
}

void Timing::ipc_stat_trigger()
{
    // In case the core is resuming execution, set IPC to 1 as we are executing
    // first instruction to not have the signal to zero.
    if (this->iss.ipc_stat_delay == 10)
    {
        this->iss.ipc_stat_event.event_real(1.0);
    }

    if (this->iss.ipc_stat_event.get_event_active() && !this->iss.ipc_clock_event->is_enqueued() && this->iss.is_active_reg.get() && this->iss.clock_active)
    {
        this->iss.event_enqueue(this->iss.ipc_clock_event, this->iss.ipc_stat_delay);
    }
}

void Timing::ipc_stat_stop()
{
    this->ipc_start_gen(true);
    if (this->iss.ipc_clock_event->is_enqueued())
    {
        this->iss.event_cancel(this->iss.ipc_clock_event);
    }
    this->iss.ipc_stat_delay = 10;
}

void Timing::ipc_stat_handler(void *__this, vp::clock_event *event)
{
    Timing *_this = (Timing *)__this;
    _this->ipc_start_gen();
    _this->ipc_stat_trigger();
}
