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

 #include "uart_adapter.hpp"


UartAdapter::UartAdapter(vp::Component *top, vp::Block *parent, std::string name, std::string itf_name)
    : vp::Block(parent, name), top(top),
    sample_rx_bit_event(this, &UartAdapter::rx_sample_handler),
    tx_send_bit_event(this, &UartAdapter::tx_send_handler),
    tx_init_event(this, &UartAdapter::tx_init_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->in.set_sync_full_meth(&UartAdapter::sync);
    top->new_slave_port(itf_name, &this->in, this);

    this->baudrate_set(115200);
    this->data_bits_set(8);
    this->stop_bits_set(1);
    this->parity_set(false);
    this->ctrl_flow_set(false);
}

void UartAdapter::sync(vp::Block *__this, int data, int clk, int rts, unsigned int mask)
{
    UartAdapter *_this = (UartAdapter *)__this;

    // TODO on some platforms, we get a sync at time 0 with random values, which disturbs the model
    // Once we have a proper uart bus handling high impedance correctly, we should be able
    // to remove that.
    if (_this->time.get_time() == 0)
    {
        return;
    }

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sync (value: %d, loopback: %d)\n", data, _this->loopback);

    if (rts != _this->rts)
    {
        _this->rts = rts;

        if (rts && _this->tx_flow_ctrl_event)
        {
            _this->tx_flow_ctrl_event->exec();
        }

        if (!rts && _this->tx_ready_event)
        {
            _this->tx_ready_event->exec();
        }
    }

    if (_this->loopback)
    {
        _this->in.sync_full(data, 2, _this->cts, _this->sync_mask);
    }

    _this->rx_current_bit = data;

    if (_this->rx_state == UART_RX_STATE_WAIT_START && data == 0)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received start bit\n");

        _this->rx_start_sampling(_this->baudrate);
    }
}

void UartAdapter::rx_start_sampling(int baudrate)
{
    this->rx_state = UART_RX_STATE_DATA;
    this->rx_pending_bits = this->data_bits;
    this->rx_parity = 0;

    // Wait 1 period and a half to start sampling in the middle of the next bit
    this->sample_rx_bit_event.enqueue(3*this->period/2);
}


void UartAdapter::rx_sample_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapter *_this = (UartAdapter *)__this;

    _this->sample_rx_bit();

    if (_this->rx_state != UART_RX_STATE_WAIT_START)
    {
        _this->sample_rx_bit_event.enqueue(_this->period);
    }
}



void UartAdapter::sample_rx_bit()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampling bit (value: %d)\n", this->rx_current_bit);

    switch (this->rx_state)
    {
        case UART_RX_STATE_DATA:
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Received data bit (data: %d)\n", this->rx_current_bit);

            this->rx_pending_byte = (this->rx_pending_byte >> 1)
                | (this->rx_current_bit << (this->data_bits - 1));

            this->rx_parity ^= this->rx_current_bit;

            this->rx_pending_bits--;
            if (this->rx_pending_bits == 0)
            {
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampled RX byte (value: 0x%x)\n", this->rx_pending_byte);

                if (this->parity)
                {
                    this->rx_state = UART_RX_STATE_PARITY;
                }
                else
                {
                    this->rx_state = UART_RX_STATE_WAIT_STOP;
                    this->rx_pending_bits = this->stop_bits;
                }
            }
            break;

        case UART_RX_STATE_PARITY:
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Checking parity (got: %d, expected: %d)\n",
                this->rx_parity, this->rx_current_bit);
            if (this->rx_current_bit != this->rx_parity)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Invalid parity\n");
            }

            this->rx_state = UART_RX_STATE_WAIT_STOP;
            this->rx_pending_bits = this->stop_bits;
        break;

        case UART_RX_STATE_WAIT_STOP:
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for stop bit\n");
            if (this->rx_current_bit == 1)
            {
                this->rx_pending_bits--;
                if (this->rx_pending_bits == 0)
                {
                    if (this->rx_ready_event)
                    {
                        this->rx_ready_event->exec();
                    }

                    this->rx_state = UART_RX_STATE_WAIT_START;
                }
            }
        break;

        default:
        break;
    }
}

void UartAdapter::reset(bool active)
{
    if (active)
    {
        this->rx_state = UART_RX_STATE_WAIT_START;
        this->tx_state = UART_TX_STATE_IDLE;
        if (this->tx_ready_event)
        {
            this->tx_ready_event->exec();
        }
    }
    else
    {
        this->tx_init_event.enqueue(1);
    }
}



void UartAdapter::tx_send_byte(uint8_t byte)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Pushing TX byte to queue (value: 0x%x)\n", byte);
    this->tx_pending_byte = byte;

    if (this->tx_state == UART_TX_STATE_IDLE)
    {
        this->tx_state = UART_TX_STATE_START;
        this->tx_send_bit_event.enqueue(this->period);
    }
}

void UartAdapter::tx_init_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapter *_this = (UartAdapter *)__this;
    _this->in.sync_full(1, 2, _this->ctrl_flow ? 1 : 0, _this->sync_mask);
}

void UartAdapter::tx_send_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapter *_this = (UartAdapter *)__this;
    int tx_bit;

    switch (_this->tx_state)
    {
        case UART_TX_STATE_START:
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending start bit\n");
            _this->tx_parity = 0;
            _this->tx_state = UART_TX_STATE_DATA;
            _this->tx_pending_bits = _this->data_bits;
            _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Popped TX byte (value: 0x%x)\n", _this->tx_pending_byte);
            tx_bit = 0;
            break;
        }

        case UART_TX_STATE_DATA:
        {
            if (_this->tx_pending_bits > 0)
            {
                tx_bit = _this->tx_pending_byte & 1;
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending data bit (bit: %d)\n", tx_bit);
                _this->tx_pending_byte >>= 1;
                _this->tx_pending_bits -= 1;
                _this->tx_parity ^= tx_bit;

                if (_this->tx_pending_bits == 0)
                {
                    if (_this->parity)
                    {
                        _this->tx_state = UART_TX_STATE_PARITY;
                    }
                    else
                    {
                        _this->tx_state = UART_TX_STATE_STOP;
                        _this->tx_pending_bits = _this->stop_bits;
                    }
                }
            }
            break;
        }

        case UART_TX_STATE_PARITY:
        {
            tx_bit = _this->tx_parity;
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending parity bit (bit: %d)\n", tx_bit);
            _this->tx_state = UART_TX_STATE_STOP;
            _this->tx_pending_bits = _this->stop_bits;
            break;
        }

        case UART_TX_STATE_STOP:
        {
            tx_bit = 1;
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending stop bit\n");
            _this->tx_pending_bits--;
            if (_this->tx_pending_bits == 0)
            {
                _this->tx_state = UART_TX_STATE_IDLE;
                if (_this->tx_ready_event)
                {
                    _this->tx_ready_event->exec();
                }
            }
            break;
        }

        default:
            break;
    }

    _this->tx_bit = tx_bit;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending bit (bit: %d)\n", tx_bit);
    _this->in.sync_full(_this->tx_bit, 2, _this->cts, _this->sync_mask);

    if (_this->tx_state != UART_TX_STATE_IDLE)
    {
        _this->tx_send_bit_event.enqueue(_this->period);
    }
}

void UartAdapter::rx_ready_event_set(vp::TimeEvent *event)
{
    this->rx_ready_event = event;
}

bool UartAdapter::tx_ready()
{
    return this->tx_state == UART_TX_STATE_IDLE;
}

bool UartAdapter::tx_flow_ctrl_ready()
{
    return !this->ctrl_flow || this->rts == 0;
}

uint8_t UartAdapter::rx_get()
{
    return this->rx_pending_byte >> (8 - this->data_bits);
}

void UartAdapter::tx_ready_event_set(vp::TimeEvent *event)
{
    this->tx_ready_event = event;
}

void UartAdapter::tx_flow_ctrl_event_set(vp::TimeEvent *event)
{
    this->tx_flow_ctrl_event = event;
}

void UartAdapter::baudrate_set(int baudrate)
{
    this->baudrate = baudrate;
    this->period = 1000000000000UL / this->baudrate;
}

void UartAdapter::data_bits_set(int data_bits)
{
    this->data_bits = data_bits;
}

void UartAdapter::stop_bits_set(int stop_bits)
{
    this->stop_bits = stop_bits;
}

void UartAdapter::parity_set(bool parity)
{
    this->parity = parity;
}

void UartAdapter::ctrl_flow_set(bool ctrl_flow)
{
    this->ctrl_flow = ctrl_flow;
    this->cts = this->ctrl_flow ? 1 : 0;
    this->sync_mask = ctrl_flow ? 0xF : 0x3;
}

void UartAdapter::loopback_set(bool enable)
{
    this->loopback = enable;
}
void UartAdapter::rx_flow_enable(bool enabled)
{
    if (this->ctrl_flow)
    {
        this->cts = !enabled;
        this->in.sync_full(this->tx_bit, 2, this->cts, this->sync_mask);
    }
}
