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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

inline iss_freg_t SnitchRegfile::get_freg(int reg)
{
#if defined(CONFIG_GVSOC_ISS_SSR)
    // Conditionally choose operands from regfile or external streams.
    if (!this->iss.ssr.ssr_is_enabled() || reg > 2)
    {
        return this->get_freg_untimed(reg);
    }
    else
    {
        iss_freg_t value = this->iss.ssr.pop_data(reg);
        return value;
    }
#else
    return this->get_freg_untimed(reg);
#endif
}

inline void SnitchRegfile::set_freg(int reg, iss_freg_t value)
{
#if defined(CONFIG_GVSOC_ISS_SSR)
    if (!this->iss.ssr.ssr_is_enabled() || reg > 2)
    {
        Regfile::set_freg(reg, value);
    }
    else
    {
        this->iss.ssr.push_data(reg, value);
    }
#else
    Regfile::set_freg(reg, value);
#endif
}
