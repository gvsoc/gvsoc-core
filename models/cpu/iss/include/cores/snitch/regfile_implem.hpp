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


inline void SnitchRegfile::set_freg(int reg, iss_freg_t value)
{
    // Conditionally choose operands from regfile or external streams.
    // Choose operands from streamer when SSR is enabled and fp register index is 0/1/2.
    if (!this->iss.ssr.ssr_enable)
    {
        this->fregs[reg] = value;
    }
    else
    {
        if (reg >= 0 & reg <= 2)
        {
            this->iss.ssr.dm_write(value, reg);
        }
        else
        {
            this->fregs[reg] = value;
        }
    }
}

inline iss_freg_t SnitchRegfile::get_freg_untimed(int reg)
{
    if (!this->iss.ssr.ssr_enable)
    {
        return this->fregs[reg];
    }
    else
    {
        if (reg >= 0 & reg <= 2)
        {
            if (reg == 0)
            {
                return this->iss.ssr.ssr_fregs[0];
            }
            if (reg == 1)
            {
                return this->iss.ssr.ssr_fregs[1];
            }
            if (reg == 2)
            {
                return this->iss.ssr.ssr_fregs[2];
            }
        }

        return this->fregs[reg];
    }
}

inline iss_freg_t SnitchRegfile::get_freg(int reg)
{
    // Conditionally choose operands from regfile or external streams.
    if (!this->iss.ssr.ssr_enable)
    {
    #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
        this->scoreboard_freg_check(reg);
    #endif

        return this->get_freg_untimed(reg);
    }
    else
    {
    // There's no need to check scoreboard if SSR is enabled.
        if (reg >= 0 & reg <= 2)
        {
            if (reg == 0 & this->iss.ssr.dm0.dm_read)
            {
                return this->iss.ssr.ssr_fregs[0];
            }
            if (reg == 1 & this->iss.ssr.dm1.dm_read)
            {
                return this->iss.ssr.ssr_fregs[1];
            }
            if (reg == 2 & this->iss.ssr.dm2.dm_read)
            {
                return this->iss.ssr.ssr_fregs[2];
            }
            // Only need to fetch from memory once at the first time
            // and the memory access from/to the same address would be finished by fetching from ssr_fregs[i].
            return this->iss.ssr.dm_read(reg);
        }
        else
        {
            // There's no need to check scoreboard if SSR is enabled.
        #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
            this->scoreboard_freg_check(reg);
        #endif

            return this->get_freg_untimed(reg);
        }
    }
}
