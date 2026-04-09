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
state_event(iss, "state", 8, gv::Vcd_event_type_logical, "Core state (active, idle, stall)"),
pc_trace_event(iss, "pc", ISS_REG_WIDTH, gv::Vcd_event_type_logical, "Program counter value"),

event_cycles(iss, "event_cycles", 1, gv::Vcd_event_type_logical, "Active cycles. This indicates when the core is active. The core can be either executing an instruction or doing something else like handling a stall."),
event_instr(iss, "event_instr", 1, gv::Vcd_event_type_logical, "Instructions. This indicates when the core is executing an instruction."),
event_fetch(iss, "event_fetch", 1, gv::Vcd_event_type_logical, "Prefetch refills. This indicates when the core prefetcher generates a prefetch refill."),
event_ld_stall(iss, "event_ld_stall", 1, gv::Vcd_event_type_logical, "Load data dependency stalls. This indicates when the core is blocked because an instruction tries to use the result of a load which is still pending."),
event_jmp_stall(iss, "event_jmp_stall", 1, gv::Vcd_event_type_logical, "Jump stalls. This indicates when the core is stalled due to the flush required by a jump."),
event_imiss(iss, "event_imiss", 1, gv::Vcd_event_type_logical, "Instruction cache misses. This indicates when the core is stalled waiting for the next instruction, for example when the prefetch buffer is being refilled."),
event_ld(iss, "event_ld", 1, gv::Vcd_event_type_logical, "Load memory accesses. This indicates when the core is issuing a load request."),
event_st(iss, "event_st", 1, gv::Vcd_event_type_logical, "Store memory accesses. This indicates when the core is issuing a store request."),
event_jump(iss, "event_jump", 1, gv::Vcd_event_type_logical, "Unconditional jumps. This indicates when the core is executing an unconditional jump instruction."),
event_branch(iss, "event_branch", 1, gv::Vcd_event_type_logical, "Conditional branches. This indicates when the core is executing a conditional jump instruction (branch)."),
event_taken_branch(iss, "event_taken_branch", 1, gv::Vcd_event_type_logical, "Conditional branch takens. This indicates when the core is executing a branch instruction and is taking the branch."),
event_rvc(iss, "event_rvc", 1, gv::Vcd_event_type_logical, "Compressed (RVC) instructions. This indicates the current instruction is a compressed one (16 bits instead of 32)"),
event_misaligned(iss, "event_misaligned", 1, gv::Vcd_event_type_logical, "Misaligned memory accesses. This indicates when the core issues a request which is misaligned, and needs to be handled with multiple requests."),
active_pc_trace_event(iss, "active_pc", ISS_REG_WIDTH, gv::Vcd_event_type_logical, "Program counter (only when active)")
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
