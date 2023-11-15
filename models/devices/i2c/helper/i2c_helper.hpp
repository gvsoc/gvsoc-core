/*
 * Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

#include <functional>
#include <queue>

#include "vp/itf/i2c.hpp"
#include "vp/component.hpp"
#include "vp/clock/clock_engine.hpp"
#include "vp/clock/clock_event.hpp"

typedef enum {
    I2C_STATUS_OK,
    I2C_STATUS_KO,
    I2C_STATUS_ERROR_ARBITRATION,
    I2C_STATUS_ERROR_FRAMING,
} i2c_status_e;

typedef enum {
    I2C_OP_START,
    I2C_OP_DATA,
    I2C_OP_ACK,
    I2C_OP_STOP
} i2c_operation_e;

typedef enum {
    I2C_INTERNAL_IDLE,
    I2C_INTERNAL_WAIT_START,
    I2C_INTERNAL_WAIT_STOP,
    I2C_INTERNAL_START,
    I2C_INTERNAL_WAIT_DATA,
    I2C_INTERNAL_DATA,
    I2C_INTERNAL_DATA_READ,
    I2C_INTERNAL_ACK,
    I2C_INTERNAL_STOP_CLOCK,
    I2C_INTERNAL_STOP_CLOCK_WAIT,
    I2C_INTERNAL_RESTART,
    I2C_INTERNAL_STOP_0,
    I2C_INTERNAL_STOP_1,
} i2c_internal_state_e;

typedef std::function<void(i2c_operation_e id, i2c_status_e status, int value)> i2c_callback_t;

typedef std::function<void(vp::ClockEvent* event,uint64_t time_ps)> i2c_enqueue_event_fn_t;
typedef std::function<void(vp::ClockEvent* event)> i2c_cancel_event_fn_t;

/**
 * \brief TODO
 *
 * connect to a clock
 * connect to the helpersical interface
 *
 * Needs to:
 * - duty_cycle, baudrate
 * - be able to send/receive I2C parts ((RE)START, STOP, ADDR+RW, DATA, etc)
 * - clock stretch
 * - clock synchronization
 * - report errors (nack, arbitration, framing, etc)
 */
class I2C_helper {
    public:
        I2C_helper(vp::Component* parent, vp::I2cMaster* itf, i2c_enqueue_event_fn_t event, i2c_cancel_event_fn_t cancel_event, std::string trace_path="");

        // TO be called when pin values change
        void update_pins(int scl, int sda);

        //TODO
        void set_timings(uint64_t delay_low_ps, uint64_t delay_high_ps);

        //TODO
        void send_start(void);

        //TODO
        void send_address(int addr, bool is_write, bool is_10bits);

        //TODO
        void send_data(int byte);

        //TODO
        void read_data(void);

        //TODO
        void send_ack(bool ack);

        //TODO
        void send_stop(void);

        //TODO
        void release_pins(void);

        //TODO
        bool is_busy(void);

        /**
         * \brief TODO register callback
         */
        void register_callback(i2c_callback_t callback);
    private:
        /******************/
        /* Static methods */
        /******************/
        static void fsm_event_handler(vp::Block *__this, vp::ClockEvent* event);
        static void st_clock_event_handler(vp::Block *__this, vp::ClockEvent* event);
        static void i2c_sync(vp::Block *__this, int scl, int sda);

        /***********/
        /* Methods */
        /***********/
        void clock_event_handler(vp::ClockEvent* event);

        void start_clock(void);
        void stop_clock(void);
        void clock_toggle(void);
        void enqueue_clock_toggle(void);
        void enqueue_data_change(int new_sda);
        void fsm_enqueue_event(int64_t delay);
        void send_data_bit();

        void fsm_step();

        void sync_pins(void);

        std::string get_state_name(i2c_internal_state_e state);

        /*************/
        /* Externals */
        /*************/
        vp::Component* parent;
        vp::I2cMaster* itf;

        /*************/
        /* Callbacks */
        /*************/
        i2c_callback_t cb_master_operation;

        i2c_enqueue_event_fn_t enqueue_event;
        i2c_cancel_event_fn_t cancel_event;

        /*****************/
        /* Configuration */
        /*****************/
        uint64_t delay_low_ps;
        uint64_t delay_high_ps;

        /****************/
        /* Runtime data */
        /****************/
        vp::ClockEvent clock_event;
        vp::ClockEvent fsm_event;

        i2c_internal_state_e internal_state;

        bool bus_is_busy;

        bool is_driving_scl;
        int scl;
        int desired_scl;
        bool is_driving_sda;
        int sda;
        int desired_sda;

        int sda_rise; /* sda sampled on scl rising edge */

        int expected_bit_value; /* checked when scl is rising */

        bool check_sent;
        int sent_value;

        bool is_stopping;
        bool is_starting;

        bool is_clock_enabled;
        int clock_value;

        vp::Trace trace;

        int ack_value;

        uint8_t pending_data;
        int pending_data_bits;
        bool fsm_waiting;

        int input_scl;
        int input_sda;
};
