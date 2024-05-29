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
 * Authors: Kexin Li, ETH Zurich (likexi@ethz.ch)
 */


#pragma once

#include <cpu/iss/include/regfile.hpp>


class SnitchRegfile : public Regfile
{
public:
    SnitchRegfile(IssWrapper &top, Iss &iss) : Regfile(top, iss) {}

    void reset(bool active);

    inline void set_freg(int reg, iss_freg_t value);
    inline iss_freg_t get_freg(int reg);
    inline iss_freg_t get_freg_untimed(int reg);

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    bool scoreboard_reg_valid[ISS_NB_REGS+1];
#if !defined(ISS_SINGLE_REGFILE)
    bool scoreboard_freg_valid[ISS_NB_REGS+1];
#endif
#endif
};
