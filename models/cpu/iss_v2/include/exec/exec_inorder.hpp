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

// In-order single-issue dispatcher + held-insn tracker.
//
// Owns the per-cycle event that fetches the next insn, dispatches it
// through decode and the appropriate handler, and accounts for stalls
// (resource conflicts, scoreboard hazards, retain/halt, WFI). Held
// insns (LSU async loads and WFI) are parked through `insn_hold`; the
// hold is released by `insn_terminate` from the corresponding response
// path. Two optional features are gated by build-time macros:
//
// - `CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD`: a single-slot 1-cycle
//   delayed scoreboard release, used by `LsuV2` to model the load-use
//   stall. `schedule_scoreboard_release(mask)` parks the response's
//   destination mask; `task_unblock_handle` fires next cycle and
//   clears the bits via `Regfile::sb_reg_invalid_clear_mask`. One
//   retire per cycle is enforced by `LsuV2::next_retire_cycle`, so a
//   single slot is always sufficient even with `nb_outstanding > 1`.
//
// - `CONFIG_GVSOC_ISS_EXEC_INORDER_COMMIT`: an in-order commit FIFO
//   of `InsnEntry` carrying every dispatched insn from dispatch to
//   commit. Held async insns enter with `ready=false`; sync followers
//   dispatched in the meantime enter with `ready=true` (their result
//   is in the regfile and their scoreboard bits are cleared at
//   dispatch, but their trace dump is deferred). `insn_terminate`
//   flips the passed entry's ready bit and drains from the head
//   while heads are ready — guaranteeing dispatch-order trace output
//   regardless of how many async insns are in flight or how their
//   responses interleave.
//
// Both features can be enabled independently or together, and both
// compile out to nothing when not enabled.
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

    virtual inline bool can_switch_to_fast_mode();
    inline void switch_to_full_mode();

    inline void enqueue_task(Task *task);

    void busy_enter();
    void busy_exit();

    iss_insn_t *get_insn(InsnEntry *entry);
    inline void insn_stall() { this->is_insn_stalled = true; }
    InsnEntry *insn_hold(iss_insn_t *insn);
    // `defer_scoreboard_release=true` skips the immediate
    // `scoreboard_insn_end` at commit. Used by `LsuV2` when paired
    // with `CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD`: the load's dest
    // regs are released one cycle later through
    // `schedule_scoreboard_release` so a dependent insn dispatched
    // the same cycle the response lands still sees them in flight
    // (load-use stall). All other callers (vector unit, WFI, Lsu v1,
    // snitch variants) leave it false so the scoreboard is cleared
    // here as before.
    void insn_terminate(InsnEntry *entry, bool defer_scoreboard_release = false);
#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
public:
    // Park the regs in `mask` in the single-slot `unblock_slot`, so
    // their `sb_reg_invalid_clear` runs one cycle later. Used by
    // `LsuV2` on retire to model the load-use writeback delay (the
    // load's dest is valid the cycle after the response arrives).
    // Available to any core wired with a scoreboard, regardless of
    // whether it opts into the in-order-commit trace mechanism.
    void schedule_scoreboard_release(uint64_t mask);
private:
    static void task_unblock_handle(Iss *iss, Task *task);
public:
#endif
#ifdef CONFIG_GVSOC_ISS_EXEC_INORDER_COMMIT
private:
    // Trace dump + return entry to the InsnEntry pool + timing
    // accounting. No scoreboard release here — the caller is
    // responsible for scheduling it (delayed via
    // `schedule_scoreboard_release` for retired loads, immediate via
    // `Regfile::scoreboard_insn_end` for deferred sync followers).
    void drain_entry(InsnEntry *entry);
public:
#endif

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

    bool debug_mode = false;

    bool skip_irq_check;

    bool has_exception;
    iss_reg_t exception_pc;

    int insn_table_index;

    // This tells if interrupts should not be handled, due to an on-going activity,
    // like a page-table walk or a push/pop instruction in its atomic phase.
    int irq_locked;

    bool pending_flush;

protected:
    Iss &iss;

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

#ifdef CONFIG_GVSOC_ISS_EXEC_INORDER_COMMIT
public:
    // In-order commit FIFO. While the queue is non-empty its head is
    // a held insn whose response (LSU async load, or WFI wakeup) has
    // not yet arrived; sync followers dispatched in the meantime are
    // appended to the tail with `ready=true` so their commit (trace
    // dump + entry release) is deferred behind the head, even though
    // their result was already written to the regfile at dispatch.
    // `insn_terminate` flips the passed entry's ready bit and drains
    // from the head while heads are ready, releasing as many entries
    // in program order as became ready since the previous call. The
    // queue grows to arbitrary depth, supporting any number of
    // outstanding async insns (LsuV2 nb_outstanding > 1, multiple
    // followers behind them, etc.).
    InsnEntry *queue_head;
    InsnEntry *queue_tail;
#endif

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
public:
    // Single-slot delayed scoreboard release. When LsuV2 retires a
    // load (or any multi-dest LSU insn — `unblock_slot_mask` is a
    // register bitmask, so all destinations written in the same
    // response cycle get their bits cleared together), the response's
    // dest reg(s) are parked here for one cycle before
    // `sb_reg_invalid_clear_mask` is called — that one-cycle delay
    // models the load-use stall (registers are observable only the
    // cycle after the response). Drained either by `unblock_task` at
    // the next cycle, or pre-emptively by the next
    // `schedule_scoreboard_release` call if the new push finds the
    // slot already due. Single slot is sufficient because LsuV2
    // elects one retire per cycle (see `LsuV2::next_retire_cycle`):
    // every push lands exactly one cycle after the previous one, so
    // the parked mask is always already due by the time the next
    // push arrives. Independent of `CONFIG_GVSOC_ISS_EXEC_INORDER_COMMIT`:
    // load-use stall is a property of the LSU/scoreboard pairing,
    // not of the trace-ordering policy.
    uint64_t unblock_slot_mask;
    int64_t unblock_slot_timestamp;
    bool unblock_slot_valid;
    bool unblock_task_pending;
    Task unblock_task;
#endif
};
