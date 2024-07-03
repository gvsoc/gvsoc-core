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

#include <queue>
#include <vp/vp.hpp>
#include <vp/itf/uart.hpp>
#include <vp/proxy.hpp>

typedef enum
{
    UART_RX_STATE_WAIT_START,
    UART_RX_STATE_DATA,
    UART_RX_STATE_PARITY,
    UART_RX_STATE_WAIT_STOP
} uart_rx_state_e;

typedef enum
{
    UART_TX_STATE_IDLE,
    UART_TX_STATE_START,
    UART_TX_STATE_DATA,
    UART_TX_STATE_PARITY,
    UART_TX_STATE_STOP
} uart_tx_state_e;


class UartVip : public vp::Component
{
public:
    UartVip(vp::ComponentConf &conf);

    void reset(bool active) override;
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
        std::vector<std::string> args, std::string req) override;

private:

    void tx_edge(int64_t timestamp, int tx);
    void rx_edge(int64_t timestamp, int rx);

    void sample_rx_bit();

    void rx_start_sampling(int baudrate);
    void stop_sample_rx_bit();

    void tx_start_sampling(int baudrate);
    void stop_sample_tx_bit();
    void send_byte(uint8_t byte);

    static void sync(vp::Block *__this, int data);

    static void rx_sample_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_send_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_init_handler(vp::Block *__this, vp::TimeEvent *event);

    void pending_flush_check();

    vp::Trace trace;

    vp::UartSlave in;

    uint64_t baudrate;
    uint64_t period;
    bool loopback;
    FILE *rx_file = NULL;
    bool stdout;
    int data_bits;
    int stop_bits;
    bool parity;

    gv::GvProxy *proxy;

    vp::TimeEvent sample_rx_bit_event;
    uart_rx_state_e rx_state;
    int rx_current_bit;
    int rx_pending_bits;
    uint8_t rx_pending_byte;
    int rx_parity;
    FILE *rx_proxy_file=NULL;
    FILE *pending_flush_proxy_file=NULL;
    int rx_req_id;

    vp::TimeEvent tx_send_bit_event;
    vp::TimeEvent tx_init_event;
    std::queue<uint8_t> tx_queue;
    uart_tx_state_e tx_state;
    int tx_parity;
    int tx_pending_bits;
    uint8_t tx_pending_byte;
    int tx_bit;

    int pending_flush_req_id = -1;
};



UartVip::UartVip(vp::ComponentConf &config)
    : vp::Component(config),
    sample_rx_bit_event(this, &UartVip::rx_sample_handler),
    tx_send_bit_event(this, &UartVip::tx_send_handler),
    tx_init_event(this, &UartVip::tx_init_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->in.set_sync_meth(&UartVip::sync);
    this->new_slave_port("input", &this->in);

    this->baudrate = this->get_js_config()->get("baudrate")->get_int();
    this->period = 1000000000000UL / this->baudrate;

    this->data_bits = this->get_js_config()->get("data_bits")->get_int();
    this->stop_bits = this->get_js_config()->get("stop_bits")->get_int();
    this->parity = this->get_js_config()->get("parity")->get_bool();

    this->loopback = this->get_js_config()->get("loopback")->get_bool();
    this->stdout = this->get_js_config()->get("stdout")->get_bool();

    std::string rx_filename = this->get_js_config()->get("rx_file")->get_str();
    if (rx_filename != "")
    {
        this->rx_file = fopen(rx_filename.c_str(), (char *)"w");
        if (this->rx_file == NULL)
        {
            this->trace.fatal("Unable to open TX log file: %s", strerror(errno));
        }
    }
}



void UartVip::reset(bool active)
{
    if (active)
    {
        this->rx_state = UART_RX_STATE_WAIT_START;
        this->tx_state = UART_TX_STATE_IDLE;
    }
}



std::string UartVip::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
    std::vector<std::string> args, std::string req)
{
    this->proxy = proxy;

    if (args[0] == "uart")
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Received commands (command: %s)\n", args[1].c_str());
        if (args[1] == "setup")
        {
            std::vector<std::string> params = {args.begin() + 2, args.end()};

            bool enabled = false;

            for (std::string x: params)
            {
                int pos = x.find_first_of("=");
                std::string name = x.substr(0, pos);
                std::string value_str = x.substr(pos + 1);
                int value = strtol(value_str.c_str(), NULL, 0);

                if (name == "itf")
                {
                }
                else if (name == "enabled")
                {
                    enabled = value;
                }
                else if (name == "word_size")
                {
                    this->data_bits = value;
                }
                else if (name == "baudrate")
                {
                    this->baudrate = value;
                    this->period = 1000000000000UL / this->baudrate;
                }
                else if (name == "stop_bits")
                {
                    this->stop_bits = value;
                }
                else if (name == "parity_mode")
                {
                    this->parity = value;
                }
                else if (name == "ctrl_flow")
                {
                }
                else if (name == "usart_polarity")
                {
                }
                else if (name == "is_usart")
                {
                }
                else if (name == "usart_phase")
                {
                }
                else
                {
                    return "err=1;msg=invalid option: " + name;
                }
            }

            if (enabled)
            {
                this->tx_init_event.enqueue(this->period);
            }
        }
        else if (args[1] == "tx")
        {
            int itf = strtol(args[2].c_str(), NULL, 0);
            int size = strtol(args[3].c_str(), NULL, 0);
            for (int i=0; i<size; i++)
            {
                uint8_t byte;
                int read_size = fread(&byte, 1, 1, req_file);
                this->send_byte(byte);
            }
        }
        else if (args[1] == "rx")
        {
            int itf = strtol(args[2].c_str(), NULL, 0);
            int enabled = strtol(args[3].c_str(), NULL, 0);

            if (enabled)
            {
                int req = strtol(args[4].c_str(), NULL, 0);
                this->rx_proxy_file = reply_file;
                this->rx_req_id = req;
            }
            else
            {
                this->rx_proxy_file = NULL;
            }
        }
        else if (args[1] == "flush")
        {
            int req = strtol(args[3].c_str(), NULL, 0);
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Received flush request\n");

            this->pending_flush_proxy_file = reply_file;
            this->pending_flush_req_id = req;

            this->pending_flush_check();
        }
        return "err=0";
    }
    else
    {
        return "err=1;msg=unsupported command";
    }
}

void UartVip::pending_flush_check()
{
    if (this->pending_flush_req_id != -1 && this->tx_queue.empty())
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Flush done\n");
        this->proxy->send_payload(this->pending_flush_proxy_file, std::to_string(this->pending_flush_req_id),
            NULL, 0);
        this->pending_flush_req_id = -1;
    }
}

void UartVip::sync(vp::Block *__this, int data)
{
    UartVip *_this = (UartVip *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sync (value: %d, loopback: %d)\n", data, _this->loopback);

    if (_this->loopback)
    {
        _this->in.sync_full(data, 2, 2, 0x0);
    }

    _this->rx_current_bit = data;

    if (_this->rx_state == UART_RX_STATE_WAIT_START && data == 0)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received start bit\n");

        _this->rx_start_sampling(_this->baudrate);
    }
}



void UartVip::rx_start_sampling(int baudrate)
{
    this->rx_state = UART_RX_STATE_DATA;
    this->rx_pending_bits = this->data_bits;
    this->rx_parity = 0;

    // Wait 1 period and a half to start sampling in the middle of the next bit

    this->sample_rx_bit_event.enqueue(3*this->period/2);
}



void UartVip::rx_sample_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;

    _this->sample_rx_bit();

    if (_this->rx_state != UART_RX_STATE_WAIT_START)
    {
        _this->sample_rx_bit_event.enqueue(_this->period);
    }
}



void UartVip::sample_rx_bit()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampling bit (value: %d)\n", rx_current_bit);

    switch (this->rx_state)
    {
        case UART_RX_STATE_DATA:
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Received data bit (data: %d)\n", rx_current_bit);

            this->rx_pending_byte = (this->rx_pending_byte >> 1)
                | (this->rx_current_bit << (this->data_bits - 1));

            this->rx_parity ^= this->rx_current_bit;

            this->rx_pending_bits--;
            if (this->rx_pending_bits == 0)
            {
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampled RX byte (value: 0x%x)\n", this->rx_pending_byte);
                if (this->stdout)
                {
                    std::cout << this->rx_pending_byte;
                }

                if (this->rx_file != NULL)
                {
                    fwrite((void *)&this->rx_pending_byte, 1, 1, rx_file);
                }

                if (this->rx_proxy_file)
                {
                    this->proxy->send_payload(this->rx_proxy_file, std::to_string(this->rx_req_id),
                        &this->rx_pending_byte, 1);
                }

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
                    this->rx_state = UART_RX_STATE_WAIT_START;
                }
            }
        break;

        default:
        break;
    }
}

void UartVip::send_byte(uint8_t byte)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Pushing TX byte to queue (value: 0x%x)\n", byte);
    this->tx_queue.push(byte);

    if (this->tx_state == UART_TX_STATE_IDLE)
    {
        this->tx_state = UART_TX_STATE_START;
        this->tx_send_bit_event.enqueue(this->period);
    }
}

void UartVip::tx_init_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;
    _this->in.sync_full(1, 2, 1, 0xf);
}

void UartVip::tx_send_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;
    int tx_bit;

    switch (_this->tx_state)
    {
        case UART_TX_STATE_START:
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending start bit\n");
            _this->tx_parity = 0;
            _this->tx_state = UART_TX_STATE_DATA;
            _this->tx_pending_bits = _this->data_bits;
            _this->tx_pending_byte = _this->tx_queue.front();
            _this->tx_queue.pop();
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
                if (_this->tx_queue.empty())
                {
                    _this->tx_state = UART_TX_STATE_IDLE;
                    _this->pending_flush_check();
                }
                else
                {
                    _this->tx_state = UART_TX_STATE_START;
                }
            }
            break;
        }

        default:
            break;
    }

    _this->tx_bit = tx_bit;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending bit (bit: %d)\n", tx_bit);
    _this->in.sync_full(_this->tx_bit, 2, 1, 0xf);

    if (_this->tx_state != UART_TX_STATE_IDLE)
    {
        _this->tx_send_bit_event.enqueue(_this->period);
    }
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new UartVip(config);
}
