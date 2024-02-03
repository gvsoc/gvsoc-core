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
#include <cpu/iss/include/htif.hpp>


class IssWrapper;


class Syscalls
{
public:
    Syscalls(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool active);

    void handle_ebreak();
    void handle_riscv_ebreak();

    bool user_access(iss_addr_t addr, uint8_t *data, iss_addr_t size, bool is_write);
    std::string read_user_string(iss_addr_t addr, int len = -1);

    vp::Trace trace;

    Iss_pcer_info_t pcer_info[32];
    int64_t cycle_count_start;
    int64_t cycle_count;
    Htif htif;

private:
    Iss &iss;
    int64_t latency;
};
