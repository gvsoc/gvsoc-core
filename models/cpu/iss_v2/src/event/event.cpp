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

#include <vp/trace/trace.hpp>
#include <cpu/iss_v2/include/event/event.hpp>

Events::Events(Iss &iss)
: iss(iss),
state_event(iss, "state", 8),
pc_trace_event(iss, "pc", ISS_REG_WIDTH),

event_cycles(iss, "event_cycles", 1),
event_instr(iss, "event_instr", 1),
event_ld_stall(iss, "event_ld_stall", 1),
event_jmp_stall(iss, "event_jmp_stall", 1),
event_imiss(iss, "event_imiss", 1),
event_ld(iss, "event_ld", 1),
event_st(iss, "event_st", 1),
event_jump(iss, "event_jump", 1),
event_branch(iss, "event_branch", 1),
event_taken_branch(iss, "event_taken_branch", 1),
event_rvc(iss, "event_rvc", 1),
event_misaligned(iss, "event_misaligned", 1),
active_pc_trace_event(iss, "active_pc", ISS_REG_WIDTH)
{
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

void Events::reset(bool active)
{
    if (active)
    {
        uint64_t zero = 0;
        this->event_cycles.dump((uint8_t *)&zero);
        this->event_instr.dump((uint8_t *)&zero);
        this->event_ld_stall.dump((uint8_t *)&zero);
        this->event_jmp_stall.dump((uint8_t *)&zero);
        this->event_imiss.dump((uint8_t *)&zero);
        this->event_ld.dump((uint8_t *)&zero);
        this->event_st.dump((uint8_t *)&zero);
        this->event_jump.dump((uint8_t *)&zero);
        this->event_branch.dump((uint8_t *)&zero);
        this->event_taken_branch.dump((uint8_t *)&zero);
        this->event_rvc.dump((uint8_t *)&zero);

        this->active_pc_trace_event.dump_highz();
    }
    else
    {
        this->iss.exec.busy_exit();
    }
}
