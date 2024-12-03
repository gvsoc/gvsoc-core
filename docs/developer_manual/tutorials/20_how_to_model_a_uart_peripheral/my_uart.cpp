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

#include "vp/vp.hpp"
#include "vp/itf/uart.hpp"
#include "utils/uart/uart_adapter.hpp"

class MyUart : public vp::Component
{
public:

    MyUart(vp::ComponentConf &conf);

    // This can be defined to get called when the reset is changing so that we can properly
    // reset our internal state
    void reset(bool active) override;

private:

    // The chip can send us these commands on the RX channel
    typedef enum
    {
        UART_RX_CMD_PRINT=1,
        UART_RX_CMD_LOOPBACK=2,
    } uart_rx_cmd_e;

    // Internal FSM state. First wait for a command, then data length and then the data.
    typedef enum
    {
        UART_RX_WAITING_CMD,
        UART_RX_WAITING_LEN,
        UART_RX_WAITING_DATA,
    } uart_rx_state_e;

    // This gets called when the UART adapter receives a byte
    static void rx_event_handler(vp::Block *__this, vp::TimeEvent *event);

    // Handle a print commad
    void handle_cmd_print();

    // Handle a loopback command
    void handle_cmd_loopback();

    // TO dump debug messages about this component activity
    vp::Trace trace;
    // UART baudrate retrieved from component config
    uint64_t baudrate;
    // UART adapter taking care of low-level UART protocol.
    // We get the buffered one to get an infinite FIFO
    UartAdapterBuffered adapter;
    // Events used to register to the UART adapter the callback for receving bytes
    vp::TimeEvent rx_event;
    // Our internal state to sample commands and handle them
    uart_rx_state_e rx_state;
    // Current command
    uint8_t command;
    // Current number of data bytes which remains to be received
    int pending_data_len;
    // The data currently received
    std::queue<uint8_t> rx_data;
};



MyUart::MyUart(vp::ComponentConf &config)
    : vp::Component(config),
    adapter(this, this, "adapter", "uart"),
    rx_event(this, &MyUart::rx_event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    // The baudrate is retrieved from the component configuration, which can be modified from
    // the command line. Propagate it to the UART adapter
    int baudrate = this->get_js_config()->get("baudrate")->get_int();
    this->adapter.baudrate_set(baudrate);

    // Register to the UART adapter our callback to receive bytes
    this->adapter.rx_ready_event_set(&this->rx_event);
}

void MyUart::reset(bool active)
{
    if (active)
    {
        // In case of a reset we need to clear any on-going activity to be able to
        // receive again a command.
        // Note that the adapter is automically reset.
        this->rx_state = UART_RX_WAITING_CMD;
        while (this->rx_data.size() > 0)
        {
            this->rx_data.pop();
        }
    }
}

void MyUart::rx_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    MyUart *_this = (MyUart *)__this;

    // We get here when the UART adapter receives a byte. Since the callback means at least
    // once byte was received, we need to pop all of them
    while (_this->adapter.rx_ready())
    {
        uint8_t byte = _this->adapter.rx_pop();

        // Check our internal state to see what we do with the byte
        switch (_this->rx_state)
        {
            case UART_RX_WAITING_CMD:
                // Store the command and go on with the data length
                _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received command (cmd: 0x%x)\n", byte);
                _this->command = byte;
                _this->rx_state = UART_RX_WAITING_LEN;
                break;

            case UART_RX_WAITING_LEN:
            // Store the data length and go on with the command data
            _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received data length (len: 0x%x)\n", byte);
                _this->pending_data_len = byte;
                _this->rx_state = UART_RX_WAITING_DATA;
                break;

            case UART_RX_WAITING_DATA:
                // Enqueue the data byte until the length is 0
                _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received data byte (byte: 0x%x)\n", byte);
                _this->rx_data.push(byte);
                _this->pending_data_len--;
                if (_this->pending_data_len == 0)
                {
                    // In case we are done, execute the command and go back to initial state
                    // to process the next command.
                    switch (_this->command)
                    {
                        case UART_RX_CMD_PRINT:
                            _this->trace.msg(vp::Trace::LEVEL_INFO, "Handling command print\n");
                            _this->handle_cmd_print();
                            break;
                        case UART_RX_CMD_LOOPBACK:
                            _this->trace.msg(vp::Trace::LEVEL_INFO, "Handling command loopback\n");
                            _this->handle_cmd_loopback();
                            break;
                    }

                    _this->rx_state = UART_RX_WAITING_CMD;
                }
                break;
        }
    }
}

void MyUart::handle_cmd_print()
{
    while(this->rx_data.size() > 0)
    {
        printf("%c", this->rx_data.front());
        this->rx_data.pop();
    }
}

void MyUart::handle_cmd_loopback()
{
    // The loopback command needs to forward all data bytes to the TX line.
    // Since we are using the buffered adapter with an infinite FIFO, we can directly push all
    // of them in the same cycle.
    while(this->rx_data.size() > 0)
    {
        this->adapter.tx_send_byte(this->rx_data.front());
        this->rx_data.pop();
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyUart(config);
}
