/*
 * Copyright (C) 2026 ETH Zurich and University of Bologna
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

#pragma once

#include <vp/vp.hpp>

inline void Events::event_cycle_enable()
{
    uint8_t one = 1;
    this->event_cycles.dump(&one);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled)
    {
        // Core becomes active: close the idle window, open the active one.
        int64_t now = this->iss.clock.get_cycles();
        if (this->idle_open)
        {
            this->stat_idle_cycles += now - this->idle_start;
            this->idle_open = false;
        }
        this->active_open = true;
        this->active_start = now;
    }
#endif
}

inline void Events::event_cycle_disable()
{
    uint8_t zero = 0;
    this->event_cycles.dump(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled)
    {
        // Core becomes idle: close the active window, open the idle one.
        int64_t now = this->iss.clock.get_cycles();
        if (this->active_open)
        {
            this->stat_active_cycles += now - this->active_start;
            this->active_open = false;
        }
        this->idle_open = true;
        this->idle_start = now;
        // No duration-window handling here: each instruction's window closes at
        // its own commit, so nothing is left open when the core goes idle.
    }
#endif
}

inline void Events::event_instr_account()
{
    uint8_t one = 1, zero = 0;
    this->event_instr.dump(&one);
    this->event_instr.dump_next(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_instr++;
#endif
}

inline void Events::event_fetch_account()
{
    uint8_t one = 1, zero = 0;
    this->event_fetch.dump(&one);
    this->event_fetch.dump_next(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_fetch++;
#endif
}

inline void Events::event_imiss_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_imiss.dump(&one);
    this->event_imiss.dump_next(&zero, incr);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_imiss += incr;
#endif
}

inline void Events::event_imiss_start()
{
    uint8_t one = 1;
    this->event_imiss.dump(&one);
}

inline void Events::event_imiss_stop()
{
    uint8_t zero = 0;
    this->event_imiss.dump_next(&zero);
}

inline void Events::event_load_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_ld.dump(&one);
    this->event_ld.dump_next(&zero, incr);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_ld += incr;
#endif
}

inline void Events::event_rvc_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_rvc.dump(&one);
    this->event_rvc.dump_next(&zero, incr);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_rvc += incr;
#endif
}

inline void Events::event_store_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_st.dump(&one);
    this->event_st.dump_next(&zero, incr);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_st += incr;
#endif
}

inline void Events::event_branch_account()
{
    uint8_t one = 1, zero = 0;
    this->event_branch.dump(&one);
    this->event_branch.dump_next(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_branch++;
#endif
}

inline void Events::event_taken_branch_account()
{
    uint8_t one = 1, zero = 0;
    this->event_taken_branch.dump(&one);
    this->event_taken_branch.dump_next(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_taken_branch++;
#endif
#if defined(CONFIG_GVSOC_ISS_TAKEN_BRANCH_STALL_CYCLES)
    this->iss.exec.stall_cycles_inc(2);
#endif
    this->event_branch_account();
}

inline void Events::event_jump_account()
{
    uint8_t one = 1, zero = 0;
    this->event_jump.dump(&one);
    this->event_jump.dump_next(&zero);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_jump++;
#endif
#if defined(CONFIG_GVSOC_ISS_JUMP_STALL_CYCLES)
    this->iss.exec.stall_cycles_inc(1);
#endif
}

inline void Events::event_misaligned_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_misaligned.dump(&one);
    this->event_misaligned.dump_next(&zero, incr);
#ifdef CONFIG_GVSOC_STATS_ACTIVE
    if (this->stats_enabled) this->stat_misaligned += incr;
#endif
}

#ifdef CONFIG_GVSOC_STATS_ACTIVE
inline void Events::dur_window_open(iss_insn_t *insn, int64_t stall_now)
{
    if (!this->stats_enabled) return;
    // Open once: a stalled load / load-use hazard returns and re-enters fetch,
    // but must keep the original anchor so its wait stays inside the window.
    if (this->dur_label != nullptr) return;
    this->dur_label = insn->desc->label;
    this->dur_open_cycle = this->iss.clock.get_cycles();
    // Baseline of any stall already queued by this instruction's fetch (a
    // synchronous icache-miss latency on ri5ky); subtracted at close so the
    // icache miss is not counted.
    this->dur_open_stall = stall_now;
}

inline void Events::dur_window_close(int64_t stall_now)
{
    if (!this->stats_enabled || this->dur_label == nullptr) return;
    int64_t now = this->iss.clock.get_cycles();
    // The instruction executes atomically, so fetch-done and commit land on the
    // same cycle; count its own issue slot as 1, then add the cycles waited
    // before commit (data-load/use) and the branch/jump/div latency still
    // pending in stall_cycles (minus the at-open fetch baseline). The icache
    // miss is excluded: it elapsed before the window opened, and a synchronous
    // fetch stall is cancelled by dur_open_stall.
    int64_t dur = 1 + (now - this->dur_open_cycle) + (stall_now - this->dur_open_stall);
    if (dur < 0) dur = 0;
    this->insn_durations.account(this->dur_label, dur);
    this->dur_label = nullptr;
}
#endif
