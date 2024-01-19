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
#include <cpu/iss/include/types.hpp>

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
    inline void insn_stall_account();
    inline void cycle_account();
    inline void insn_stall_start();
    inline void insn_stall_stop();

    inline void event_load_account(int incr);
    inline void event_rvc_account(int incr);
    inline void event_store_account(int incr);
    inline void event_branch_account(int incr);
    inline void event_taken_branch_account(int incr);
    inline void event_jump_account(int incr);
    inline void event_misaligned_account(int incr);
    inline void event_apu_contention_account(int incr);
    inline void event_load_load_account(int incr);

    inline void event_trace_account(unsigned int event, int cycles);
    inline void event_trace_set(unsigned int event);
    inline void event_trace_reset(unsigned int event);
    inline int event_trace_is_active(unsigned int event);

    inline void stall_cycles_account(int incr);

    inline void event_account(unsigned int event, int incr);
    inline void handle_pending_events();

    void reset(bool active);

    vp::ClockEvent *ipc_clock_event;
    vp::Trace state_event;
    vp::Trace pc_trace_event;
    vp::Trace active_pc_trace_event;
    vp::Trace func_trace_event;
    vp::Trace inline_trace_event;
    vp::Trace line_trace_event;
    vp::Trace file_trace_event;
    vp::Trace binaries_trace_event;
    vp::Trace pcer_trace_event[32];
    int64_t pcer_trace_pending_cycles[32];
    vp::Trace insn_trace_event;
    vp::WireMaster<uint32_t> ext_counter[32];
    std::vector<vp::PowerSource> insn_groups_power;
    vp::PowerSource power_stall_first;
    vp::PowerSource power_stall_next;
    vp::PowerSource background_power;
    uint32_t pcer_trace_active_events;

private:

    Iss &iss;
    bool declare_binaries = true;
};
