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

 UartAdapterBuffered::UartAdapterBuffered(vp::Component *top, vp::Block *parent, std::string name, std::string itf_name)
    : vp::Block(parent, name), adapter(top, this, name + "adapter", itf_name),
    rx_event(this, &UartAdapterBuffered::rx_event_handler),
    tx_ready_event(this, &UartAdapterBuffered::tx_ready_event_handler),
    tx_flow_ctrl_event(this, &UartAdapterBuffered::tx_flow_ctrl_event_handler),
    limiter_event(this, &UartAdapterBuffered::rx_limiter_event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->tx_flow_ctrl_threshold = 0;
    this->adapter.rx_ready_event_set(&this->rx_event);
    this->adapter.tx_ready_event_set(&this->tx_ready_event);
    this->adapter.tx_flow_ctrl_event_set(&this->tx_flow_ctrl_event);
}
//                     _this->adapter.rx_flow_enable(false);

void UartAdapterBuffered::tx_send_byte()
{
    if (this->tx_queue.size() > 0 && this->tx_can_send())
    {
        uint8_t byte = this->tx_queue.front();
        this->tx_queue.pop();
        if (this->tx_flow_ctrl_threshold_count > 0)
        {
            this->tx_flow_ctrl_threshold_count--;
        }
        this->adapter.tx_send_byte(byte);
    }
}

void UartAdapterBuffered::rx_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapterBuffered *_this = (UartAdapterBuffered *)__this;

    if (_this->rx_queue.size() >= _this->rx_max_pending_bytes)
    {
        _this->trace.force_warning("Trying to push received byte while max number of received has been reached\n");
        return;
    }
    else
    {
        _this->rx_queue.push(_this->adapter.rx_get());
    }

    if (_this->rx_event_user)
    {
        _this->rx_event_user->exec();
    }

    // if (_this->ctrl_flow && _this->ctrl_flow_limiter)
    //         {
    //             if (_this->rx_timestamp == -1)
    //             {
    //                 _this->rx_timestamp = _this->time.get_time();
    //             }
    //             else
    //             {
    //                 _this->received_bytes++;

    //                 if ((double)_this->received_bytes  / (_this->time.get_time() - _this->rx_timestamp) * 1000000000000ULL > _this->ctrl_flow_limiter)
    //                 {
    //                     _this->adapter.rx_flow_enable(false);
    //                     _this->limiter_event.enqueue(
    //                         (double)_this->received_bytes * 1000000000000ULL / _this->ctrl_flow_limiter +
    //                             _this->rx_timestamp - _this->time.get_time()
    //                     );
    //                 }
    //             }
    //         }
}

void UartAdapterBuffered::tx_ready_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapterBuffered *_this = (UartAdapterBuffered *)__this;

    if (_this->tx_queue.size() == 0)
    {
        if (_this->tx_empty_event_user)
        {
            _this->tx_empty_event_user->exec();
        }
    }
    else
    {
        _this->tx_send_byte();
    }
}

void UartAdapterBuffered::tx_flow_ctrl_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapterBuffered *_this = (UartAdapterBuffered *)__this;

    if (_this->adapter.tx_flow_ctrl_ready())
    {
        _this->tx_flow_ctrl_threshold_count = 0;
    }
    else
    {
        if (_this->tx_flow_ctrl_threshold != 0)
        {
            _this->tx_flow_ctrl_threshold_count = std::min((int)_this->tx_queue.size(), _this->tx_flow_ctrl_threshold);
        }
    }
}

void UartAdapterBuffered::tx_flow_ctrl_threshold_set(uint8_t byte)
{
    this->tx_flow_ctrl_threshold = byte;
}

void UartAdapterBuffered::reset(bool active)
{
    if (active)
    {
        this->tx_flow_ctrl_threshold_count = 0;
    }
}

void UartAdapterBuffered::baudrate_set(int baudrate)
{
    this->adapter.baudrate_set(baudrate);
}

void UartAdapterBuffered::data_bits_set(int data_bits)
{
    this->adapter.data_bits_set(data_bits);
}

void UartAdapterBuffered::stop_bits_set(int stop_bits)
{
    this->adapter.stop_bits_set(stop_bits);
}

void UartAdapterBuffered::parity_set(bool parity)
{
    this->adapter.parity_set(parity);
}

void UartAdapterBuffered::ctrl_flow_set(bool ctrl_flow)
{
    this->adapter.ctrl_flow_set(ctrl_flow);
    this->limiter_event.enqueue(1);
}

void UartAdapterBuffered::loopback_set(bool enabled)
{
    this->adapter.loopback_set(enabled);
}

void UartAdapterBuffered::rx_ready_event_set(vp::TimeEvent *event)
{
    this->rx_event_user = event;
}

bool UartAdapterBuffered::rx_ready()
{
    return !this->rx_queue.empty();
}

uint8_t UartAdapterBuffered::rx_pop()
{
    uint8_t value = this->rx_queue.front();
    this->rx_queue.pop();
    return value;
}

void UartAdapterBuffered::rx_flow_limiter_set(double bandwidth)
{
    this->ctrl_flow_limiter = bandwidth;
}

void UartAdapterBuffered::rx_limiter_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartAdapterBuffered *_this = (UartAdapterBuffered *)__this;
    _this->adapter.rx_flow_enable(true);
}

void UartAdapterBuffered::tx_empty_event_set(vp::TimeEvent *event)
{
    this->tx_empty_event_user = event;
}

void UartAdapterBuffered::tx_send_byte(uint8_t byte)
{
    if (this->tx_can_send())
    {
        if (this->tx_flow_ctrl_threshold_count > 0)
        {
            this->tx_flow_ctrl_threshold_count--;
        }
        this->adapter.tx_send_byte(byte);
    }
    else
    {
        this->tx_queue.push(byte);
    }
}

bool UartAdapterBuffered::tx_empty()
{
    return this->adapter.tx_ready() && this->tx_queue.size() == 0;
}

bool UartAdapterBuffered::tx_ready()
{
    return this->adapter.tx_ready() && this->adapter.tx_flow_ctrl_ready() && this->tx_queue.size() == 0;
}

bool UartAdapterBuffered::tx_can_send()
{
    return this->adapter.tx_ready() && (this->adapter.tx_flow_ctrl_ready() || this->tx_flow_ctrl_threshold_count > 0);
}

void UartAdapterBuffered::rx_fifo_size_set(int bytes)
{
    this->rx_max_pending_bytes = bytes;
}
