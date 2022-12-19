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

void Timing::build()
{
    this->iss.traces.new_trace_event("state", &state_event, 8);
    this->iss.traces.new_trace_event("pc", &pc_trace_event, 32);
    this->iss.traces.new_trace_event("active_pc", &active_pc_trace_event, 32);
    this->pc_trace_event.register_callback(std::bind(&Trace::insn_trace_callback, &this->iss.trace));
    this->iss.traces.new_trace_event_string("asm", &insn_trace_event);
    this->iss.traces.new_trace_event_string("func", &func_trace_event);
    this->iss.traces.new_trace_event_string("inline_func", &inline_trace_event);
    this->iss.traces.new_trace_event_string("file", &file_trace_event);
    this->iss.traces.new_trace_event_string("binaries", &binaries_trace_event);
    this->iss.traces.new_trace_event("line", &line_trace_event, 32);

    this->iss.traces.new_trace_event_real("ipc_stat", &ipc_stat_event);
}

void Timing::ipc_start_gen(bool pulse)
{
    if (!pulse)
        this->ipc_stat_event.event_real((float)this->ipc_stat_nb_insn / 100);
    else
        this->ipc_stat_event.event_real_pulse(this->iss.get_period(), (float)this->ipc_stat_nb_insn / 100, 0);

    this->ipc_stat_nb_insn = 0;
    if (this->ipc_stat_delay == 10)
        this->ipc_stat_delay = 30;
    else
        this->ipc_stat_delay = 100;
}

void Timing::ipc_stat_trigger()
{
    // In case the core is resuming execution, set IPC to 1 as we are executing
    // first instruction to not have the signal to zero.
    if (this->ipc_stat_delay == 10)
    {
        this->ipc_stat_event.event_real(1.0);
    }

    if (this->ipc_stat_event.get_event_active() && !this->ipc_clock_event->is_enqueued() &&
        this->iss.exec.is_active_reg.get() && this->iss.exec.clock_active_get())
    {
        this->iss.event_enqueue(this->ipc_clock_event, this->ipc_stat_delay);
    }
}

void Timing::ipc_stat_stop()
{
    this->ipc_start_gen(true);
    if (this->ipc_clock_event->is_enqueued())
    {
        this->iss.event_cancel(this->ipc_clock_event);
    }
    this->ipc_stat_delay = 10;
}

void Timing::ipc_stat_handler(void *__this, vp::clock_event *event)
{
    Timing *_this = (Timing *)__this;
    _this->ipc_start_gen();
    _this->ipc_stat_trigger();
}
