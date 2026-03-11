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
#include <cpu/iss_v2/include/types.hpp>
#include <cpu/iss_v2/include/insn.hpp>
#include <cpu/iss_v2/include/offload.hpp>
#include <cpu/iss_v2/include/task.hpp>

class ExecInOrder
{
public:
    ExecInOrder(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    void retain_inc();
    void retain_dec();

    void icache_flush();

    void pc_set(iss_addr_t value);

    void sleep_enter(iss_insn_t *insn);
    void sleep_exit();

    inline bool is_retained() { return this->retained.get() > 0; }

    inline iss_reg_t insn_exec(iss_insn_t *insn, iss_reg_t pc);
    inline iss_reg_t insn_exec_fast(iss_insn_t *insn, iss_reg_t pc);

    inline bool can_switch_to_fast_mode();
    inline void switch_to_full_mode();

    inline void enqueue_task(Task *task);

    void busy_enter();
    void busy_exit();

    iss_insn_t *get_insn(InsnEntry *entry);
    inline void insn_stall() { this->is_insn_stalled = true; }
    InsnEntry *insn_hold(iss_insn_t *insn);
    void insn_terminate(InsnEntry *entry);

    void dbg_unit_step_check();

    inline void insn_exec_profiling();
    inline void insn_exec_power(iss_insn_t *insn);

    inline void interrupt_taken();

    inline void stall_cycles_inc(int inc) { this->stall_cycles += inc; }

    iss_reg_t current_insn;
    vp::ClockEvent instr_event;
    vp::reg_64 retained;

    vp::Trace trace;

    static void exec_instr(vp::Block *__this, vp::ClockEvent *event);
    static void exec_instr_check_all(vp::Block *__this, vp::ClockEvent *event);

    vp::reg_32 bootaddr_reg;
    vp::reg_1 fetch_enable_reg;
    vp::reg_1 wfi;
    int64_t wfi_start;
    vp::reg_1 busy;

    bool instr_disabled;

    std::vector<iss_resource_instance_t *> resources; // When accesses to the resources are scheduled statically, this gives the instance allocated to this core for each resource

    vp::reg_1 halted;
    vp::reg_1 step_mode;
    vp::reg_1 irq_enter;
    vp::reg_1 irq_exit;

    bool cache_sync;

    bool debug_mode;

    bool skip_irq_check;

    bool has_exception;
    iss_reg_t exception_pc;

    int insn_table_index;

    // This tells if interrupts should not be handled, due to an on-going activity,
    // like a page-table walk or a push/pop instruction in its atomic phase.
    int irq_locked;

    bool pending_flush;

private:
    void retain_check();
    static void flush_cache_ack_sync(vp::Block *_this, bool active);
    static void clock_sync(vp::Block *_this, bool active);
    static void bootaddr_sync(vp::Block *_this, uint32_t value);
    static void fetchen_sync(vp::Block *_this, bool active);
    void bootaddr_apply(uint32_t value);
    inline InsnEntry *get_entry();
    inline void release_entry(InsnEntry *entry);
    inline bool handle_tasks();

    Iss &iss;

    vp::WireMaster<bool> busy_itf;
    vp::WireMaster<bool> flush_cache_req_itf;
    vp::WireSlave<bool> flush_cache_ack_itf;
    vp::WireSlave<uint32_t> bootaddr_itf;
    vp::WireSlave<bool> clock_itf;
    vp::WireSlave<bool> fetchen_itf;

    bool clock_active;

    vp::Trace asm_trace_event;

    bool is_insn_stalled;
    bool is_insn_hold;
    InsnEntry *first_entry = NULL;
    Task *first_task;
    InsnEntry *wfi_entry;
    int stall_cycles;
};
