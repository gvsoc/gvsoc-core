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


#include "testbench.hpp"
#include "spim_verif.hpp"
#include "i2s_verif.hpp"
#include <stdio.h>
#include "vp/proxy.hpp"

static unsigned int testbench_seed = 1;

Uart_flow_control_checker::Uart_flow_control_checker(Testbench *top, Uart *uart, pi_testbench_req_t *req)
: top(top), uart(uart)
{

    top->traces.new_trace("uart_" + std::to_string(uart->id) + "/flow_control", &this->trace, vp::DEBUG);

    this->uart->set_cts(0);

    uart->baudrate = req->uart.baudrate;
    this->current_string = "";
    this->waiting_command = true;
    this->rx_size = 0;
    this->tx_size = 0;
    this->rx_timestamp = -1;

    this->bw_limiter_event = top->event_new((vp::Block *)this, Uart_flow_control_checker::bw_limiter_handler);
}


void Uart_flow_control_checker::bw_limiter_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Uart_flow_control_checker *_this = (Uart_flow_control_checker *)__this;
    _this->uart->set_cts(0);
}


void Uart_flow_control_checker::send_byte()
{
    if (this->send_reply)
    {
        this->uart->send_byte(this->reply[this->reply_index]);
        this->reply_index++;
        if (this->reply_index == this->reply.size() + 1)
        {
            this->send_reply = false;
        }
    }
    else
    {
        this->uart->send_byte(this->rx_start);
        this->rx_start += this->rx_incr;
        this->rx_size--;
    }
}


void Uart_flow_control_checker::send_byte_done()
{
    if (this->rx_size > 0 || this->send_reply)
        this->send_byte();
}


void Uart_flow_control_checker::check_end_of_command()
{
    if (this->tx_size == 0 && this->rx_size == 0)
    {
        this->waiting_command = true;
    }
}

void Uart_flow_control_checker::handle_received_byte(uint8_t byte)
{
    if (waiting_command)
    {
        if (byte != 0)
        {
            this->current_string += byte;
        }
        else
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "UART flow control received command (command: %s)\n", this->current_string.c_str());

            std::vector<std::string> words;
            std::istringstream iss(this->current_string);
            std::string token;

            while (std::getline(iss, token, ' ')) {
                if (!token.empty()) {
                    words.push_back(token);
                }
            }

            if (words[0] == "START")
            {
                this->trace.msg(vp::Trace::LEVEL_INFO, "UART flow control received start command\n");
                this->waiting_command = false;
                this->status = 0;
                if (this->rx_size > 0)
                    this->send_byte();
            }
            else if (words[0] == "TRAFFIC_RX")
            {
                this->received_bytes = 0;
                this->rx_start = std::stoi(words[1]);
                this->rx_incr = std::stoi(words[2]);
                this->rx_size = std::stoi(words[3]);
                this->rx_bandwidth = std::stoi(words[4]);
                this->rx_nb_iter = std::stoi(words[5]);
                this->trace.msg(vp::Trace::LEVEL_INFO, "UART flow control received traffic rx command (start: %d, incr: %d, size: %d, bandwidth: %d, nb_iter: %d)\n", this->rx_start, this->rx_incr, this->rx_size, this->rx_bandwidth, this->rx_nb_iter);
            }
            else if (words[0] == "TRAFFIC_TX")
            {
                this->tx_start = std::stoi(words[1]);
                this->tx_value = this->tx_start;
                this->tx_value_init = this->tx_start;
                this->tx_incr = std::stoi(words[2]);
                this->tx_size = std::stoi(words[3]);
                this->tx_iter_size = std::stoi(words[4]);
                this->tx_iter_size_init = this->tx_iter_size;
                this->trace.msg(vp::Trace::LEVEL_INFO, "UART flow control received traffic tx command (start: %d, incr: %d, size: %d, iter_size: %d)\n", this->tx_start, this->tx_incr, this->tx_size, this->tx_iter_size);
            }
            else if (words[0] == "STATUS")
            {
                this->trace.msg(vp::Trace::LEVEL_INFO, "UART flow control received status command\n");
                this->send_reply = true;
                this->reply_index = 0;
                if (this->status == 0)
                {
                    this->reply = "OK";
                }
                else
                {
                    this->reply = "KO";
                }
                this->send_byte();
            }

            this->current_string = "";
        }
    }
    else
    {
        if (this->tx_size > 0)
        {
            int64_t current_time = this->top->time.get_time();

            if (byte != (this->tx_value & 0xFF))
            {
                this->status = 1;
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received unexpected byte (value: 0x%x, expected: 0x%x)\n",
                    byte, this->tx_value & 0xFF);
            }
            this->tx_value += this->tx_incr;

            if (this->rx_timestamp == -1)
            {
                this->rx_timestamp = current_time - 1000000000000ULL*8/this->uart->baudrate;
            }

            this->received_bytes++;

            if (current_time > this->rx_timestamp)
            {
                if ((int64_t)this->received_bytes  / (current_time - this->rx_timestamp) * 1000000000000ULL > this->rx_bandwidth)
                {
                    this->uart->set_cts(1);

                    int64_t next_time = (int64_t)this->received_bytes / this->rx_bandwidth * 1000000000000ULL + this->rx_timestamp;
                    int64_t period = this->uart->clock->get_period();
                    int64_t cycles = (next_time - current_time + period - 1) / period;

                    // Randomize a bit
                    cycles = cycles * (rand_r(&testbench_seed) % 100) / 100 + 1;

                    if (!this->bw_limiter_event->is_enqueued())
                    {
                        this->uart->clock->enqueue(this->bw_limiter_event, cycles);
                    }
                }

            }

            this->tx_size--;
            this->tx_iter_size--;
            if (this->tx_iter_size == 0)
            {
                this->tx_value = this->tx_value_init;
                this->tx_iter_size = this->tx_iter_size_init;
            }
            this->check_end_of_command();
        }
    
    }
}

void Testbench::reset(bool active)
{
    for (Uart *uart: this->uarts)
    {
        uart->reset(active);
    }
}


Testbench::Testbench(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->state = STATE_WAITING_CMD;
    this->current_req_size = 0;
    this->is_pdm = false;
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->ctrl_type = get_js_config()->get("ctrl_type")->get_str();
    this->nb_gpio = get_js_config()->get("nb_gpio")->get_int();
    this->nb_spi = get_js_config()->get("nb_spi")->get_int();
    this->nb_i2s = get_js_config()->get("nb_i2s")->get_int();
    this->nb_i2c = get_js_config()->get("nb_i2c")->get_int();
    this->nb_uart = get_js_config()->get("nb_uart")->get_int();

    for (int i=0; i<this->nb_uart; i++)
    {
        this->uarts.push_back(new Uart(this, i));
    }

    this->gpios.resize(this->nb_gpio);
    
    for (int i=0; i<this->nb_gpio; i++)
    {
        this->gpios[i] = new Gpio(this);
        this->gpios[i]->itf.set_sync_meth_muxed(&Testbench::gpio_sync, i);
        this->new_slave_port("gpio" + std::to_string(i), &this->gpios[i]->itf);
    }
    
    for (int i=0; i<this->nb_spi; i++)
    {
        for (int j=0; j<4; j++)
        {
            Spi *spi = new Spi(this, i, j);
            this->spis.push_back(spi);
        }
    }

    for (int i=0; i<this->nb_i2s; i++)
    {
        I2s *i2s = new I2s(this, i);
        this->i2ss.push_back(i2s);
    }

    this->i2cs.resize(this->nb_i2c);
    
    for (int i=0; i<this->nb_i2c; i++)
    {
        this->i2cs[i].conf(this, i);
        this->i2cs[i].itf.set_sync_meth_muxed(&Testbench::i2c_sync, i);
        this->new_slave_port("i2c" + std::to_string(i), &this->i2cs[i].itf);
    }

    if (this->ctrl_type == "uart")
    {
        int uart_id = get_js_config()->get("uart_id")->get_int();
        int uart_baudrate = get_js_config()->get("uart_baudrate")->get_int();

        this->uarts[uart_id]->set_control(true, uart_baudrate);
        this->uarts[uart_id]->set_dev(new Uart_reply(this, this->uarts[uart_id]));
        this->uart_ctrl = this->uarts[uart_id];
    }

}



void Testbench::start()
{
    if (get_js_config()->get_child_bool("spislave_boot/enabled"))
    {
        int itf = get_js_config()->get_child_int("spislave_boot/itf");
        this->spis[itf*4]->create_loader(get_js_config()->get("spislave_boot"));
    }

    for (int i=0; i<this->nb_uart; i++)
    {
        this->uarts[i]->start();
    }
}

void Uart::reset(bool active)
{
    if (!active)
    {
        if (this->is_control_active)
        {
            if (this->clock != NULL)
            {
                this->clock->enqueue(this->init_event, 1);
            }
        }
    }
}

void Uart::set_control(bool active, int baudrate)
{
    this->is_control_active = active;
    this->baudrate = baudrate;
    this->is_control = active;
}


void Uart::handle_received_byte(uint8_t byte)
{
    if (this->proxy_file)
    {
        this->top->proxy->send_payload(this->proxy_file, std::to_string(this->req), &byte, 1);
    }
    else if (this->is_control)
    {
        this->top->handle_received_byte(uart_byte);
    }
    else if (this->dev)
    {
        dev->handle_received_byte(uart_byte);
    }
}


void Uart::uart_tx_sampling()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampling bit (value: %d)\n", uart_current_tx);

    switch (this->rx_state)
    {
        case UART_TX_STATE_DATA:
        {
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Received data bit (data: %d)\n", uart_current_tx);
            uart_byte = (uart_byte >> 1) | (uart_current_tx << 7);
            uart_nb_bits++;
            if (uart_nb_bits == 8)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sampled TX byte (value: 0x%x)\n", uart_byte);

                if (!this->is_usart)
                {
                    this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for stop bit\n");
                    if (this->tx_parity_en)
                        this->rx_state = UART_TX_STATE_PARITY;
                    else
                    {
                        this->rx_state = UART_TX_STATE_STOP;
                    }
                }
                else
                {
                    uart_tx_wait_start = true;
                }
                this->handle_received_byte(uart_byte);
            }
            break;
        }
        case UART_TX_STATE_PARITY:
        {
            this->rx_state = UART_TX_STATE_STOP;
            break;
        }
        case UART_TX_STATE_STOP:
        {
            if (uart_current_tx == 1)
            {
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Received stop bit\n", uart_current_tx);
                uart_tx_wait_start = true;
                this->uart_stop_tx_sampling();
            }
            break;
        }

        default:
            break;
    }
}


void Uart::check_send_byte()
{
    if (!this->is_usart && this->tx_pending_bits && !this->uart_tx_event->is_enqueued())
    {
        if (this->rtr == 0 || this->is_control_active || !this->flow_control)
        {
            this->tx_clock->reenqueue(this->uart_tx_event, 2);
        }
    }
}


void Uart::send_buffer(uint8_t *buffer, int size)
{
    if (this->pending_buffers.size() == 0 && this->tx_pending_bits == 0)
    {
        for (int i=1; i<size; i++)
        {
            this->pending_buffers.push(buffer[i]);
        }

        this->send_byte(buffer[0]);
    }
    else
    {
        for (int i=0; i<size; i++)
        {
            this->pending_buffers.push(buffer[i]);
        }
    }
}


void Uart::send_byte(uint8_t byte)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Send byte (value: 0x%x)\n", byte);

    this->tx_pending_byte = byte;
    this->tx_pending_bits = 8;

    if (this->is_usart)
    {
        this->tx_state = UART_TX_STATE_DATA;
        //this->send_bit();
    }
    else
    {
        this->tx_state = UART_TX_STATE_START;
        // Work-around. Since frequency is now changed only at end of current cycle, we need to set it
        // to zero so that the next time we set a real frequency, this is immediate.
        // To do it properly, the testbench should use timed events.
        this->tx_clock_cfg.set_frequency(0);
        this->tx_clock_cfg.set_frequency(this->baudrate*2);

        this->check_send_byte();
    }
}


Uart::Uart(Testbench *top, int id)
: top(top), id(id)
{
    this->uart_sampling_event = top->event_new((vp::Block *)this, Uart::uart_sampling_handler);
    this->uart_tx_event = top->event_new((vp::Block *)this, Uart::uart_tx_handler);
    this->init_event = top->event_new((vp::Block *)this, Uart::init_handler);
    this->itf.set_sync_meth(&Uart::sync);
    this->itf.set_sync_full_meth(&Uart::sync_full);
    this->top->new_slave_port("uart" + std::to_string(this->id), &this->itf, (vp::Block *)this);
    this->top->new_master_port( "uart" + std::to_string(this->id) + "_clock_cfg", &clock_cfg, (vp::Block *)this);
    this->top->new_master_port("uart" + std::to_string(this->id) + "_tx_clock_cfg", &tx_clock_cfg, (vp::Block *)this);

    this->clock_itf.set_reg_meth(&Uart::clk_reg);
    this->top->new_slave_port("uart" + std::to_string(this->id) + "_clock", &this->clock_itf, (vp::Block *)this);

    this->tx_clock_itf.set_reg_meth(&Uart::tx_clk_reg);
    this->top->new_slave_port("uart" + std::to_string(this->id) + "_tx_clock", &this->tx_clock_itf, (vp::Block *)this);

    top->traces.new_trace("uart_" + std::to_string(id), &trace, vp::DEBUG);

    this->uart_current_tx = 0;
    this->is_usart = 0;

    this->is_control_active = false;
    this->proxy_file = NULL;
}


void Uart::start()
{
}


void Uart::tx_clk_reg(vp::Component *__this, vp::Component *clock)
{
    Uart *_this = (Uart *)__this;
    _this->tx_clock = (vp::ClockEngine *)clock;
}


void Uart::clk_reg(vp::Component *__this, vp::Component *clock)
{
    Uart *_this = (Uart *)__this;
    _this->clock = (vp::ClockEngine *)clock;
}


void Uart::init_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Uart *_this = (Uart *)__this;
    _this->itf.sync_full(1, 2, 2, 0xf);
}


void Uart::uart_sampling_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Uart *_this = (Uart *)__this;

    _this->uart_tx_sampling();

    if (_this->uart_sampling_tx)
    {
        _this->clock->enqueue(_this->uart_sampling_event, 2*10);
    }
}


void Uart::sync_full(vp::Block *__this, int data, int clk, int rtr, unsigned int mask)
{
    Uart *_this = (Uart *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "UART sync (data: %d, clk: %d, rtr: %d)\n", data, clk, rtr);
    _this->rtr = rtr;
    _this->clk = clk ^ _this->polarity;

    _this->check_send_byte();

    Uart::sync(__this, data);
}


void Uart::sync(vp::Block *__this, int data)
{
    Uart *_this = (Uart *)__this;

    if (!_this->is_control && !_this->dev && !_this->proxy_file)
        return;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "UART control sync (value: %d, waiting_start: %d)\n", data, _this->uart_tx_wait_start);

    int prev_data = _this->uart_current_tx;
    _this->uart_current_tx = data;

    if (_this->uart_tx_wait_start && prev_data == 1 && data == 0)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received start bit\n");

        if (!_this->is_usart)
        {
            _this->uart_start_tx_sampling(_this->baudrate);
        }
        _this->uart_tx_wait_start = false;
        _this->rx_state = UART_TX_STATE_DATA;
        _this->uart_nb_bits = 0;
    }
    else if (_this->is_usart)
    {
        if (_this->prev_clk != _this->clk)
        {
            int high = _this->clk ^ _this->phase;

            if (high)
            {
                _this->uart_tx_sampling();
            }
            else
            {
                _this->send_bit();
            }
        }
    }

    _this->prev_clk = _this->clk;
}


void Uart::set_cts(int cts)
{
    this->tx_cts = cts;
    this->itf.sync_full(this->tx_bit, 2, this->tx_cts, 0xf);
}


void Uart::uart_tx_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Uart *_this = (Uart *)__this;
    _this->send_bit();
}


void Uart::send_bit()
{
    int bit;

    switch (this->tx_state)
    {
        case UART_TX_STATE_START:
        {
            this->tx_parity = 0;
            this->tx_state = UART_TX_STATE_DATA;
            this->tx_current_stop_bits = this->tx_stop_bits;
            bit = 0;
            break;
        }
        case UART_TX_STATE_DATA:
        {
            if (this->tx_pending_bits > 0)
            {
                bit = this->tx_pending_byte & 1;
                this->tx_pending_byte >>= 1;
                this->tx_pending_bits -= 1;
                this->tx_parity ^= bit;

                if (this->tx_pending_bits == 0)
                {
                    if (this->is_usart)
                    {
                        this->dev->send_byte_done();
                    }
                    else
                    {
                        if (this->tx_parity_en)
                            this->tx_state = UART_TX_STATE_PARITY;
                        else
                        {
                            this->tx_state = UART_TX_STATE_STOP;
                        }
                    }
                }
            }
            break;
        }
        case UART_TX_STATE_PARITY:
        {
            bit = this->tx_parity;
            this->tx_state = UART_TX_STATE_STOP;
            break;
        }
        case UART_TX_STATE_STOP:
        {
            bit = 1;
            this->tx_current_stop_bits--;
            if (this->tx_current_stop_bits == 0)
            {
                this->tx_state = UART_TX_STATE_START;
                if (this->pending_buffers.size())
                {
                    uint8_t byte = this->pending_buffers.front();
                    this->pending_buffers.pop();
                    this->send_byte(byte);
                }
                else if (this->dev)
                {
                    this->dev->send_byte_done();
                }

            }
            break;
        }
    }

    this->tx_bit = bit;
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending bit (bit: %d)\n", bit);
    this->itf.sync_full(this->tx_bit, 2, this->tx_cts, 0xf);

    if (!this->is_usart && this->tx_state != UART_TX_STATE_START)
    {
        this->tx_clock->reenqueue(this->uart_tx_event, 2);
    }
}


void Uart::uart_start_tx_sampling(int baudrate)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Start TX sampling (baudrate: %d)\n", this->baudrate);

    // Work-around. Since frequency is now changed only at end of current cycle, we need to set it
    // to zero so that the next time we set a real frequency, this is immediate.
    // To do it properly, the testbench should use timed events.
    this->clock_cfg.set_frequency(0);

    // We set the frequency to twice the baudrate to be able sampling in the
    // middle of the cycle
    this->clock_cfg.set_frequency(this->baudrate*2*10);

    this->uart_sampling_tx = 1;

    this->clock->reenqueue(this->uart_sampling_event, 3*10);
}


void Uart::uart_stop_tx_sampling(void)
{
    this->uart_sampling_tx = 0;

    if (this->uart_sampling_event->is_enqueued())
    {
        this->clock->cancel(this->uart_sampling_event);
    }
}


Uart_reply::Uart_reply(Testbench *top, Uart *uart) : top(top)
{
}


void Uart_reply::send_byte_done()
{
    this->top->send_byte_done();
}


void Testbench::send_byte_done()
{
    this->tx_buff_index++;
    if (this->tx_buff_index == this->tx_buff_size)
    {
        this->state = STATE_WAITING_CMD;
        this->current_req_size = 0;
    }
    else
    {
        this->uart_ctrl->send_byte(this->tx_buff[this->tx_buff_index]);
    }
}


void Testbench::handle_received_byte(uint8_t byte)
{
    if (this->state == STATE_WAITING_CMD)
    {
        if (this->current_req_size == 0)
        {
            this->cmd = byte;
        }
        else
        {
            this->cmd |= (byte << this->current_req_size);
        }

        this->current_req_size += 8;
        if (this->current_req_size < 32)
            return;

        switch (this->cmd & 0xffff) {
            case PI_TESTBENCH_CMD_GPIO_LOOPBACK:
            case PI_TESTBENCH_CMD_GPIO_GET_FREQUENCY:
            case PI_TESTBENCH_CMD_UART_CHECKER:
            case PI_TESTBENCH_CMD_SET_STATUS:
            case PI_TESTBENCH_CMD_GPIO_PULSE_GEN:
            case PI_TESTBENCH_CMD_SPIM_VERIF_SETUP:
            case PI_TESTBENCH_CMD_SPIM_VERIF_TRANSFER:
            case PI_TESTBENCH_CMD_SPIM_VERIF_SPI_WAKEUP:
            case PI_TESTBENCH_CMD_I2S_VERIF_SETUP:
            case PI_TESTBENCH_CMD_I2S_VERIF_START:
            case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_START:
            case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_STOP:
            case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_SETUP:
                this->state = STATE_WAITING_REQUEST;
                this->req_size = cmd >> 16;
                this->current_req_size = 0;
                if (this->req_size == 0)
                {
                    this->trace.fatal("Received zero size\n");
                }
                break;


            case PI_TESTBENCH_CMD_GET_TIME_PS:
                this->state = STATE_SENDING_REPLY;
                this->tx_buff = new uint8_t[8];
                *(uint64_t *)this->tx_buff = this->time.get_time();
                this->tx_buff_size = 8;
                this->tx_buff_index = 0;
                this->uart_ctrl->itf.sync_full(1, 2, 0, 0xf);
                this->uart_ctrl->send_byte(this->tx_buff[0]);
                break;

            default:
                this->trace.fatal("Received unknown request: 0x%2.2x\n", this->cmd & 0xffff);
        }
    }
    else if (this->state == STATE_WAITING_REQUEST)
    {
        this->req[this->current_req_size++] = byte;
        if (this->current_req_size == this->req_size)
        {
            this->state = STATE_WAITING_CMD;
            this->current_req_size = 0;

            switch (this->cmd & 0xffff) {
                case PI_TESTBENCH_CMD_GPIO_LOOPBACK:
                    this->handle_gpio_loopback();
                    break;

                case PI_TESTBENCH_CMD_GPIO_GET_FREQUENCY:
                    this->handle_gpio_get_frequency();
                    break;

                case PI_TESTBENCH_CMD_UART_CHECKER:
                    this->handle_uart_checker();
                    break;

                case PI_TESTBENCH_CMD_SET_STATUS:
                    this->handle_set_status();
                    break;

                case PI_TESTBENCH_CMD_GPIO_PULSE_GEN:
                    this->handle_gpio_pulse_gen();
                    break;

                case PI_TESTBENCH_CMD_SPIM_VERIF_SETUP:
                    this->handle_spim_verif_setup();
                    break;

                case PI_TESTBENCH_CMD_SPIM_VERIF_TRANSFER:
                    this->handle_spim_verif_transfer();
                    break;

                case PI_TESTBENCH_CMD_SPIM_VERIF_SPI_WAKEUP:
                    this->handle_spim_verif_spi_wakeup();
                    break;

                case PI_TESTBENCH_CMD_I2S_VERIF_SETUP:
                    this->handle_i2s_verif_setup();
                    break;

                case PI_TESTBENCH_CMD_I2S_VERIF_START:
                    this->handle_i2s_verif_start();
                    break;

                case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_SETUP:
                    this->handle_i2s_verif_slot_setup();
                    break;

                case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_START:
                {
                    this->handle_i2s_verif_slot_start(std::vector<int>());
                    break;
                }

                case PI_TESTBENCH_CMD_I2S_VERIF_SLOT_STOP:
                {
                    this->handle_i2s_verif_slot_stop();
                    break;
                }
            }
        }
    }
}


void Spi::create_loader(js::Config *load_config)
{
    uint8_t enabled;
    uint8_t itf;
    uint8_t cs;
    uint8_t is_master;
    uint16_t mem_size_log2;
    pi_testbench_req_spim_verif_setup_t config;
    config.enabled = 1;
    config.itf = this->itf_id;
    config.cs = 0;
    config.is_master = 1;
    config.mem_size_log2 = 20;
    config.dummy_cycles = load_config->get_child_int("dummy_cycles");

    this->spim_verif = new Spim_verif(this->top, this, &this->itf, &config);

    this->spim_verif->enqueue_spi_load(load_config);
}


void Spi::sync(vp::Block *__this, int sck, int data_0, int data_1, int data_2, int data_3, int mask)
{
    Spi *_this = (Spi *)__this;
    if (_this->spim_verif)
    {
        _this->spim_verif->sync(sck, data_0, data_1, data_2, data_3, mask);
    }
}


void Spi::cs_sync(vp::Block *__this, bool active)
{
    Spi *_this = (Spi *)__this;
    if (_this->spim_verif)
    {
        _this->spim_verif->cs_sync(!active);
    }
}



void Testbench::gpio_sync(vp::Block *__this, int value, int id)
{
    Testbench *_this = (Testbench *)__this;
    Gpio *gpio = _this->gpios[id];

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received GPIO sync (id: %d, value: %d)\n", id, value);

    if (gpio->get_frequency && gpio->value != value)
    {
        if (value == 1)
        {
            if (gpio->get_frequency_current_period)
            {
                gpio->get_frequency_period += _this->time.get_time() - gpio->get_frequency_start;
            }

            gpio->get_frequency_start = _this->time.get_time();
            gpio->get_frequency_current_period++;

            if (gpio->get_frequency_current_period == gpio->get_frequency_nb_period + 1)
            {
                pi_testbench_req_gpio_get_frequency_reply_t *reply = new pi_testbench_req_gpio_get_frequency_reply_t;
                reply->period = gpio->get_frequency_period / gpio->get_frequency_nb_period;
                reply->width = gpio->get_frequency_width / gpio->get_frequency_nb_period;

                _this->trace.msg(vp::Trace::LEVEL_INFO, "Finished sampling frequency (gpio: %d, period: %ld, width: %ld)\n", id, reply->period, reply->width);

                _this->tx_buff = (uint8_t *)reply;
                _this->tx_buff_size = sizeof(pi_testbench_req_gpio_get_frequency_reply_t);
                _this->tx_buff_index = 0;

                _this->uart_ctrl->send_byte(_this->tx_buff[0]);

                gpio->get_frequency = false;
            }
        }
        else
        {
            if (gpio->get_frequency_start != -1)
            {
                gpio->get_frequency_width += _this->time.get_time() - gpio->get_frequency_start;
            }
        }
    }

    gpio->value = value;

    if (gpio->loopback != -1)
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Generating gpio on loopback (id: %d)\n", gpio->loopback);
        _this->gpios[gpio->loopback]->itf.sync(value);
    }
}


void I2C::conf(Testbench *top, int id)
{
    this->top = top;
    this->id = id;
    this->state = I2C_STATE_WAIT_START;
    this->prev_sda = 1;
    this->pending_send_ack = false;
}


void I2C::handle_byte()
{
    this->top->trace.msg(vp::Trace::LEVEL_DEBUG, "Received I2C byte (id: %d, value: 0x%x)\n", this->id, this->pending_data & 0xff);
}


void I2C::sync(int scl, int sda)
{
    this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Received I2C sync (id: %d, scl: %d, sda: %d)\n", this->id, scl, sda);

    if (scl == 1 && this->prev_sda != sda)
    {
        if (this->prev_sda == 1)
        {
            this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Received I2C start bit (id: %d)\n", id);

            this->state = I2C_STATE_WAIT_ADDRESS;
            this->address = 0;
            this->pending_bits = 8;
        }
        else
        {
            this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Received I2C stop bit (id: %d)\n", id);
            this->state = I2C_STATE_WAIT_START;
        }
    }
    else
    {
        if (scl == 0)
        {
            if (this->pending_send_ack)
            {
                this->pending_send_ack = false;
                this->itf.sync(2, 1);
            }
        }
        else
        {
            switch (this->state)
            {
                case I2C_STATE_WAIT_START:
                    break;
                case I2C_STATE_WAIT_ADDRESS:
                {
                    if (this->pending_bits > 1)
                    {
                        this->address = (this->address << 1) | sda;
                    }
                    else
                    {
                        this->is_read = sda;
                    }
                    this->pending_bits--;
                    if (this->pending_bits == 0)
                    {
                        this->state = I2C_STATE_ACK;
                        this->pending_bits = 8;
                    }
                    break;
                }

                case I2C_STATE_GET_DATA:
                {
                    //if (sda_out)
                    //{
                    //    *sda_out = (this->pending_send_byte >> 7) & 1;
                    //    this->pending_send_byte <<= 1;
                    //}
                    this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Got I2C data (id: %d, sda: %d)\n", id, this->pending_bits);
                    this->pending_data = (this->pending_data << 1) | sda;
                    this->pending_bits--;
                    if (this->pending_bits == 0)
                    {
                        this->pending_bits = 8;
                        this->handle_byte();
                        this->state = I2C_STATE_ACK;
                    }
                    break;
                }
                
                case I2C_STATE_ACK:
                {
                    this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Generate I2C ack (id: %d)\n", id);

                    this->itf.sync(2, 0);
                    this->state = I2C_STATE_GET_DATA;
                    break;
                }
            }
        }
    }

    this->prev_sda = sda;
}


void Testbench::i2c_sync(vp::Block *__this, int scl, int sda, int id)
{
    Testbench *_this = (Testbench *)__this;

    _this->i2cs[id].sync(scl, sda);
}



void Testbench::handle_uart_checker()
{
    pi_testbench_req_t *req = (pi_testbench_req_t *)this->req;

    if (req->uart.enabled)
    {
        Uart *uart = this->uarts[req->uart.id];
        uart->set_control(false, 0);
        Uart_dev *dev = new Uart_flow_control_checker(this, uart, req);
        uart->set_dev(dev);
        uart->is_usart = req->uart.usart;
        uart->polarity = req->uart.polarity;
        uart->phase = req->uart.phase;
        uart->flow_control = req->uart.flow_control;
        uart->tx_parity_en = req->uart.parity;
    }
}




void Testbench::handle_set_status()
{
    pi_testbench_req_t *req = (pi_testbench_req_t *)this->req;

    this->time.get_engine()->quit(req->set_status.status);
}


void Testbench::handle_gpio_loopback()
{
    pi_testbench_req_t *req = (pi_testbench_req_t *)this->req;

    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling GPIO loopback (enabled: %d, output: %d, intput: %d)\n", req->gpio.enabled, req->gpio.output, req->gpio.input);

    if (req->gpio.enabled)
    {
        this->gpios[req->gpio.output]->loopback = req->gpio.input;

        if (!this->gpios[req->gpio.input]->itf.is_bound())
        {
            this->trace.force_warning("Trying to set GPIO while interface is not connected\n");
        }
        else
        {
            this->gpios[req->gpio.input]->itf.sync(this->gpios[req->gpio.output]->value);
        }
    }
    else
    {
        this->gpios[req->gpio.output]->loopback = -1;
    }
}


void Testbench::handle_gpio_get_frequency()
{
    pi_testbench_req_t *req = (pi_testbench_req_t *)this->req;

    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling GPIO get frequency (gpio: %d, nb_period: %d)\n", req->gpio_get_frequency.gpio, req->gpio_get_frequency.nb_period);

    Gpio *gpio = this->gpios[req->gpio_get_frequency.gpio];

    gpio->get_frequency = req->gpio_get_frequency.nb_period != 0;

    if (gpio->get_frequency)
    {
        gpio->get_frequency_current_period = 0;
        gpio->get_frequency_nb_period = req->gpio_get_frequency.nb_period;
        gpio->get_frequency_period = 0;
        gpio->get_frequency_width = 0;
        gpio->get_frequency_start = -1;
    }
}


void Testbench::handle_gpio_pulse_gen()
{
    pi_testbench_req_t *req = (pi_testbench_req_t *)this->req;

    bool enabled = req->gpio_pulse_gen.enabled;
    int gpio_id = req->gpio_pulse_gen.gpio;
    int64_t duration_ps = req->gpio_pulse_gen.duration_ps;
    int64_t period_ps = req->gpio_pulse_gen.period_ps;
    Gpio *gpio = this->gpios[gpio_id];
    
    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling GPIO pulse generator (gpio: %d, enabled: %d, duration_ps: %ld, period_ps: %ld)\n",
        gpio_id, enabled, duration_ps, period_ps);

    gpio->pulse_enabled = enabled;

    gpio->itf.sync(0);

    if (enabled)
    {
        gpio->pulse_duration_ps = duration_ps;
        gpio->pulse_period_ps = period_ps;
        gpio->pulse_gen_rising_edge = true;
        this->event_enqueue(gpio->pulse_event, req->gpio_pulse_gen.first_delay_ps);
    }
}

void Testbench::handle_spim_verif_setup()
{
    pi_testbench_req_spim_verif_setup_t *req = (pi_testbench_req_spim_verif_setup_t *)this->req;

    int itf = req->itf;
    int cs = req->cs;

    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling Spim verif setup (itf: %d, cs: %d, mem_size: %d)\n",
        itf, cs, 1<<req->mem_size_log2);

    this->spis[cs + itf*4]->spim_verif_setup(req);
}


void Testbench::handle_spim_verif_transfer()
{
    pi_testbench_req_spim_verif_transfer_t *req = (pi_testbench_req_spim_verif_transfer_t *)this->req;

    int itf = req->itf;
    int cs = req->cs;

    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling Spim verif transfer (itf: %d, cs: %d)\n",
        itf, cs);

    this->spis[cs + itf*4]->spim_verif_transfer((pi_testbench_req_spim_verif_transfer_t *)req);
}


void Testbench::handle_spim_verif_spi_wakeup()
{
    pi_testbench_req_spim_verif_spi_wakeup_t *req = (pi_testbench_req_spim_verif_spi_wakeup_t *)this->req;

    int itf = req->itf;
    int cs = req->cs;

    this->trace.msg(vp::Trace::LEVEL_INFO, "Handling Spim verif spi wakeup (itf: %d, cs: %d)\n",
        itf, cs);

    this->spis[cs + itf*4]->spim_verif_spi_wakeup((pi_testbench_req_spim_verif_spi_wakeup_t *)req);
}


void Testbench::handle_i2s_verif_setup()
{
    pi_testbench_i2s_verif_config_t *req = (pi_testbench_i2s_verif_config_t *)this->req;
    int itf = req->itf;

    if (itf >= this->nb_i2s)
    {
        this->trace.fatal("Invalid I2S interface (id: %d, nb_interfaces: %d)", itf, this->nb_i2s);
        return;
    }

    this->i2ss[itf]->i2s_verif_setup(req);
}

void Testbench::handle_i2s_verif_start()
{
    pi_testbench_i2s_verif_start_config_t *req = (pi_testbench_i2s_verif_start_config_t *)this->req;
    int itf = req->itf;

    if (itf >= this->nb_i2s)
    {
        this->trace.fatal("Invalid I2S interface (id: %d, nb_interfaces: %d)", itf, this->nb_i2s);
        return;
    }

    this->i2ss[itf]->i2s_verif_start(req);
}


void Testbench::handle_i2s_verif_slot_setup()
{
    pi_testbench_i2s_verif_slot_config_t *req = (pi_testbench_i2s_verif_slot_config_t *)this->req;
    int itf = req->itf;

    if (itf >= this->nb_i2s)
    {
        this->trace.fatal("Invalid I2S interface (id: %d, nb_interfaces: %d)", itf, this->nb_i2s);
        return;
    }

    this->i2ss[itf]->i2s_verif_slot_setup(req);
}


void Testbench::handle_i2s_verif_slot_start(std::vector<int> slots)
{
    pi_testbench_i2s_verif_slot_start_config_t *req = (pi_testbench_i2s_verif_slot_start_config_t *)this->req;
    int itf = req->itf;

    if (itf >= this->nb_i2s)
    {
        this->trace.fatal("Invalid I2S interface (id: %d, nb_interfaces: %d)", itf, this->nb_i2s);
        return;
    }

    this->i2ss[itf]->i2s_verif_slot_start(req, slots);
}


void Testbench::handle_i2s_verif_slot_stop()
{
    pi_testbench_i2s_verif_slot_stop_config_t *req = (pi_testbench_i2s_verif_slot_stop_config_t *)this->req;
    int itf = req->itf;

    if (itf >= this->nb_i2s)
    {
        this->trace.fatal("Invalid I2S interface (id: %d, nb_interfaces: %d)", itf, this->nb_i2s);
        return;
    }

    this->i2ss[itf]->i2s_verif_slot_stop(req);
}


std::string Testbench::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req)
{
    bool error = false;
    string error_str = "";
    try
    {
        if (args[0] == "i2s")
        {
            if (args[1] == "setup")
            {
                pi_testbench_i2s_verif_config_t *config = (pi_testbench_i2s_verif_config_t *)this->req;

                this->proxy = proxy;

                *config = {};

                std::vector<std::string> params = {args.begin() + 2, args.end()};

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "enabled")
                    {
                        config->enabled = value;
                    }
                    else if (name == "sampling_freq")
                    {
                        config->sampling_freq = value;   
                    }
                    else if (name == "word_size")
                    {
                        config->word_size = value;
                    }
                    else if (name == "nb_slots")
                    {
                        config->nb_slots = value;
                    }
                    else if (name == "is_pdm")
                    {
                        config->is_pdm = value;
                        this->is_pdm = value;
                    }
                    else if (name == "is_full_duplex")
                    {
                        config->is_full_duplex = value;
                    }
                    else if (name == "is_ext_clk")
                    {
                        config->is_ext_clk = value;
                    }
                    else if (name == "is_ext_ws")
                    {
                        config->is_ext_ws = value;
                    }
                    else if (name == "is_sai0_clk")
                    {
                        config->is_sai0_clk = value;
                    }
                    else if (name == "is_sai0_ws")
                    {
                        config->is_sai0_ws = value;
                    }
                    else if (name == "clk_polarity")
                    {
                        config->clk_polarity = value;
                    }
                    else if (name == "ws_polarity")
                    {
                        config->ws_polarity = value;
                    }
                    else if (name == "ws_delay")
                    {
                        config->ws_delay = value;
                    }
                    else
                    {
                        this->trace.fatal("Received invalid property (name: %s)\n", name.c_str());
                    }
                }

                this->handle_i2s_verif_setup();
            }
            else if (args[1] == "clk_start" || args[1] == "clk_stop")
            {
                pi_testbench_i2s_verif_start_config_t *config = (pi_testbench_i2s_verif_start_config_t *)this->req;

                *config = {};

                int id = strtol(args[1].c_str(), NULL, 0);

                config->itf = id;
                config->start = args[1] == "clk_start";

                this->handle_i2s_verif_start();
            }
            else if (args[1] == "slot_setup")
            {
                pi_testbench_i2s_verif_slot_config_t *config = (pi_testbench_i2s_verif_slot_config_t *)this->req;

                *config = {};

                std::vector<std::string> params = {args.begin() + 2, args.end()};

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "slot")
                    {
                        config->slot = value;
                    }
                    else if (name == "is_rx")
                    {
                        config->is_rx = value;   
                    }
                    else if (name == "enabled")
                    {
                        config->enabled = value;
                    }
                    else if (name == "word_size")
                    {
                        config->word_size = value;
                    }
                    else if (name == "format")
                    {
                        config->format = value;
                    }
                }

                this->handle_i2s_verif_slot_setup();
            }
            else if (args[1] == "slot_rx_file_reader")
            {
                pi_testbench_i2s_verif_slot_start_config_t *config = (pi_testbench_i2s_verif_slot_start_config_t *)this->req;
                char *filepath = (char *)config + sizeof(pi_testbench_i2s_verif_slot_start_config_t);
                int max_filepath_len = PI_TESTBENCH_MAX_REQ_SIZE - sizeof(pi_testbench_i2s_verif_slot_start_config_t);

                *config = {};
                config->rx_file_reader.pcm2pdm_is_true = FALSE;

                std::vector<std::string> params = {args.begin() + 2, args.end()};
                std::vector<int> slots;

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "slot")
                    {
                        config->slot = value;
                        slots.push_back(value);
                    }
                    else if (name == "width")
                    {
                        config->rx_file_reader.width = value;
                    }
                    else if (name == "filepath")
                    {
                        if (max_filepath_len < value_str.size() + 1)
                        {
                            this->trace.force_warning("File path is too long: %s", value_str.c_str());
                        }
                        else
                        {
                            strcpy(filepath, value_str.c_str());
                        }
                    }
                    else if (name == "filetype")
                    {
                        if (value_str == "wav")
                        {
                            config->rx_file_reader.type = PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_WAV;
                            if (this->is_pdm){
                                config->rx_file_reader.pcm2pdm_is_true = TRUE;
                            }
                        }
                        else if (value_str == "bin")
                        {
                            config->rx_file_reader.type = PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_BIN;
                        }
                        else if (value_str == "au")
                        {
                            config->rx_file_reader.type = PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_AU;
                        }
                        else
                        {
                            config->rx_file_reader.type = 0;
                        }
                    }
                    else if (name == "encoding")
                    {
                        if (value_str == "pcm"){
                            if (this->is_pdm){
                                config->rx_file_reader.pcm2pdm_is_true = TRUE;
                            }
                            config->rx_file_reader.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                        else if (value_str == "asis")
                        {
                            config->rx_file_reader.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                        else if (value_str == "plusminus")
                        {
                            config->rx_file_reader.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_PLUSMINUS;
                        }
                        else
                        {
                            config->rx_file_reader.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                    }
                    else if (name == "interpolation_ratio_shift")
                    {
                        config->rx_file_reader.conversion_config.interpolation_ratio_shift = value;
                    }
                    else if (name == "interpolation_type")
                    {
                        if (value_str == "iir"){
                            config->rx_file_reader.conversion_config.interpolation_type = IIR;
                        }
                        else if (value_str == "linear"){
                            config->rx_file_reader.conversion_config.interpolation_type = LINEAR;
                        }
                        else{
                            printf("Interpolation type unknown, set to default value : linear");
                            config->rx_file_reader.conversion_config.interpolation_type = LINEAR;

                        }
                    }
                }

                config->type = PI_TESTBENCH_I2S_VERIF_RX_FILE_READER;
                config->rx_file_reader.nb_samples = -1;
                config->rx_file_reader.filepath_len = strlen(filepath) + 1;

                this->handle_i2s_verif_slot_start(slots);
            }
            else if (args[1] == "slot_tx_file_dumper")
            {
                pi_testbench_i2s_verif_slot_start_config_t *config = (pi_testbench_i2s_verif_slot_start_config_t *)this->req;
                char *filepath = (char *)config + sizeof(pi_testbench_i2s_verif_slot_start_config_t);
                int max_filepath_len = PI_TESTBENCH_MAX_REQ_SIZE - sizeof(pi_testbench_i2s_verif_slot_start_config_t);

                *config = {};
                config->tx_file_dumper.pdm2pcm_is_true = FALSE;

                std::vector<std::string> params = {args.begin() + 2, args.end()};
                std::vector<int> slots;

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "slot")
                    {
                        config->slot = value;
                        slots.push_back(value);
                    }
                    else if (name == "filepath")
                    {
                        if (max_filepath_len < value_str.size() + 1)
                        {
                            this->trace.force_warning("File path is too long: %s", value_str.c_str());
                        }
                        else
                        {
                            strcpy(filepath, value_str.c_str());
                        }
                    }
                    else if (name == "width")
                    {
                        config->tx_file_dumper.width = value;
                    }
                    else if (name == "filetype")
                    {
                        if (value_str == "wav")
                        {
                            config->tx_file_dumper.type = PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER_TYPE_WAV;
                            if (this->is_pdm){
                                config->tx_file_dumper.pdm2pcm_is_true = TRUE;
                            }
                        }
                        else if (value_str == "bin")
                        {
                            config->tx_file_dumper.type = PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER_TYPE_BIN;
                        }
                        else if (value_str == "au")
                        {
                            config->tx_file_dumper.type = PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER_TYPE_AU;
                        }
                        else
                        {
                            config->tx_file_dumper.type = 0;
                        }
                    }
                    else if (name == "encoding")
                    {
                        if (value_str == "pcm"){
                            if (this->is_pdm){
                                config->tx_file_dumper.pdm2pcm_is_true = TRUE;
                            }
                            config->tx_file_dumper.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                        if (value_str == "asis")
                        {
                            config->tx_file_dumper.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                        else if (value_str == "plusminus")
                        {
                            config->tx_file_dumper.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_PLUSMINUS;
                        }
                        else
                        {
                            config->tx_file_dumper.encoding = PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_ASIS;
                        }
                    }
                    else if (name == "cic_n")
                    {
                        config->tx_file_dumper.conversion_config.cic_n = value;
                    }
                    else if (name == "cic_m")
                    {
                        config->tx_file_dumper.conversion_config.cic_m = value;
                    }
                    else if (name == "cic_r")
                    {
                        config->tx_file_dumper.conversion_config.cic_r = value;
                    }
                    else if (name == "cic_shift")
                    {
                        config->tx_file_dumper.conversion_config.cic_shift = value;
                    }
                    else if (name == "filter_coef")
                    {
                        config->tx_file_dumper.conversion_config.filter_coef = value;
                    }
                    else if (name == "wav_sampling_freq")
                    {
                        config->tx_file_dumper.wav_sampling_freq = value;
                    }
                }

                config->type = PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER;
                config->tx_file_dumper.nb_samples = -1;
                config->tx_file_dumper.filepath_len = strlen(filepath) + 1;

                this->handle_i2s_verif_slot_start(slots);
            }
            else if (args[1] == "slot_stop")
            {
                pi_testbench_i2s_verif_slot_stop_config_t *config = (pi_testbench_i2s_verif_slot_stop_config_t *)this->req;
                char *filepath = (char *)config + sizeof(pi_testbench_i2s_verif_slot_stop_config_t);

                *config = {};

                std::vector<std::string> params = {args.begin() + 2, args.end()};

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "slot")
                    {
                        config->slot = value;
                    }
                    else if (name == "stop_rx")
                    {
                        config->stop_rx = value;
                    }
                    else if (name == "stop_tx")
                    {
                        config->stop_tx = value;
                    }
                }

                this->handle_i2s_verif_slot_stop();
            }
        }
        if (args[0] == "uart")
        {
            if (args[1] == "setup")
            {
                pi_testbench_req_uart_checker_t *config = (pi_testbench_req_uart_checker_t *)this->req;

                *config = {};

                std::vector<std::string> params = {args.begin() + 2, args.end()};

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->id = value;
                    }
                    else if (name == "enabled")
                    {
                        config->enabled = value;
                    }
                    else if (name == "word_size")
                    {
                        config->word_size = value;
                    }
                    else if (name == "baudrate")
                    {
                        config->baudrate = value;
                    }
                    else if (name == "stop_bits")
                    {
                        config->stop_bits = value;
                    }
                    else if (name == "parity_mode")
                    {
                        config->parity = value;
                    }
                    else if (name == "ctrl_flow")
                    {
                        config->flow_control = value;
                    }
                    else if (name == "usart_polarity")
                    {
                        config->polarity = value;
                    }
                    else if (name == "is_usart")
                    {
                        config->usart = value;
                    }
                    else if (name == "usart_phase")
                    {
                        config->phase = value;
                    }
                    else
                    {
                        return "err=1;msg=invalid option: " + name;
                    }
                }

                this->handle_uart_checker();
            }
            else if (args[1] == "tx")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int size = strtol(args[3].c_str(), NULL, 0);
                uint8_t *buffer = new uint8_t[size];
                int read_size = fread(buffer, 1, size, req_file);
                this->uarts[itf]->send_buffer(buffer, size);
            }
            else if (args[1] == "rx")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int enabled = strtol(args[3].c_str(), NULL, 0);

                if (enabled)
                {
                    int req = strtol(args[4].c_str(), NULL, 0);
                    this->uarts[itf]->proxy_file = reply_file;
                    this->uarts[itf]->req = req;
                }
                else
                {
                    this->uarts[itf]->proxy_file = NULL;
                }
            }
        }
        if (args[0] == "spi")
        {
            if (args[1] == "setup")
            {
                pi_testbench_req_spim_verif_setup_t *config = (pi_testbench_req_spim_verif_setup_t *)this->req;

                *config = {};

                std::vector<std::string> params = {args.begin() + 2, args.end()};

                for (std::string x: params)
                {
                    int pos = x.find_first_of("=");
                    std::string name = x.substr(0, pos);
                    std::string value_str = x.substr(pos + 1);
                    int value = strtol(value_str.c_str(), NULL, 0);

                    if (name == "itf")
                    {
                        config->itf = value;
                    }
                    else if (name == "enabled")
                    {
                        config->enabled = value;
                    }
                    else if (name == "cs")
                    {
                        config->cs = value;
                    }
                    else if (name == "is_master")
                    {
                        config->is_master = value;
                    }
                    else if (name == "polarity")
                    {
                        config->polarity = value;
                    }
                    else if (name == "phase")
                    {
                        config->phase = value;
                    }
                    else if (name == "mem_size_log2")
                    {
                        config->mem_size_log2 = value;
                    }
                    else if (name == "dummy_cycles")
                    {
                        config->dummy_cycles = value;
                    }
                    else
                    {
                        return "err=1;msg=invalid option: " + name;
                    }
                }

                this->spis[config->cs + config->itf*4]->gvcontrol = true;
                this->handle_spim_verif_setup();
            }

            if (args[1] == "full_duplex")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int cs = strtol(args[3].c_str(), NULL, 0);
                int size = strtol(args[4].c_str(), NULL, 0);
                uint8_t *buffer = new uint8_t[size];
                int read_size = fread(buffer, 1, size, req_file);
                this->spis[4 * itf + cs]->spim_verif->enqueue_buffer(buffer);
                uint64_t cmd = 0;
                SPIM_VERIF_BUILD_CMD(cmd, SPIM_VERIF_CMD_FULL_DUPLEX,
                                     SPIM_VERIF_CMD_BIT, SPIM_VERIF_CMD_WIDTH);
                SPIM_VERIF_BUILD_CMD(cmd, size*8,
                                     SPIM_VERIF_CMD_INFO_BIT, SPIM_VERIF_CMD_INFO_WIDTH);
                this->spis[4 * itf + cs]->spim_verif->enqueue_cmd(cmd);
            }

            if (args[1] == "tx")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int cs = strtol(args[3].c_str(), NULL, 0);
                int size = strtol(args[4].c_str(), NULL, 0);
                uint8_t *buffer = new uint8_t[size];
                int read_size = fread(buffer, 1, size, req_file);
                this->spis[4 * itf + cs]->spim_verif->enqueue_buffer(buffer);
                uint64_t cmd = 0;
                SPIM_VERIF_BUILD_CMD(cmd, SPIM_VERIF_CMD_READ,
                                     SPIM_VERIF_CMD_BIT, SPIM_VERIF_CMD_WIDTH);
                SPIM_VERIF_BUILD_CMD(cmd, size*8,
                                     SPIM_VERIF_CMD_INFO_BIT, SPIM_VERIF_CMD_INFO_WIDTH);
                this->spis[4 * itf + cs]->spim_verif->enqueue_cmd(cmd);
            }

            if(args[1] == "master_cmd")
            {
                // commands given in gvcontrol are from testbench point of vue
                // spim_verif code is from a gap point of vue :
                //       master = 0 means gap is slave so testbench is master.

                pi_testbench_req_spim_verif_transfer_t *config = new pi_testbench_req_spim_verif_transfer_t;
                int itf = strtol(args[2].c_str(), NULL, 0);
                int cs = strtol(args[3].c_str(), NULL, 0);
                uint32_t size = strtol(args[4].c_str(), NULL, 0);
                u_int32_t freq = strtol(args[5].c_str(), NULL, 0);
                u_int8_t duplex = strtol(args[6].c_str(), NULL, 0);
                u_int8_t rx = strtol(args[7].c_str(), NULL, 0);
                if(!rx || duplex)
                {
                    uint8_t *buffer = new uint8_t[size];
                    int read_size = fread(buffer, 1, size, req_file);
                    this->spis[4 * itf + cs]->spim_verif->enqueue_buffer(buffer);
                }

                config->is_master = 0;
                config->size = size;
                config->frequency = freq;
                config->is_duplex = duplex;
                config->is_rx = rx;
                config->is_boot_protocol = 0;
                config->address = 0;
                this->spis[4 * itf + cs]->spim_verif->enqueue_master_cmd(config);
            }

            if (args[1] == "rx")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int cs = strtol(args[3].c_str(), NULL, 0);
                int size = strtol(args[4].c_str(), NULL, 0);
                uint64_t cmd = 0;
                SPIM_VERIF_BUILD_CMD(cmd, SPIM_VERIF_CMD_WRITE,
                                     SPIM_VERIF_CMD_BIT, SPIM_VERIF_CMD_WIDTH);
                SPIM_VERIF_BUILD_CMD(cmd, size*8,
                                     SPIM_VERIF_CMD_INFO_BIT, SPIM_VERIF_CMD_INFO_WIDTH);
                this->spis[4 * itf + cs]->spim_verif->enqueue_cmd(cmd);
            }

            if (args[1] == "rx_enable")
            {
                int itf = strtol(args[2].c_str(), NULL, 0);
                int cs = strtol(args[3].c_str(), NULL, 0);
                int enabled = strtol(args[4].c_str(), NULL, 0);

                if (enabled)
                {
                    int req = strtol(args[5].c_str(), NULL, 0);
                    this->spis[4 * itf + cs]->spim_verif->proxy_file = reply_file;
                    this->spis[4 * itf + cs]->spim_verif->req = req;
                }
                else
                {
                    this->spis[4 * itf + cs]->spim_verif->proxy_file = NULL;
                }
            }
        }
    }
    catch (std::invalid_argument& e)
    {
        return "err=1;msg=" + std::string(e.what());
    }

    if (error)
    {
        return "err=1";
    }
    else
    {
        return "err=0";
    }
}


void Gpio::pulse_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Gpio *_this = (Gpio *)__this;

    _this->itf.sync(_this->pulse_gen_rising_edge);

    if (_this->pulse_gen_rising_edge)
    {
        _this->top->event_enqueue(_this->pulse_event, _this->pulse_duration_ps);
    }
    else if (_this->pulse_period_ps > 0)
    {
        _this->top->event_enqueue(_this->pulse_event, _this->pulse_period_ps - _this->pulse_duration_ps);
    }
    
    _this->pulse_gen_rising_edge ^= 1;
}


Gpio::Gpio(Testbench *top) : top(top)
{
    this->pulse_event = top->event_new((vp::Block *)this, Gpio::pulse_handler);
    this->get_frequency = false;
}


Spi::Spi(Testbench *top, int itf, int cs) : top(top)
{
    this->itf.set_sync_meth(&Spi::sync);
    this->top->new_slave_port("spi" + std::to_string(itf) + "_cs" + std::to_string(cs) + "_data", &this->itf, (vp::Block *)this);
    this->cs_itf.set_sync_meth(&Spi::cs_sync);
    this->top->new_slave_port("spi" + std::to_string(itf) + "_cs" + std::to_string(cs), &this->cs_itf, (vp::Block *)this);
    
    this->spim_verif = NULL;
    this->itf_id = itf;
    this->cs = cs;
}


void Spi::spim_verif_setup(pi_testbench_req_spim_verif_setup_t *config)
{
    if (this->spim_verif)
    {
        delete this->spim_verif;
        this->spim_verif = NULL;
    }

    if (config->enabled)
    {
        this->spim_verif = new Spim_verif(this->top, this, &this->itf, config);
    }
}


void Spi::spim_verif_transfer(pi_testbench_req_spim_verif_transfer_t *config)
{
    if (this->spim_verif)
    {
        this->spim_verif->transfer(config);
    }

}


void Spi::spim_verif_spi_wakeup(pi_testbench_req_spim_verif_spi_wakeup_t *config)
{
    if (this->spim_verif)
    {
        this->spim_verif->spi_wakeup(config);
    }
}



I2s::I2s(Testbench *top, int itf) : top(top)
{
    this->i2s_verif = NULL;
    this->itf_id = itf;
    this->clk_propagate = 0;
    this->ws_propagate = 0;

    this->itf.set_sync_meth(&I2s::sync);
    top->new_master_port("i2s" + std::to_string(itf), &this->itf, (vp::Block *)this);

    top->traces.new_trace("i2s_itf" + std::to_string(itf), &trace, vp::DEBUG);
}


void I2s::i2s_verif_slot_start(pi_testbench_i2s_verif_slot_start_config_t *config, std::vector<int> slots)
{
    if (!this->i2s_verif)
    {
        this->trace.fatal("Trying to start slot in inactive interface (itf: %d)", this->itf_id);
        return;
    }

    this->i2s_verif->slot_start(config, slots);
}


void I2s::i2s_verif_slot_stop(pi_testbench_i2s_verif_slot_stop_config_t *config)
{
    if (!this->i2s_verif)
    {
        this->trace.fatal("Trying to start slot in inactive interface (itf: %d)", this->itf_id);
        return;
    }

    this->i2s_verif->slot_stop(config);
}


void I2s::sync(vp::Block *__this, int sck, int ws, int sd, bool full_duplex)
{
    I2s *_this = (I2s *)__this;

    if (_this->ws_propagate)
    {
        for (int i=0; i<_this->top->nb_i2s; i++)
        {
            if ((_this->ws_propagate >> i) & 1)
            {
                _this->top->i2ss[i]->sync_ws(ws);
            }
        }
    }

    if (_this->clk_propagate)
    {
        for (int i=0; i<_this->top->nb_i2s; i++)
        {
            if ((_this->clk_propagate >> i) & 1)
            {
                _this->top->i2ss[i]->sync_sck(sck);
            }
        }
    }

    if (_this->i2s_verif)
    {
        _this->i2s_verif->sync(sck, ws, sd, full_duplex);
    }
}

void I2s::sync_sck(int sck)
{
    if (this->i2s_verif)
    {
        this->i2s_verif->sync_sck(sck);
    }
}

void I2s::sync_ws(int ws)
{
    if (this->i2s_verif)
    {
        this->i2s_verif->sync_ws(ws);
    }
}


void I2s::i2s_verif_setup(pi_testbench_i2s_verif_config_t *config)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "I2S verif setup (enabled: %d, sampling_rate: %d, word_size: %d, nb_slots: %d, ext_clk: %d, ext_ws: %d, pdm: %d)\n",
        config->enabled, config->sampling_freq, config->word_size, config->nb_slots, config->is_ext_clk, config->is_ext_ws, config->is_pdm);

    if (this->i2s_verif)
    {
        delete this->i2s_verif;
        this->i2s_verif = NULL;
    }

    if (config->enabled)
    {
        this->i2s_verif = new I2s_verif(this->top, &this->itf, this->itf_id, config);
    }
}


void I2s::i2s_verif_start(pi_testbench_i2s_verif_start_config_t *config)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "I2S verif start (start: %d)\n",
        config->start);

    if (!this->i2s_verif)
    {
        this->trace.fatal("Trying to start inactive interface (itf: %d)", this->itf_id);
        return;
    }

    this->i2s_verif->start(config);
}


void I2s::i2s_verif_slot_setup(pi_testbench_i2s_verif_slot_config_t *config)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "I2S verif slot setup (slot: %d, is_rx: %d, enabled: %d, word_size: %d, format: %x)\n",
        config->slot, config->is_rx, config->enabled, config->word_size, config->format);

    if (!this->i2s_verif)
    {
        this->trace.fatal("Trying to configure slot in inactive interface (itf: %d, slot: %d)", this->itf_id, config->slot);
        return;
    }

    this->i2s_verif->slot_setup(config);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Testbench(config);
}
