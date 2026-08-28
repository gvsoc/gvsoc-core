/*
 * Copyright (C) 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

/*
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include <cpu/iss_v2/include/iss.hpp>
#include <cpu/iss_v2/include/hwloop/hwloop.hpp>


Hwloop::Hwloop(Iss &iss) : iss(iss)
{
    iss.traces.new_trace("hwloop", &this->trace, vp::DEBUG);
}


void Hwloop::reset(bool active)
{
    if (active)
    {
        for (int i = 0; i < CONFIG_GVSOC_ISS_NB_HWLOOP; i++)
        {
            this->start_pc[i] = 0;
            this->end_pc[i] = 0;
            this->count[i] = 0;
        }
        this->active = 0;
    }
}
