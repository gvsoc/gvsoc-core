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

    void debug_req();

    bool user_access(iss_addr_t addr, uint8_t *data, iss_addr_t size, bool is_write);
    std::string read_user_string(iss_addr_t addr, int len = -1);

    static vp::io_req_status_e dbg_unit_req(void *__this, vp::io_req *req);
    inline void iss_exec_insn_check_debug_step();

    void set_halt_mode(bool halted, int cause);

    void handle_ebreak();
    void handle_riscv_ebreak();

    void dump_debug_traces();

    void insn_trace_callback();

    void declare_pcer(int index, std::string name, std::string help);


    void pc_set(iss_addr_t value);

    /*
     * Performance events
     */

    inline void perf_event_incr(unsigned int event, int incr);
    inline int perf_event_is_active(unsigned int event);

    static void clock_sync(void *_this, bool active);
    static void bootaddr_sync(void *_this, uint32_t value);
    static void fetchen_sync(void *_this, bool active);
    static void flush_cache_sync(void *_this, bool active);
    static void flush_cache_ack_sync(void *_this, bool active);
    static void halt_sync(void *_this, bool active);
    void halt_core();

    static void ipc_stat_handler(void *__this, vp::clock_event *event);
    void gen_ipc_stat(bool pulse = false);
    void trigger_ipc_stat();
    void stop_ipc_stat();

    Prefetcher prefetcher;
    Exec exec;
    Decode decode;
    Timing timing;
    Irq irq;
    Gdbserver gdbserver;
    Lsu lsu;


    vp::io_master data;
    vp::io_master fetch;
    vp::io_slave dbg_unit;

    vp::wire_slave<int> irq_req_itf;
    vp::wire_master<int> irq_ack_itf;
    vp::wire_master<bool> busy_itf;

    vp::wire_master<bool> flush_cache_req_itf;
    vp::wire_slave<bool> flush_cache_ack_itf;

    vp::wire_master<uint32_t> ext_counter[32];

    vp::io_req io_req;

    vp::trace trace;
    vp::trace decode_trace;
    vp::trace insn_trace;
    vp::trace csr_trace;
    vp::trace perf_counter_trace;

    vp::reg_32 bootaddr_reg;
    vp::reg_1 fetch_enable_reg;
    vp::reg_1 is_active_reg;
    vp::reg_1 wfi;
    vp::reg_1 misaligned_access;
    vp::reg_1 halted;
    vp::reg_1 step_mode;
    vp::reg_1 do_step;

    vp::reg_1 elw_stalled;

    std::vector<vp::power::power_source> insn_groups_power;
    vp::power::power_source background_power;

    vp::trace state_event;
    vp::reg_1 busy;
    vp::trace pc_trace_event;
    vp::trace active_pc_trace_event;
    vp::trace func_trace_event;
    vp::trace inline_trace_event;
    vp::trace line_trace_event;
    vp::trace file_trace_event;
    vp::trace binaries_trace_event;
    vp::trace pcer_trace_event[32];
    vp::trace insn_trace_event;

    Iss_pcer_info_t pcer_info[32];
    int64_t cycle_count_start;
    int64_t cycle_count;

    iss_insn_cache_t insn_cache;
    iss_insn_t *current_insn;
    iss_insn_t *prev_insn;
    iss_insn_t *stall_insn;
    iss_regfile_t regfile;
    iss_cpu_state_t state;
    iss_config_t config;
    iss_csr_t csr;
    iss_pulpv2_t pulpv2;
    iss_pulp_nn_t pulp_nn;
    iss_rnnext_t rnnext;
    iss_corev_t corev;
    std::vector<iss_resource_instance_t *> resources; // When accesses to the resources are scheduled statically, this gives the instance allocated to this core for each resource

    bool dump_trace_enabled;

    int ipc_stat_nb_insn;
    vp::trace ipc_stat_event;
    vp::clock_event *ipc_clock_event;
    int ipc_stat_delay;

#ifdef USE_TRDB
    trdb_ctx *trdb;
    struct list_head trdb_packet_list;
    uint8_t trdb_pending_word[16];
#endif

    int irq_req;
    int irq_req_value;

    bool iss_opened;
    int halt_cause;
    int64_t wakeup_latency;
    int bootaddr_offset;
    iss_reg_t hit_reg = 0;
    bool riscv_dbg_unit;

    iss_reg_t ppc;
    iss_reg_t npc;

    int misaligned_size;
    uint8_t *misaligned_data;
    iss_addr_t misaligned_addr;
    bool misaligned_is_write;

    vp::wire_slave<uint32_t> bootaddr_itf;
    vp::wire_slave<bool> clock_itf;
    vp::wire_slave<bool> fetchen_itf;
    vp::wire_slave<bool> flush_cache_itf;
    vp::wire_slave<bool> halt_itf;
    vp::wire_master<bool> halt_status_itf;

    bool clock_active;

};

