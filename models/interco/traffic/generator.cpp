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

#include <cstring>
#include <stdexcept>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>
#include <vp/queue.hpp>
#include "interco/traffic/generator.hpp"

class Transfer
{
public:
    uint64_t address;
    uint64_t size;
    bool do_write;
    size_t packet_size;
    uint8_t *data;
};

class Generator : public vp::Component, TrafficGenerator
{

public:
    Generator(vp::ComponentConf &conf);
    ~Generator();

private:
    void start_transfer() override;
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficGeneratorConfig *config);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void handle_req_end(vp::IoReq *req, int64_t latenty=0);
    void handle_step();
    void handle_transfer();
    void handle_post_transfer();
    void handle_end();
    void close_transfer();

    vp::Trace trace;

    vp::IoMaster output_itf;
    vp::WireSlave<TrafficGeneratorConfig *> control_itf;
    vp::ClockEvent fsm_event;
    bool check;
    bool check_write;
    uint64_t address;
    uint8_t *data;
    vp::Signal<uint64_t> size;
    vp::Signal<uint64_t> pending_size;
    TrafficGeneratorSync *sync;
    vp::Signal<bool> stalled;
    vp::Signal<bool> busy;
    vp::Signal<uint64_t> signal_req_addr;
    vp::Signal<uint64_t> signal_req_size;
    vp::Signal<bool> signal_req_is_write;
    int64_t last_req_cyclestamp = 0;

    int nb_pending_reqs;
    vp::Queue free_reqs;
    std::queue<Transfer *> transfers;
    Transfer *current_transfer = NULL;
    uint8_t *ref_data;
    bool check_status;
    int64_t start_cycles;
    int64_t duration;
    int step;
    uint64_t config_size;
    uint64_t config_address;
    size_t packet_size;
};

Generator::Generator(vp::ComponentConf &config)
    : vp::Component(config), fsm_event(this, Generator::fsm_handler),
    size(*this, "send_size", 64), pending_size(*this, "pending_size", 64),
    signal_req_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
    signal_req_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
    signal_req_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ),
    busy(*this, "busy", 1, true, false),
    stalled(*this, "stalled", 1), free_reqs(this, "free_reqs")
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->output_itf.set_grant_meth(&Generator::grant);
    this->output_itf.set_resp_meth(&Generator::response);
    this->new_master_port("output", &this->output_itf);

    this->control_itf.set_sync_meth(&Generator::control_sync);
    this->new_slave_port("control", &this->control_itf);

    this->nb_pending_reqs = this->get_js_config()->get_int("nb_pending_reqs");
}

Generator::~Generator()
{
}

void Generator::start_transfer()
{
    this->handle_step();

    this->fsm_event.enqueue();
}

void Generator::grant(vp::Block *__this, vp::IoReq *req)
{
    Generator *_this = (Generator *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received grant (req: %p)\n", req);
    _this->stalled = false;
    _this->fsm_event.enqueue();
}

void Generator::response(vp::Block *__this, vp::IoReq *req)
{
    Generator *_this = (Generator *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received response (req: %p)\n", req);
    _this->handle_req_end(req, req->get_latency());
}

void Generator::control_sync(vp::Block *__this, TrafficGeneratorConfig *config)
{
    Generator *_this = (Generator *)__this;

    if (config->is_start)
    {
        config->sync->add_generator(_this);

        _this->check_status = false;
        _this->busy = true;
        _this->sync = config->sync;
        _this->stalled = false;
        _this->check = config->check;
        _this->check_write = config->do_write;
        _this->step = 0;
        _this->config_size = config->size;
        _this->config_address = config->address;
        _this->packet_size = config->packet_size;

        for (int i=0; i<_this->nb_pending_reqs; i++)
        {
            vp::IoReq *req = new vp::IoReq();
            _this->free_reqs.push_back(req);
        }

        if (config->check)
        {
            _this->ref_data = new uint8_t[config->size];

            for (int i=0; i<config->size; i++)
            {
                _this->ref_data[i] = std::rand() % 256;
            }
        }
    }
    else
    {
        config->result = !_this->busy;
        config->check_status = _this->check_status;
        config->duration = _this->duration;
    }
}

void Generator::handle_step()
{
    switch (this->step)
    {
        case 0:
        {
            if (this->check && !this->check_write)
            {
                uint8_t *data = new uint8_t[this->config_size];
                memcpy(data, this->ref_data, this->config_size);
                this->transfers.push(new Transfer(
                    {this->config_address, this->config_size, !this->check_write, this->packet_size,
                       data}));
                this->fsm_event.enqueue();
                this->step++;
            }
            else
            {
                this->step++;
                this->handle_step();
            }

            break;
        }

        case 1:
        {
            this->sync->nb_pre_check_done++;
            if (this->sync->nb_pre_check_done == this->sync->generators.size())
            {
                for (TrafficGenerator *generator: this->sync->generators)
                {
                    ((Generator *)generator)->handle_transfer();
                }
            }
            this->step++;
            break;
        }

        case 2:
        {
            this->sync->nb_transfers_done++;
            if (this->sync->nb_transfers_done == this->sync->generators.size())
            {
                for (TrafficGenerator *generator: this->sync->generators)
                {
                    ((Generator *)generator)->handle_post_transfer();
                }
            }
            this->step++;
            break;
        }

        case 3:
        {
            this->sync->nb_post_check_done++;
            if (this->sync->nb_post_check_done == this->sync->generators.size())
            {
                for (TrafficGenerator *generator: this->sync->generators)
                {
                    ((Generator *)generator)->handle_end();
                }

                this->sync->event->enqueue();
            }

            this->step++;
            break;
        }
    }
}

void Generator::handle_transfer()
{
    this->start_cycles = this->clock.get_cycles();

    uint8_t *data = new uint8_t[this->config_size];

    if (this->check && this->check_write)
    {
        memcpy(data, this->ref_data, this->config_size);
    }

    this->close_transfer();

    this->transfers.push(new Transfer(
        {this->config_address, this->config_size, this->check_write, this->packet_size, data}));
    this->fsm_event.enqueue();

}

void Generator::handle_post_transfer()
{
    this->duration = this->clock.get_cycles() - this->start_cycles;

    if (this->check && this->check_write)
    {
        this->close_transfer();

        uint8_t *data = new uint8_t[this->config_size];
        this->transfers.push(new Transfer(
            {this->config_address, this->config_size, !this->check_write, this->packet_size, data}));
        this->fsm_event.enqueue();
    }

}

void Generator::handle_end()
{
    if (this->check)
    {
        this->check_status |= std::memcmp(this->ref_data, this->current_transfer->data,
            this->current_transfer->size) != 0;
        // for (int i=0; i<_this->current_transfer->size; i++)
        // {
        //     printf("%d: %d %d\n", i, _this->ref_data[i], _this->current_transfer->data[i]);
        // }
    }

    for (int i=0; i<this->nb_pending_reqs; i++)
    {
        vp::IoReq *req = (vp::IoReq *)this->free_reqs.pop();
        delete req;
    }

    this->close_transfer();
    this->busy = false;
}

void Generator::close_transfer()
{
    if (this->current_transfer)
    {
        delete[] this->current_transfer->data;
        delete this->current_transfer;
        this->current_transfer = NULL;
    }
}

void Generator::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Generator *_this = (Generator *)__this;

    if (!_this->current_transfer && _this->transfers.size() > 0)
    {
        _this->current_transfer = _this->transfers.front();
        _this->transfers.pop();
        _this->size = _this->current_transfer->size;
        _this->pending_size = _this->current_transfer->size;
        _this->address = _this->current_transfer->address;
        _this->data = _this->current_transfer->data;
    }

    if (!_this->stalled && _this->size > 0 && !_this->free_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)_this->free_reqs.pop();

        req->prepare();

        for (int i=0; i<4; i++)
        {
            req->arg_push((void *)(0x0123456789ABCDEF + i));
        }

        req->set_size(_this->current_transfer->packet_size);
        req->set_addr(_this->address);
        req->set_data(_this->data);
        req->set_is_write(_this->current_transfer->do_write);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending request (req: %p, address: 0x%llx, size: 0x%llx, packet_size: 0x%llx)\n",
            req, _this->address, _this->current_transfer->packet_size, _this->current_transfer->packet_size);

        _this->signal_req_addr.set_and_release(req->get_addr());
        _this->signal_req_size.set_and_release(req->get_size());
        _this->signal_req_is_write.set_and_release(req->get_is_write());

        _this->address += _this->current_transfer->packet_size;
        _this->data += _this->current_transfer->packet_size;

        vp::IoReqStatus status = _this->output_itf.req(req);
        _this->size -= _this->current_transfer->packet_size;

        if (status == vp::IO_REQ_DENIED)
        {
            _this->stalled = true;
        }
        else
        {
            if (status == vp::IO_REQ_PENDING)
            {
                _this->fsm_event.enqueue();
            }
            else
            {
                _this->handle_req_end(req, req->get_latency()-1);
            }
        }
    }

    if (_this->pending_size == 0 && _this->free_reqs.size() == _this->nb_pending_reqs &&
            _this->last_req_cyclestamp <= _this->clock.get_cycles())
    {
        _this->handle_step();
    }

    if (_this->busy)
    {
        _this->fsm_event.enqueue();
    }
}

void Generator::handle_req_end(vp::IoReq *req, int64_t latency)
{
    this->pending_size -= req->get_size();
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Handling req end (req: %p, size: 0x%x, pending_size: 0x%x, latency: %ld)\n",
        req, req->get_size(), this->pending_size.get(), latency);

    for (int i=3; i>=0; i--)
    {
        uint64_t value = (uint64_t)req->arg_pop();
        this->check_status |= value != 0x0123456789ABCDEF + i;

    }

    this->free_reqs.push_back(req, latency);
    this->last_req_cyclestamp = this->clock.get_cycles() + latency;

    this->fsm_event.enqueue();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Generator(config);
}
