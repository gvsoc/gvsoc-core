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

#include "vp/vp_data.hpp"


inline void vp::PowerSource::turn_on()
{
#ifdef VP_TRACE_ACTIVE
    this->is_on = true;
    this->check();
#endif
}


inline void vp::PowerSource::turn_off()
{
#ifdef VP_TRACE_ACTIVE
    this->is_on = false;
    this->check();
#endif
}


inline void vp::PowerSource::leakage_power_start()
{
#ifdef VP_TRACE_ACTIVE
    // Only start if leakage is defined
    if (this->leakage != -1)
    {
        this->is_leakage_power_started = true;
        this->check();
    }
#endif
}



inline void vp::PowerSource::leakage_power_stop()
{
#ifdef VP_TRACE_ACTIVE
    // Only stop if leakage is defined
    if (this->leakage != -1)
    {
        this->is_leakage_power_started = false;
        this->check();
    }
#endif
}



inline void vp::PowerSource::dynamic_power_start()
{
#ifdef VP_TRACE_ACTIVE
    // Only start accounting background power if it is is defined
    if (this->background_power != -1)
    {
        this->is_dynamic_power_started = true;
        this->check();
    }
#endif
}



inline void vp::PowerSource::dynamic_power_stop()
{
#ifdef VP_TRACE_ACTIVE
    // Only stop accounting background power if it is is defined
    if (this->background_power != -1)
    {
        this->is_dynamic_power_started = false;
        this->check();
    }
#endif
}



inline void vp::PowerSource::account_energy_quantum()
{
#ifdef VP_TRACE_ACTIVE
    // Only account energy is a quantum is defined
    if (this->is_on && this->quantum != -1)
    {
        this->trace->inc_dynamic_energy(this->quantum);
    }
#endif
}
