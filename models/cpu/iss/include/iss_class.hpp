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

#include <lsu.hpp>
#include <decode.hpp>
#include <trace.hpp>
#include <csr.hpp>
#include <dbgunit.hpp>
#include <syscalls.hpp>
#include <timing.hpp>
#include <irq/irq_external.hpp>
#include <exec/exec_inorder.hpp>
#include <prefetch/prefetch_single_line.hpp>
#include <gdbserver.hpp>

class Iss : public vp::component
{

public:
    Iss(js::config *config);

    int build();
    void start();
    void pre_reset();
    void reset(bool active);
    int iss_open();
    virtual void target_open();

    Prefetcher prefetcher;
    Exec exec;
    Decode decode;
    Timing timing;
    Irq irq;
    Gdbserver gdbserver;
    Lsu lsu;
    DbgUnit dbgunit;
    Syscalls syscalls;
    Trace trace;
    Csr csr;

    // power
    std::vector<vp::power::power_source> insn_groups_power;
    vp::power::power_source background_power;

    // syscalls
    Iss_pcer_info_t pcer_info[32];
    int64_t cycle_count_start;
    int64_t cycle_count;

    // trace
    bool dump_trace_enabled;


    // timing
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



    // wrapper
    vp::trace wrapper_trace;
    bool iss_opened;
    iss_regfile_t regfile;
    iss_cpu_state_t state;
    iss_config_t config;
    iss_pulpv2_t pulpv2;
    iss_pulp_nn_t pulp_nn;
    iss_rnnext_t rnnext;
    iss_corev_t corev;



    // irq
    int irq_req;
    int irq_req_value;
    vp::wire_master<int> irq_ack_itf;
    vp::wire_slave<int> irq_req_itf;

    // lsu
    vp::io_master data;
    vp::io_req io_req;
    int64_t wakeup_latency;
    int misaligned_size;
    uint8_t *misaligned_data;
    iss_addr_t misaligned_addr;
    bool misaligned_is_write;
    vp::reg_1 elw_stalled;
    vp::reg_1 misaligned_access;


    // fetch
    vp::io_master fetch;


    // decode
    vp::wire_slave<bool> flush_cache_itf;
    iss_insn_cache_t insn_cache;

    // dbg unit
    vp::io_slave dbg_unit;
    vp::wire_slave<bool> halt_itf;
    vp::wire_master<bool> halt_status_itf;
    iss_reg_t ppc;
    iss_reg_t npc;
    bool riscv_dbg_unit;
    iss_reg_t hit_reg = 0;
    int halt_cause;
    vp::reg_1 halted;
    vp::reg_1 step_mode;
    vp::reg_1 do_step;



};
