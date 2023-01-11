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

#include "iss.hpp"

static inline iss_reg_t get_field(iss_reg_t field, int bit, int width)
{
    return (field >> bit) & ((1 << width) - 1);
}

Pmp::Pmp(Iss &iss)
: iss(iss)
{
}

void Pmp::build()
{
    this->iss.top.traces.new_trace("pmp", &this->trace, vp::DEBUG);
}

void Pmp::reset(bool active)
{
    for (int i=0; i<16; i++)
    {
        this->entry[i].value = 0;
    }
}

bool Pmp::pmpcfg_read(iss_reg_t *value, int id)
{
    iss_reg_t val = 0;
    for (int i=0; i<ISS_REG_WIDTH/8; i++)
    {
        val |= this->entry[i].value << (i*8);
    }
    return false;
}

bool Pmp::pmpcfg_write(iss_reg_t value, int id)
{
    for (int i=0; i<ISS_REG_WIDTH/8; i++)
    {
        this->entry_cfg_set(i, (value >> (i*8)) & 0xFF);
    }
    return false;
}

bool Pmp::pmpaddr_read(iss_reg_t *value, int id)
{
    return false;
}

bool Pmp::pmpaddr_write(iss_reg_t value, int id)
{
    return false;
}

void Pmp::entry_cfg_set(int id, uint8_t config)
{
    if (id < CONFIG_GVSOC_ISS_PMP_NB_ENTRIES)
    {
        this->entry[id].value = config & 0x9F;

        PmpEntry *entry = &this->entry[id];
        this->trace.msg(vp::trace::LEVEL_DEBUG, "Setting entry (id: %d, R: %d, W: %d, X: %d, mode: %s)\n",
            id, entry->R, entry->W, entry->X, entry->get_mode_name().c_str());
    }
}


std::string PmpEntry::get_mode_name()
{
    switch (this->A)
    {
        case 0: return "off";
        case 1: return "tor";
        case 2: return "na4";
        case 3: return "napot";
    }

    return "-";
}