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

#include "cpu/iss/include/types.hpp"

class IssWrapper;

class Regfile
{
public:

    Regfile(IssWrapper &top, Iss &iss);

    void reset(bool active);

    iss_reg_t regs[ISS_NB_REGS+1];

#if !defined(ISS_SINGLE_REGFILE)
    iss_freg_t fregs[ISS_NB_FREGS];
#endif

    iss_reg_t regs_memcheck[ISS_NB_REGS+1];

    inline iss_reg_t *reg_ref(int reg);
    inline iss_reg_t *reg_check_ref(int reg);
    inline iss_reg_t *reg_store_ref(int reg);
    inline iss_reg_t *reg_check_store_ref(int reg);
    inline void set_reg(int reg, iss_reg_t value);
    inline iss_reg_t get_reg(int reg);
    inline iss_reg_t get_reg_from_insn(iss_insn_t *insn, int insn_reg_index);
    inline iss_reg_t get_reg_untimed(int reg);
    inline iss_reg64_t get_reg64(int reg);
    inline iss_reg64_t get_reg64_untimed(int reg);
    inline iss_reg64_t get_memcheck_reg64(int reg);
    inline void set_reg64(int reg, iss_reg64_t value);

    inline iss_freg_t *freg_ref(int reg);
    inline iss_freg_t *freg_store_ref(int reg);
    inline void set_freg(int reg, iss_freg_t value);
    inline iss_freg_t get_freg(int reg);
    inline iss_freg_t get_freg_untimed(int reg);

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    inline void scoreboard_reg_set_timestamp(int reg, int64_t latency, int stall_reason);
    inline void scoreboard_freg_set_timestamp(int reg, int64_t latency, int stall_reason);
    inline void scoreboard_reg_invalidate(int reg);
    inline void scoreboard_freg_invalidate(int reg);
    inline void scoreboard_reg_check(int reg);
    inline void scoreboard_freg_check(int reg);
#endif

    inline bool memcheck_reg(int reg);
    inline void memcheck_branch_reg(int reg);
    inline void memcheck_propagate_1(int out_reg, int in_reg);
    inline void memcheck_access_reg(int reg);
    inline void memcheck_fault();
    inline iss_reg_t memcheck_get(int reg);
    inline void memcheck_set(int reg, iss_reg_t value);
    inline bool memcheck_get_valid(int reg);
    inline void memcheck_set_valid(int reg, bool valid);
    inline void memcheck_merge(int out_reg, int in_reg);
    inline void memcheck_merge64(int out_reg, int in_reg);
    inline void memcheck_copy(int out_reg, int in_reg);
    inline void memcheck_bitwise_and(int out_reg, int in_reg_0, int in_reg_1);
    inline void memcheck_shift_left(int out_reg, int in_reg, int shift);
    inline void memcheck_shift_right(int out_reg, int in_reg, int shift);
    inline void memcheck_shift_right_signed(int out_reg, int in_reg, int shift);

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    int64_t scoreboard_reg_timestamp[ISS_NB_REGS+1];
    int scoreboard_reg_stall_reason[ISS_NB_REGS+1];
#if !defined(ISS_SINGLE_REGFILE)
    int64_t scoreboard_freg_timestamp[ISS_NB_FREGS];
    int scoreboard_freg_stall_reason[ISS_NB_FREGS];
#endif
#endif

protected:
    Iss &iss;

    vp::Trace trace;

    vp::ClockEngine *engine;

    bool memcheck_reg_fault;
    int memcheck_reg_fault_id;
    std::string memcheck_reg_fault_message;
};