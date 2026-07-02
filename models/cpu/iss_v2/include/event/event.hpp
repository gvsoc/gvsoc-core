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
#include <cpu/iss_v2/include/stats/insn_duration.hpp>

// Opaque per-register tag stored by the scoreboard at invalidation
// time and handed back to the per-core events class when a dependent
// instruction stalls. The scoreboard itself never interprets the
// value — only Events (and its per-core overrides) does. Adding a
// new category is a matter of adding an enumerator here, having the
// producer subsystem call sb_set_reason(mask, NEW_REASON), and giving
// the per-core event_scoreboard_stall a new switch arm.
enum IssStallReason : uint8_t {
    ISS_STALL_REASON_NONE = 0,
    ISS_STALL_REASON_LOAD = 1,
};

#ifdef CONFIG_GVSOC_STATS_ACTIVE
// Derived statistic: instructions per non-idle cycle (IPC), computed
// lazily at dump time from the live instr and active-cycle counters.
// Lives in the model (not the engine) so no engine change is needed —
// register_stat() accepts any vp::StatCommon.
class StatIpc : public vp::StatCommon
{
public:
    StatIpc(vp::StatScalar *instr, vp::StatScalar *active_cycles)
        : instr(instr), active_cycles(active_cycles) {}

    std::string format_value(bool raw) const override
    {
        uint64_t cycles = this->active_cycles->get();
        double ipc = cycles ? (double)this->instr->get() / (double)cycles : 0.0;
        char buf[32];
        snprintf(buf, sizeof(buf), raw ? "%.6f" : "%.3f", ipc);
        return buf;
    }

    void reset() override {}

private:
    vp::StatScalar *instr;
    vp::StatScalar *active_cycles;
};

// Derived statistic: percentage of time the core was active (not idle),
// computed from the active and idle cycle counters which together tile
// the whole run.
class StatActivePct : public vp::StatCommon
{
public:
    StatActivePct(vp::StatScalar *active_cycles, vp::StatScalar *idle_cycles)
        : active_cycles(active_cycles), idle_cycles(idle_cycles) {}

    std::string format_value(bool raw) const override
    {
        uint64_t active = this->active_cycles->get();
        uint64_t total = active + this->idle_cycles->get();
        double pct = total ? 100.0 * (double)active / (double)total : 0.0;
        char buf[32];
        snprintf(buf, sizeof(buf), raw ? "%.4f" : "%.2f %%", pct);
        return buf;
    }

    void reset() override {}

private:
    vp::StatScalar *active_cycles;
    vp::StatScalar *idle_cycles;
};
#endif

class Events
{
public:

    Events(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    inline void stall_fetch_account(int count){}
    inline void stall_taken_branch_account(){}
    inline void stall_insn_account(int cycles){}
    inline void stall_insn_dependency_account(int latency){}
    inline void stall_load_dependency_account(int latency){}
    inline void stall_jump_account(){}
    inline void stall_misaligned_account(){}
    inline void stall_load_account(int cycles){}
    inline void insn_account(){}
    inline void insn_stall_account(){}
    inline void cycle_account(){}
    inline void insn_stall_start(){}
    inline void insn_stall_stop(){}

    virtual inline void event_cycle_enable();
    virtual inline void event_cycle_disable();
    virtual inline void event_instr_account();
    virtual inline void event_fetch_account();
    virtual inline void event_imiss_account(int incr);
    virtual inline void event_imiss_start();
    virtual inline void event_imiss_stop();
    virtual inline void event_load_account(int incr);
    virtual inline void event_rvc_account(int incr);
    virtual inline void event_store_account(int incr);
    virtual inline void event_branch_account();
    virtual inline void event_taken_branch_account();
    virtual inline void event_jump_account();
    // Called once per executed JALR with the rs1 register index. Lets a
    // core (e.g. Ri5kyEvents) charge the taken-jump pipeline flush and
    // detect the RI5CY-style jr_stall (rs1 produced by a recent
    // instruction). Default: no-op.
    virtual inline void event_jalr_account(int rs1) {}
    // Called when an instruction retires, with the retiring insn. Lets a
    // core track the previous instruction's destination register so the
    // jalr-with-producer hazard can be detected by event_jalr_account.
    // Default: no-op.
    virtual inline void event_retire_account(iss_insn_t *insn) {}
    // Called when a retiring insn carries a non-zero per-instruction
    // latency (set by the decoder, typically via a setup pass that walks
    // get_insns_from_tag(...) and assigns u.insn.latency = N). The hook
    // lets each core pick a stall policy that matches its pipeline:
    // unconditional structural stall (e.g. RI5CY MULH FSM blocking the
    // ID->EX port), scoreboard-timestamp only (dependency-aware), resource
    // contention, etc. Default: no-op, so cores that don't tag any insn
    // and don't override this pay nothing beyond the latency != 0 check.
    virtual inline void event_insn_latency_account(iss_insn_t *insn, int latency) {}
    // Called by the shared rv32m.hpp div/divu/rem/remu handlers with the
    // live operand values, so per-core events classes can model
    // operand-dependent latency (e.g. the RI5CY divider's serial
    // bit-iteration loop whose cycle count depends on the divisor's
    // leading-zero count). Default: no-op.
    virtual inline void event_div_account(iss_reg_t dividend, iss_reg_t divisor,
                                          bool is_signed, bool is_rem) {}
    virtual inline void event_misaligned_account(int incr);
    virtual inline void event_apu_contention_account(int incr){}
    virtual inline void event_load_load_account(int incr){}
    // Called by Regfile::scoreboard_insn_check whenever an instruction
    // is stalled by the scoreboard, with the opaque reason byte that
    // was stored when the blocking register was invalidated (see
    // IssStallReason). Per-core events classes interpret the reason
    // and fire the matching producer-specific stall counter
    // (e.g. PCCR_LD_STALL for ISS_STALL_REASON_LOAD on Ri5ky). The
    // scoreboard itself never names a producer. Default: no-op.
    virtual inline void event_scoreboard_stall(uint8_t reason) {}

    inline void event_trace_account(unsigned int event, int cycles){}
    inline void event_trace_set(unsigned int event){}
    inline void event_trace_reset(unsigned int event){}
    inline int event_trace_is_active(unsigned int event){ return 0;}

    inline void stall_cycles_account(int incr){}

    inline void event_account(unsigned int event, int incr){}
    inline void handle_pending_events(){}

    vp::Event state_event;
    vp::Event pc_trace_event;


    vp::Event active_pc_trace_event;
    std::vector<vp::PowerSource> insn_groups_power;
    vp::PowerSource power_stall_first;
    vp::PowerSource power_stall_next;
    vp::PowerSource background_power;

protected:

    Iss &iss;

    vp::Event event_cycles;
    vp::Event event_instr;
    vp::Event event_fetch;
    vp::Event event_ld_stall;
    vp::Event event_jmp_stall;
    vp::Event event_imiss;
    vp::Event event_ld;
    vp::Event event_st;
    vp::Event event_jump;
    vp::Event event_branch;
    vp::Event event_taken_branch;
    vp::Event event_rvc;
    vp::Event event_misaligned;

#ifdef CONFIG_GVSOC_STATS_ACTIVE
    // Engine statistics, dumped to stats.txt when run with --stats. One
    // counter per countable event, registered in the constructor and
    // incremented from the matching event_*_account() in event_implem.hpp.
    bool stats_enabled = false;   // cached in ctor: true only when --stats is active
    vp::StatScalar stat_instr;
    vp::StatScalar stat_fetch;
    vp::StatScalar stat_imiss;
    vp::StatScalar stat_ld;
    vp::StatScalar stat_st;
    vp::StatScalar stat_jump;
    vp::StatScalar stat_branch;
    vp::StatScalar stat_taken_branch;
    vp::StatScalar stat_rvc;
    vp::StatScalar stat_misaligned;
    vp::StatScalar stat_active_cycles;   // cycles the core was not idle
    vp::StatScalar stat_idle_cycles;     // cycles the core was idle
    StatIpc stat_ipc{&stat_instr, &stat_active_cycles};  // instr / active_cycles
    StatActivePct stat_active_pct{&stat_active_cycles, &stat_idle_cycles};  // active %
    // Active/idle window accounting. The core alternates between active
    // (busy_enter..busy_exit) and idle (busy_exit..busy_enter); each
    // transition closes the open window and opens the other one.
    bool active_open = false;
    bool idle_open = false;
    int64_t active_start = 0;
    int64_t idle_start = 0;
    // Per-label execution-duration statistics. The window opens right after an
    // instruction's fetch completes and closes at its commit; at close we add
    // the latency still pending in stall_cycles (branch/jump/div), so the
    // duration is exec + data-load/use wait + pipeline penalty, with the icache
    // miss excluded (it happens during the fetch, before the window opens, and
    // any synchronous-fetch stall is cancelled by the dur_open_stall baseline).
    InsnDurationStats insn_durations;
    const char *dur_label = nullptr;   // label of the open window (null = none)
    int64_t dur_open_cycle = 0;        // cycle the fetch completed
    int64_t dur_open_stall = 0;        // stall_cycles snapshot at open (sync-fetch baseline)
#endif

#ifdef CONFIG_GVSOC_STATS_ACTIVE
public:
    // Open a duration window for insn just after its fetch/decode, capturing the
    // stall_cycles baseline. Opens once (no-op if a window is already open), so a
    // stalled load's retries do not re-anchor it. Non-virtual: runs for every
    // iss_v2 core (e.g. Ri5kyEvents) without an override.
    inline void dur_window_open(iss_insn_t *insn, int64_t stall_now);
    // Close the open window at commit, charging exec + data wait + the latency
    // still pending in stall_cycles minus the at-open baseline.
    inline void dur_window_close(int64_t stall_now);
#endif
};
