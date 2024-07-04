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

#define PRIV_U 0
#define PRIV_S 1
#define PRIV_M 3

class Core
{
public:
    Core(Iss &iss);

    void build();
    void reset(bool active);

    iss_reg_t mret_handle();
    iss_reg_t dret_handle();
    iss_reg_t sret_handle();
    int mode_get() { return this->mode; }
    void mode_set(int mode);
    iss_reg_t load_reserve_addr_get() { return this->load_reserve_addr; }
    void load_reserve_addr_set(iss_reg_t addr) { this->load_reserve_addr = addr; }
    void load_reserve_addr_clear() { this->load_reserve_addr = -1; }

    int float_mode;

private:
    bool mstatus_update(bool is_write, iss_reg_t &value);
    bool sstatus_update(bool is_write, iss_reg_t &value);

    Iss &iss;
    vp::Trace trace;

    int mode;
    iss_reg_t mstatus_write_mask;
    iss_reg_t sstatus_write_mask;
    iss_reg_t load_reserve_addr;
    bool reset_stall = false;
};

