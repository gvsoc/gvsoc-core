/*
 * Copyright (C) 2021 GreenWaves Technologies, SAS, ETH Zurich and
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

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/clock.hpp>
#include <vp/itf/i2c.hpp>

#include <cstdint>

#include "i2c_helper.hpp"

class I2c_corruptor : public vp::Component
{
    public:
        I2c_corruptor(js::Config* config);

        void reset(bool active);

    private:
        static void st_start_event_handler(vp::Block* __this, vp::ClockEvent* event);
        void i2c_enqueue_event(vp::ClockEvent* event, uint64_t time_ps);
        void i2c_cancel_event(vp::ClockEvent* event);
        static void i2c_sync(vp::Block *__this, int scl, int sda);
        void i2c_helper_callback(i2c_operation_e id, i2c_status_e status, int value);
        void trigger_start(void);

        /**********/
        /* FIELDS */
        /**********/

        /** trace utility */
        vp::Trace trace;

        /** I2C interface */
        vp::I2cMaster i2c_itf;

        /* clock related */
        vp::ClockMaster clock_cfg;

        /** I2C slave helper */
        I2C_helper i2c_helper;

        /* start detection*/
        int prev_sda;
        int prev_scl;

        /** number of start counter */
        uint32_t start_counter;
        /** start threshold */
        uint32_t start_threshold;

        /** event used to trigger start */
        vp::ClockEvent start_event;

        bool is_started;
};
