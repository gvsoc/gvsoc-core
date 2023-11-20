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

#ifndef __TESTBENCH_I2S_VERIF_HPP__
#define __TESTBENCH_I2S_VERIF_HPP__

#include "testbench.hpp"

class Testbench;
class Slot;

class I2s_verif : public vp::Block
{
public:
    I2s_verif(Testbench *top, vp::I2sMaster *itf, int itf_id, pi_testbench_i2s_verif_config_t *config);
    ~I2s_verif();

    void start(pi_testbench_i2s_verif_start_config_t *config);
    void slot_setup(pi_testbench_i2s_verif_slot_config_t *config);
    void slot_start(pi_testbench_i2s_verif_slot_start_config_t *config, std::vector<int> slots);
    void slot_stop(pi_testbench_i2s_verif_slot_stop_config_t *config);
    void sync(int sck, int ws, int sd, bool full_duplex);
    void sync_sck(int sck);
    void sync_ws(int ws);
    int64_t exec();
    void set_pdm_data(int slot, int data);

    Testbench *top;
    vp::Trace trace;
    vp::I2sMaster *itf;
    pi_testbench_i2s_verif_config_t config;
    int ws_delay;
    
    int  prev_ws;
    int  prev_sck;
    bool frame_active;
    int  current_ws_delay;
    int  active_slot;
    bool sync_ongoing;
    bool zero_delay_start;
    int  pending_bits;

    bool is_pdm;
    bool is_full_duplex;

    bool clk_active;
    int64_t clk_period;
    bool is_ext_clk;
    int clk;
    int data;
    int sdio;
    int ws;
    int propagated_clk;
    int propagated_ws;
    int ws_count;
    int ws_value;
    bool pdm_lanes_is_out[3];
    int64_t prev_frame_start_time;
    int64_t sampling_period;

    std::vector<Slot *> slots;

    int64_t ws_gen_timestamp = -1;
    int64_t clk_gen_timestamp = -1;
};

#endif