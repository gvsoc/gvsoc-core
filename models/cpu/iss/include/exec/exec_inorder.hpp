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
#include ISS_CORE_INC(class.hpp)

typedef iss_insn_t *(*iss_insn_callback_t)(Iss *iss, iss_insn_t *insn);

class Exec
{
public:
    Exec(Iss &iss);
    void build();
    void reset(bool active);

    inline void stalled_inc();
    inline void stalled_dec();

    void icache_flush();

    inline bool clock_active_get();

    void pc_set(iss_addr_t value);
    // Terminate a previously stalled instruction, by dumping the instruction trace
    inline void insn_terminate();

    inline void insn_stall();
    inline void insn_hold();

    inline bool is_stalled();

    inline iss_insn_t *insn_exec(iss_insn_t *insn);
    inline iss_insn_t *insn_exec_fast(iss_insn_t *insn);

    inline iss_insn_callback_t insn_trace_callback_get();
    inline iss_insn_callback_t insn_stalled_callback_get();
    inline iss_insn_callback_t insn_stalled_fast_callback_get();
    inline bool can_switch_to_fast_mode();
    inline void switch_to_full_mode();

    void dbg_unit_step_check();

    inline void insn_exec_profiling();
    inline void insn_exec_power(iss_insn_t *insn);

    iss_insn_t *current_insn;
    vp::reg_64 stalled;

    vp::trace trace;
    vp::clock_event *instr_event;

    static void exec_instr(void *__this, vp::clock_event *event);
    static void exec_instr_check_all(void *__this, vp::clock_event *event);

    vp::reg_32 bootaddr_reg;
    vp::reg_1 fetch_enable_reg;
    vp::reg_1 is_active_reg;
    vp::reg_1 wfi;
    vp::reg_1 busy;
    int bootaddr_offset;
    iss_insn_t *stall_insn;
    std::vector<iss_resource_instance_t *> resources; // When accesses to the resources are scheduled statically, this gives the instance allocated to this core for each resource

    vp::reg_1 halted;
    vp::reg_1 step_mode;

    iss_insn_t *hwloop_start_insn[2];
    iss_insn_t *hwloop_end_insn[2];
    iss_insn_t *hwloop_next_insn;
    // This is used by HW loop to know that we interrupted and replayed
    // a ELW instructin so that it is not accounted twice in the loop.
    int elw_interrupted;
    bool cache_sync;

    bool debug_mode;
    iss_insn_t *elw_insn;



private:
    static void flush_cache_ack_sync(void *_this, bool active);
    static void clock_sync(void *_this, bool active);
    static void bootaddr_sync(void *_this, uint32_t value);
    static void fetchen_sync(void *_this, bool active);

    Iss &iss;

    vp::wire_master<bool> busy_itf;
    vp::wire_master<bool> flush_cache_req_itf;
    vp::wire_slave<bool> flush_cache_ack_itf;
    vp::wire_slave<uint32_t> bootaddr_itf;
    vp::wire_slave<bool> clock_itf;
    vp::wire_slave<bool> fetchen_itf;

    bool clock_active;
};
