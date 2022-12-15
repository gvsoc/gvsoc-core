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

typedef iss_insn_t *(*iss_insn_callback_t)(Iss *iss, iss_insn_t *insn);

class Exec
{
public:
    Exec(Iss &iss);

    inline void stalled_inc();
    inline void stalled_dec();

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

    vp::reg_8 stalled;

    vp::trace trace;
    vp::clock_event *instr_event;

    static void exec_instr(void *__this, vp::clock_event *event);
    static void exec_instr_check_all(void *__this, vp::clock_event *event);
    static void exec_first_instr(void *__this, vp::clock_event *event);

    void exec_first_instr(vp::clock_event *event);

private:

    Iss &iss;

};
