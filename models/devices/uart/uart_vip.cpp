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
#include "devices/utils/uart_adapter.hpp"

class UartVip : public vp::Component
{
public:

    UartVip(vp::ComponentConf &conf);

    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
        std::vector<std::string> args, std::string req) override;

private:

    void tx_edge(int64_t timestamp, int tx);
    void rx_edge(int64_t timestamp, int rx);

    void stop_sample_rx_bit();

    void tx_start_sampling(int baudrate);
    void stop_sample_tx_bit();

    static void clear_event_handler(vp::Block *__this, vp::TimeEvent *event);
    static void rx_event_handler(vp::Block *__this, vp::TimeEvent *event);
    static void tx_empty_event_handler(vp::Block *__this, vp::TimeEvent *event);

    void pending_flush_check();
    void pending_clear_check();

    vp::Trace trace;

    uint64_t baudrate;
    uint64_t period;
    int data_bits;
    int stop_bits;
    bool parity;
    bool ctrl_flow;

    UartAdapterBuffered adapter;

    FILE *rx_file = NULL;
    bool stdout;
    int64_t last_symbol_timestamp;

    gv::GvProxy *proxy;

    FILE *rx_proxy_file=NULL;
    FILE *pending_flush_proxy_file=NULL;
    int rx_req_id;

    int pending_flush_req_id = -1;

    FILE *pending_clear_proxy_file=NULL;
    int pending_clear_req_id = -1;
    int64_t pending_clear_period;
    vp::TimeEvent clear_event;
    vp::TimeEvent rx_event;
    vp::TimeEvent tx_empty_event;

    uint64_t received_bytes;
    int64_t rx_timestamp=-1;
};



UartVip::UartVip(vp::ComponentConf &config)
    : vp::Component(config), adapter(this, this, "adapter", "input"),
    clear_event(this, &UartVip::clear_event_handler),
    rx_event(this, &UartVip::rx_event_handler),
    tx_empty_event(this, &UartVip::tx_empty_event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    int baudrate = this->get_js_config()->get("baudrate")->get_int();

    int data_bits = this->get_js_config()->get("data_bits")->get_int();
    int stop_bits = this->get_js_config()->get("stop_bits")->get_int();
    bool parity = this->get_js_config()->get("parity")->get_bool();
    bool ctrl_flow = this->get_js_config()->get("ctrl_flow")->get_bool();
    int ctrl_flow_limiter = this->get_js_config()->get("ctrl_flow_limiter")->get_int();

    this->adapter.baudrate_set(baudrate);
    this->adapter.data_bits_set(data_bits);
    this->adapter.stop_bits_set(stop_bits);
    this->adapter.parity_set(parity);
    this->adapter.ctrl_flow_set(ctrl_flow);
    this->adapter.rx_fifo_size_set(ctrl_flow ? 1 : INT_MAX);
    this->adapter.rx_ready_event_set(&this->rx_event);
    this->adapter.tx_empty_event_set(&this->tx_empty_event);
    this->adapter.rx_flow_limiter_set(ctrl_flow_limiter);

    this->adapter.loopback_set(this->get_js_config()->get("loopback")->get_bool());
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

    this->adapter.tx_flow_ctrl_threshold_set(4);
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
                    this->ctrl_flow = value;
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
                this->adapter.baudrate_set(this->baudrate);
                this->adapter.data_bits_set(this->data_bits);
                this->adapter.stop_bits_set(this->stop_bits);
                this->adapter.parity_set(this->parity);
                this->adapter.ctrl_flow_set(this->ctrl_flow);
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
                this->adapter.tx_send_byte(byte);
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
        else if (args[1] == "clear")
        {
            int req = strtol(args[3].c_str(), NULL, 0);
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Received clear request\n");

            this->last_symbol_timestamp = this->time.get_time();
            this->pending_clear_proxy_file = reply_file;
            this->pending_clear_req_id = req;
            // We will check nothing was received every period equivalent to one symbol
            this->pending_clear_period = this->period * (this->data_bits + this->stop_bits + 2);

            this->pending_clear_check();
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
    if (this->pending_flush_req_id != -1 && this->adapter.tx_empty())
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Flush done\n");
        this->proxy->send_payload(this->pending_flush_proxy_file, std::to_string(this->pending_flush_req_id),
            NULL, 0);
        this->pending_flush_req_id = -1;
    }
}

void UartVip::pending_clear_check()
{
    if (this->time.get_time() - this->last_symbol_timestamp >= this->pending_clear_period)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Clear done\n");
        this->proxy->send_payload(this->pending_clear_proxy_file, std::to_string(this->pending_clear_req_id),
            NULL, 0);
        this->pending_clear_req_id = -1;

    }
    else
    {
        this->clear_event.enqueue(this->pending_clear_period);
    }
}


void UartVip::clear_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;
    _this->pending_clear_check();
}

void UartVip::rx_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;

    while (_this->adapter.rx_ready())
    {
        uint8_t byte = _this->adapter.rx_pop();

        if (_this->stdout)
        {
            std::cout << byte;
        }

        if (_this->rx_file != NULL)
        {
            fwrite((void *)&byte, 1, 1, _this->rx_file);
        }

        if (_this->rx_proxy_file)
        {
            _this->proxy->send_payload(_this->rx_proxy_file, std::to_string(_this->rx_req_id),
                &byte, 1);
        }
    }

    _this->last_symbol_timestamp = _this->time.get_time();
}

void UartVip::tx_empty_event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    UartVip *_this = (UartVip *)__this;
    _this->pending_flush_check();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new UartVip(config);
}
