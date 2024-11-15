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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/uart.hpp>
#include <vp/itf/clock.hpp>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>

class Uart_checker : public vp::Component
{
public:
    Uart_checker(vp::ComponentConf &conf);

    void tx_edge(int64_t timestamp, int tx);
    void rx_edge(int64_t timestamp, int rx);

    void tx_sampling();
    void rx_sampling();

private:
    void dpi_task(void);
    bool rx_is_sampling(void);
    void stdin_task(void);

    static void dpi_uart_task_stub(Uart_checker *);
    static void dpi_uart_stdin_task_stub(Uart_checker *);

    void start_tx_sampling(int baudrate);
    void stop_tx_sampling();

    void start_rx_sampling(int baudrate);
    void stop_rx_sampling();

    static void sync(vp::Block *__this, int data);

    static void event_handler(vp::Block *__this, vp::ClockEvent *event);

    enum UartTxState {
        WaitStart,
        WaitStop,
        DataReceive
    };

    UartTxState tx_state = WaitStart;

    uint64_t period;
    int current_tx;
    int current_rx;
    uint64_t baudrate;
    int received_bits = 0;
    uint32_t rx_bit_buffer = 0;
    int sent_bits = 0;
    bool loopback;
    bool print_stdout;
    bool stdin;
    bool sampling_rx = false;
    bool sampling_tx = false;
    uint8_t byte;
    FILE *tx_file = NULL;

    vp::UartSlave in;

    std::thread *stdin_thread;

    std::mutex rx_mutex;
    vp::Trace trace;

    vp::ClockEvent *event;
    vp::ClockMaster clock_cfg;
};

Uart_checker::Uart_checker(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->new_master_port("clock_cfg", &clock_cfg);

    baudrate = get_js_config()->get("baudrate")->get_int();
    loopback = get_js_config()->get("loopback")->get_bool();
    print_stdout = get_js_config()->get("stdout")->get_bool();
    stdin = get_js_config()->get("stdin")->get_bool();

    this->in.set_sync_meth(&Uart_checker::sync);
    new_slave_port("input", &in);

    this->event = event_new(Uart_checker::event_handler);

    period = 1000000000000UL / baudrate;

    std::string tx_filename = get_js_config()->get("tx_file")->get_str();
    if (tx_filename != "") {
        tx_file = fopen(tx_filename.c_str(), (char *)"w");
    }

    if (this->stdin) {
        stdin_thread = new std::thread(&Uart_checker::stdin_task, this);
    }
}

void Uart_checker::tx_sampling()
{
    switch (this->tx_state) {
        case WaitStop:
        case WaitStart:
            // handled in ::sync()
            break;
        case DataReceive:
            this->trace.msg(vp::Trace::LEVEL_INFO, "Received data bit (data: %d)\n", current_tx);
            byte = (byte >> 1) | (current_tx << 7);
            received_bits++;
            if (received_bits == 8) {
                this->trace.msg(vp::Trace::LEVEL_INFO, "Sampled TX byte (value: 0x%x)\n", byte);

                if (print_stdout) std::putc(byte, stdout);
                else if (tx_file) std::putc(byte, tx_file);

                this->trace.msg(vp::Trace::LEVEL_INFO, "Waiting for stop bit\n");
                tx_state = WaitStop;
            }
            break;
    }
}

void Uart_checker::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Uart_checker *_this = (Uart_checker *)__this;

    _this->tx_sampling();

    if (_this->tx_state == DataReceive) {
        _this->event_enqueue(event, 4);
    }
}

void Uart_checker::sync(vp::Block *__this, int data)
{
    Uart_checker *_this = (Uart_checker *)__this;

    if (_this->loopback) {
        _this->in.sync_full(data, 2, 2, 0x0);
    }

    _this->current_tx = data;

    switch (_this->tx_state) {
        case WaitStart:
            if (data == 0) {
                _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received start bit\n");
                _this->received_bits = 0;
                _this->start_tx_sampling(_this->baudrate);
                _this->tx_state = DataReceive;
            }
            break;
        case WaitStop:
            if (data == 1) {
                _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received stop bit\n");
                _this->received_bits = 0;
                _this->stop_tx_sampling();
                _this->tx_state = WaitStart;
            }
            break;
        case DataReceive:
            // handled in ::tx_sampling()
            break;
    }
}

void Uart_checker::rx_sampling()
{
    rx_mutex.lock();
    this->current_rx = this->rx_bit_buffer & 0x1;
    this->rx_bit_buffer = this->rx_bit_buffer >> 1;
    //std::cerr << "Sampling bit " << current_rx << std::endl;

    //uart->rx_edge(this->current_rx);
    sent_bits++;
    if (sent_bits == 10)
    {
        this->stop_rx_sampling();
    }
    rx_mutex.unlock();
}

void Uart_checker::start_tx_sampling(int baudrate)
{
    // We set the frequency to four times the baudrate to be able sampling in the
    // middle of the cycle
    this->clock_cfg.set_frequency(baudrate*4);
    this->sampling_tx = true;
    this->event_reenqueue(this->event, 6);
}

void Uart_checker::stop_tx_sampling()
{
    this->sampling_tx = false;
    if (this->event->is_enqueued()) {
        this->event_cancel(this->event);
    }
}

// will be called protected by a mutex
void Uart_checker::start_rx_sampling(int baudrate)
{
    this->sampling_rx = 1;
}

void Uart_checker::stop_rx_sampling(void)
{
    this->sampling_rx = 0;
    sent_bits = 0;
}

void Uart_checker::dpi_task(void)
{
    while (1)
    {
        while (!(this->rx_is_sampling() || this->sampling_tx))
        {
            //this->wait_event();
        }

        //wait_ps(period/2);

        while (this->rx_is_sampling() || this->sampling_tx)
        {
            //this->wait_ps(period);
            if (this->sampling_tx)
            {
                this->tx_sampling();
            }
            if (this->rx_is_sampling())
            {
                this->rx_sampling();
            }
        }
    }
}

bool Uart_checker::rx_is_sampling(void)
{
    rx_mutex.lock();
    bool ret = this->sampling_rx;
    rx_mutex.unlock();
    return ret;
}

void Uart_checker::stdin_task(void)
{
    while (1)
    {
        uint8_t c = 0;
        if (this->stdin)
        {
            c = getchar();
        }

        while (this->rx_is_sampling())
        { // TODO: use cond instead
            usleep(5);
        }
        // TODO: use once atomic function for this instead
        this->rx_mutex.lock();
        rx_bit_buffer = 0;
        rx_bit_buffer |= ((uint32_t)c) << 1;
        rx_bit_buffer |= 1 << 9;
        sent_bits = 0;
        this->start_rx_sampling(baudrate);
        this->rx_mutex.unlock();
        //raise_event_from_ext();
        printf("raised_event\n");
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Uart_checker(config);
}
