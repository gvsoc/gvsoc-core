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
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>
#include <vp/queue.hpp>
#include "interco/traffic/generator.hpp"

class Generator : public vp::Component
{

public:
    Generator(vp::ComponentConf &conf);

private:
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficGeneratorConfig *config);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void handle_req_end(vp::IoReq *req, int64_t latenty=0);

    vp::Trace trace;

    vp::IoMaster output_itf;
    vp::WireSlave<TrafficGeneratorConfig *> control_itf;
    vp::ClockEvent fsm_event;
    uint64_t address;
    vp::Signal<uint64_t> size;
    size_t packet_size;
    vp::Signal<uint64_t> pending_size;
    vp::ClockEvent *end_trigger;
    vp::Signal<bool> stalled;
    vp::Signal<bool> busy;
    vp::Signal<uint64_t> signal_req_addr;
    vp::Signal<uint64_t> signal_req_size;
    vp::Signal<bool> signal_req_is_write;
    int64_t last_req_cyclestamp = 0;
    bool do_write;

    int nb_pending_reqs = 64;
    vp::Queue free_reqs;
};

Generator::Generator(vp::ComponentConf &config)
    : vp::Component(config), fsm_event(this, Generator::fsm_handler),
    size(*this, "send_size", 64), pending_size(*this, "pending_size", 64),
    signal_req_addr(*this, "req_addr", 64), signal_req_size(*this, "req_size", 64),
    signal_req_is_write(*this, "req_is_write", 1), busy(*this, "busy", 1, true, false),
    stalled(*this, "stalled", 1), free_reqs(this, "free_reqs")
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->output_itf.set_grant_meth(&Generator::grant);
    this->output_itf.set_resp_meth(&Generator::response);
    this->new_master_port("output", &this->output_itf);

    this->control_itf.set_sync_meth(&Generator::control_sync);
    this->new_slave_port("control", &this->control_itf);
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
    _this->handle_req_end(req);
}

void Generator::control_sync(vp::Block *__this, TrafficGeneratorConfig *config)
{
    Generator *_this = (Generator *)__this;

    if (config->is_start)
    {
        _this->busy = true;
        _this->packet_size = config->packet_size;
        _this->size = config->size;
        _this->pending_size = config->size;
        _this->address = config->address;
        _this->end_trigger = config->end_trigger;
        _this->do_write = config->do_write;
        _this->stalled = false;

        for (int i=0; i<_this->nb_pending_reqs; i++)
        {
            uint8_t *data = new uint8_t[_this->packet_size];
            vp::IoReq *req = new vp::IoReq();
            req->set_data(data);

            _this->free_reqs.push_back(req);
        }

        _this->fsm_event.enqueue();
    }
    else
    {
        config->result = !_this->busy;
    }
}

void Generator::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Generator *_this = (Generator *)__this;

    if (!_this->stalled && _this->size > 0 && !_this->free_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)_this->free_reqs.pop();

        req->prepare();

        req->set_size(_this->packet_size);
        req->set_addr(_this->address);
        req->set_is_write(_this->do_write);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending request (req: %p, address: 0x%llx, size: 0x%llx, packet_size: 0x%llx)\n",
            req, _this->address, _this->packet_size, _this->packet_size);

        _this->signal_req_addr = req->get_addr();
        _this->signal_req_size = req->get_size();
        _this->signal_req_is_write = req->get_is_write();

        _this->address += _this->packet_size;

        vp::IoReqStatus status = _this->output_itf.req(req);
        _this->size -= _this->packet_size;

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
        _this->busy = false;

        for (int i=0; i<_this->nb_pending_reqs; i++)
        {
            vp::IoReq *req = (vp::IoReq *)_this->free_reqs.pop();
            delete[] req->get_data();
            delete req;
        }

        _this->end_trigger->enqueue();
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

    this->free_reqs.push_back(req, latency);
    this->last_req_cyclestamp = this->clock.get_cycles() + latency;

    this->fsm_event.enqueue();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Generator(config);
}
