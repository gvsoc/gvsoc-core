/*
 * Copyright (C) 2021 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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
#include <mutex>
#include <condition_variable>
#include "gvsoc.hpp"

namespace gv {

    class TestbenchUart;

    /**
     * Testbench interface.
     * 
     * This class can be instantiated to interact with the testbench model, which a model used
     * for interacting with a chip for test purpose, for example to generate traffic on I2S
     * or UART interfaces.
     */
    class Testbench
    {
        friend class TestbenchUart;

    public:
        /**
         * Construct a new component Testbench object.
         * 
         * @param gvsoc A pointer to the top GVSOC instance controlling the simulation.
         * @param path  The path to the testbench component.
         */
        Testbench(Gvsoc *gvsoc, std::string path="**/testbench/testbench");

        /**
         * Get a uart.
         * 
         * This will return a class instance which can be used to interact with the specified uart.
         *
         * @param id The ID of the UART interface to interact with.
         * @return A pointer to the uart instance
         */
        TestbenchUart *get_uart(int id=0);

    protected:
        void *component;

    private:
        Gvsoc *gvsoc;
    };

    /**
     * TestbenchUart configuration
     * 
     * This is used to describe the way the testbench uart should be configured when it is
     * instantiated. This class is instantiated with default values which can be overriden to 
     * specify the desired configuration.
     */
    class TestbenchUartConf
    {
    public:
        /**
         * Uart baudrate
         */
        int baudrate = 115200;

        /**
         * Word size in bits
         */
        int word_size = 8;

        /**
         * Stop bits
         */
        int stop_bits = 1;

        /**
         * Parity mode
         */
        bool parity_mode = false;

        /**
         * HW control flow
         */
        bool ctrl_flow = true;

        /**
         * USART mode (true will use clock)
         */
        bool is_usart = false;

        /**
         * USART polarity
         */
        int usart_polarity = 0;

        /**
         * USART phase
         */
        int usart_phase = 0;
    };

    /**
     * Testbench uart interface.
     * 
     * This class is instantiated by the testbench when a uart is required, and can be used
     * to interact with the uart.
     */
    class TestbenchUart
    {
    public:
        /**
         * Construct a new component TestbenchUart object.
         * 
         * This class does not need to be instantiated, it is implicitely done when
         * the uart is retrieved from the testbench instance.
         * 
         * @param gvsoc A pointer to the top GVSOC instance controlling the simulation.
         * @param gvsoc A pointer to the testbench instance.
         * @param id  The path to the testbench component.
         */
        TestbenchUart(Gvsoc *gvsoc, Testbench *testbench, int id);

        /**
         * Open the uart.
         * 
         * This is mostly used to specify the uart configuration.
         * 
         * @param conf The class describing the uart configuration (uart baudrate and so on)
         */
        void open(TestbenchUartConf *conf);

        /**
         * Close the uart.
         * 
         * Once closed, the instance also needs to be deleted.
         */
        void close();

        /**
         * Send data to the uart.
         * 
         * @param buffer A pointer to  the data to be sent.
         * @param size The size of the data to be sent.
         */
        void tx(uint8_t *buffer, int size);

        /**
         * Attach a callback for received data.
         * 
         * The callback will be sent everytime data is received from the uart
         * so that the caller can handle the data.
         * 
         * @param callback The callback.
         */
        void rx_attach_callback(std::function<void(uint8_t *, int)> callback);

        /**
         * Enable data reception.
         * 
         * Must be called so that data start being received.
         */
        void rx_enable();

    private:
        void handle_rx(uint8_t *, int);
        Gvsoc *gvsoc;
        Testbench *testbench;
        int id;
        std::mutex mutex;
        std::condition_variable cond;
        std::function<void(uint8_t *, int)> rx_callback = NULL;
    };

};