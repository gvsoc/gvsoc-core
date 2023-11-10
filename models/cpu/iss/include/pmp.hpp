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
#include <cpu/iss/include/types.hpp>

#ifndef CONFIG_GVSOC_ISS_PMP_NB_ENTRIES
#define CONFIG_GVSOC_ISS_PMP_NB_ENTRIES 16
#endif

class PmpEntry
{
public:
    std::string get_mode_name();

    union
    {
        uint8_t value;
        struct
        {
            unsigned int R:1;
            unsigned int W:1;
            unsigned int X:1;
            unsigned int A:2;
            unsigned int reserved:2;
            unsigned int L:1;
        };
    };
};

class Pmp
{
public:
    Pmp(Iss &iss);

    void build();
    void reset(bool active);

    bool pmpcfg_read(iss_reg_t *value, int id);
    bool pmpcfg_write(iss_reg_t value, int id);

    bool pmpaddr_read(iss_reg_t *value, int id);
    bool pmpaddr_write(iss_reg_t value, int id);

private:
    void entry_cfg_set(int id, uint8_t config);

    PmpEntry entry[16];

    Iss &iss;
    vp::trace trace;
};

