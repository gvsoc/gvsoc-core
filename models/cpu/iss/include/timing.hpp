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

#include <vp/vp.hpp>
#include <types.hpp>

class Timing
{
public:
    Timing(Iss &iss);
    void build();
    inline void stall_fetch_account(int count);
    inline void stall_taken_branch_account();
    inline void stall_insn_account(int cycles);
    inline void stall_insn_dependency_account(int latency);
    inline void stall_load_dependency_account(int latency);
    inline void stall_jump_account();
    inline void stall_misaligned_account();
    inline void stall_load_account(int cycles);
    inline void insn_account();

    inline void event_load_account(int incr);
    inline void event_rvc_account(int incr);
    inline void event_store_account(int incr);
    inline void event_branch_account(int incr);
    inline void event_taken_branch_account(int incr);
    inline void event_jump_account(int incr);
    inline void event_misaligned_account(int incr);
    inline void event_insn_contention_account(int incr);

    inline void event_trace_account(unsigned int event, int cycles);
    inline int event_trace_is_active(unsigned int event);

    inline void stall_cycles_account(int incr);

    void reset(bool active);

    static void ipc_stat_handler(void *__this, vp::clock_event *event);
    void ipc_start_gen(bool pulse = false);
    void ipc_stat_trigger();
    void ipc_stat_stop();

    int ipc_stat_nb_insn;
    vp::trace ipc_stat_event;
    vp::clock_event *ipc_clock_event;
    int ipc_stat_delay;
    vp::trace state_event;
    vp::trace pc_trace_event;
    vp::trace active_pc_trace_event;
    vp::trace func_trace_event;
    vp::trace inline_trace_event;
    vp::trace line_trace_event;
    vp::trace file_trace_event;
    vp::trace binaries_trace_event;
    vp::trace pcer_trace_event[32];
    vp::trace insn_trace_event;
    vp::wire_master<uint32_t> ext_counter[32];
    std::vector<vp::power::power_source> insn_groups_power;
    vp::power::power_source background_power;


private:
    inline void event_account(unsigned int event, int incr);

    Iss &iss;
};
