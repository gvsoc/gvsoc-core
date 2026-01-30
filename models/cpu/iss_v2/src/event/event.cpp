/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include <vp/vp.hpp>
#include <cpu/iss_v2/include/iss.hpp>

Event::Event(Iss &iss)
: iss(iss)
{
    this->iss.traces.new_trace_event("state", &state_event, 8);
    this->iss.traces.new_trace_event("pc", &pc_trace_event, 32);
    this->iss.traces.new_trace_event("active_pc", &active_pc_trace_event, 32);
    this->pc_trace_event.register_callback(std::bind(&Trace::insn_trace_callback, &this->iss.trace));
    this->iss.traces.new_trace_event_string("asm", &insn_trace_event);
    this->iss.traces.new_trace_event_string("func", &func_trace_event);
    this->iss.traces.new_trace_event_string("inline_func", &inline_trace_event);
    this->iss.traces.new_trace_event_string("file", &file_trace_event);
    this->iss.traces.new_trace_event("line", &line_trace_event, 32);
    this->iss.traces.new_trace_event_string("binaries", &binaries_trace_event);
    this->iss.traces.new_trace_event_string("user_func", &user_func_trace_event);
    this->iss.traces.new_trace_event_string("user_inline_func", &user_inline_trace_event);
    this->iss.traces.new_trace_event_string("user_file", &user_file_trace_event);
    this->iss.traces.new_trace_event("user_line", &user_line_trace_event, 32);

    if (this->iss.get_js_config()->get("**/insn_groups"))
    {
        js::Config *config = this->iss.get_js_config()->get("**/insn_groups");
        this->insn_groups_power.resize(config->get_size());
        for (int i = 0; i < config->get_size(); i++)
        {
            this->iss.power.new_power_source("power_insn_" + std::to_string(i), &this->insn_groups_power[i], config->get_elem(i));
        }
    }
    else
    {
        this->insn_groups_power.resize(1);
        this->iss.power.new_power_source("power_insn", &this->insn_groups_power[0], this->iss.get_js_config()->get("**/insn"));
    }

    this->iss.power.new_power_source("power_stall_first", &this->power_stall_first, this->iss.get_js_config()->get("**/power_models/stall_first"));
    this->iss.power.new_power_source("power_stall_next", &this->power_stall_next, this->iss.get_js_config()->get("**/power_models/stall_next"));

    this->iss.power.new_power_source("background", &background_power, this->iss.get_js_config()->get("**/power_models/background"));
}
