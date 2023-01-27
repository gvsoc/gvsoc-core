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

#include <types.hpp>

class Gdbserver : public vp::Gdbserver_core
{
public:
    Gdbserver(Iss &iss);
    void build();
    void start();
    void reset(bool active);
    int gdbserver_get_id();
    std::string gdbserver_get_name();
    int gdbserver_reg_set(int reg, uint8_t *value);
    int gdbserver_reg_get(int reg, uint8_t *value);
    int gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value);
    int gdbserver_stop();
    int gdbserver_cont();
    int gdbserver_stepi();
    int gdbserver_state();


    Iss &iss;
    vp::trace trace;
    vp::Gdbserver_engine *gdbserver;
};