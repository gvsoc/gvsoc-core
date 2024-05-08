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

#include "cpu/iss/include/iss.hpp"


void SnitchRegfile::reset(bool active)
{
    Regfile::reset(active);

    if (active)
    {
        // Initialize the scoreboard so that registers can be read by default.
        for (int i = 0; i < ISS_NB_REGS; i++)
        {
            this->scoreboard_reg_valid[i] = true;
        }
        for (int i = 0; i < ISS_NB_FREGS; i++)
        {
            this->scoreboard_freg_valid[i] = true;
        }
    }
}
