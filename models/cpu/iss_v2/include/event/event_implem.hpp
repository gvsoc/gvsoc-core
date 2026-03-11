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
}

inline void Events::event_cycle_disable()
{
    uint8_t zero = 0;
    this->event_cycles.dump(&zero);
}

inline void Events::event_instr_account()
{
    uint8_t one = 1, zero = 0;
    this->event_instr.dump(&one);
    this->event_instr.dump_next(&zero);
}

inline void Events::event_load_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_ld.dump(&one);
    this->event_ld.dump_next(&zero, incr);
}

inline void Events::event_rvc_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_rvc.dump(&one);
    this->event_rvc.dump_next(&zero, incr);
}

inline void Events::event_store_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_st.dump(&one);
    this->event_st.dump_next(&zero, incr);
}

inline void Events::event_branch_account()
{
    uint8_t one = 1, zero = 0;
    this->event_branch.dump(&one);
    this->event_branch.dump_next(&zero);
}

inline void Events::event_taken_branch_account()
{
    uint8_t one = 1, zero = 0;
    this->event_taken_branch.dump(&one);
    this->event_taken_branch.dump_next(&zero);
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
#if defined(CONFIG_GVSOC_ISS_JUMP_STALL_CYCLES)
    this->iss.exec.stall_cycles_inc(1);
#endif
}

inline void Events::event_misaligned_account(int incr)
{
    uint8_t one = 1, zero = 0;
    this->event_misaligned.dump(&one);
    this->event_misaligned.dump_next(&zero, incr);
}
