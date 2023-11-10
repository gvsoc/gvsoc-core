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



class DbgUnit
{
public:
    DbgUnit(Iss &iss);

    void build();

    static void halt_sync(void *_this, bool active);
    void debug_req();
    void set_halt_mode(bool halted, int cause);

    vp::io_slave dbg_unit;
    vp::wire_slave<bool> halt_itf;
    vp::wire_master<bool> halt_status_itf;
    bool riscv_dbg_unit;
    iss_reg_t hit_reg = 0;
    int halt_cause;
    vp::reg_1 do_step;


    vp::trace trace;

private:
    Iss &iss;
};
