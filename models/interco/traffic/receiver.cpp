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
#include "interco/traffic/receiver.hpp"

class Receiver : public vp::Component
{

public:
    Receiver(vp::ComponentConf &conf);

private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficReceiverConfig config);
    static void pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static void ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::Trace trace;

    vp::IoSlave input_itf;
    vp::WireSlave<TrafficReceiverConfig> control_itf;
    vp::ClockEvent pending_fsm_event;
    vp::ClockEvent ready_fsm_event;
    vp::Queue pending_reqs;
    vp::Queue ready_reqs;
    int64_t ready_timestamp = -1;
    int bandwidth;
    int max_pending_reqs = 4;
    vp::IoReq *stalled_req = NULL;
    vp::Signal<bool> stalled;
    vp::Signal<uint64_t> signal_req_addr;
};

Receiver::Receiver(vp::ComponentConf &config)
    : vp::Component(config),
    pending_fsm_event(this, Receiver::pending_fsm_handler),
    ready_fsm_event(this, Receiver::ready_fsm_handler),
    pending_reqs(this, "pending_reqs", &this->pending_fsm_event),
    ready_reqs(this, "ready_reqs", &this->ready_fsm_event),
    signal_req_addr(*this, "req_addr", 64),
    stalled(*this, "stalled", 1)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->input_itf.set_req_meth(&Receiver::req);
    this->new_slave_port("input", &this->input_itf);

    this->control_itf.set_sync_meth(&Receiver::control_sync);
    this->new_slave_port("control", &this->control_itf);
}

vp::IoReqStatus Receiver::req(vp::Block *__this, vp::IoReq *req)
{
    Receiver *_this = (Receiver *)__this;

    _this->signal_req_addr = req->get_addr();

    vp_assert_always(_this->stalled == false, &_this->trace, "Enqueueing request while being stalled\n");

    if (_this->pending_reqs.size() == _this->max_pending_reqs)
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, stalling (req: %p)\n", req);

        _this->stalled_req = req;
        _this->stalled = true;
        return vp::IO_REQ_DENIED;
    }
    else
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, queuing (req: %p)\n", req);

        _this->pending_reqs.push_back(req);
        return vp::IO_REQ_PENDING;
    }
}

void Receiver::control_sync(vp::Block *__this, TrafficReceiverConfig config)
{
    Receiver *_this = (Receiver *)__this;
    _this->bandwidth = config.bandwidth;
}

void Receiver::pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Receiver *_this = (Receiver *)__this;

    if (!_this->pending_reqs.empty() && _this->ready_timestamp <= _this->clock.get_cycles())
    {
        vp::IoReq *req = (vp::IoReq *)_this->pending_reqs.pop();

        int64_t cycles = (req->get_size() + _this->bandwidth - 1) / _this->bandwidth;
        _this->ready_timestamp = _this->clock.get_cycles() + cycles;

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Popping pending req (req: %p, ready cycle: %lld)\n", req, _this->ready_timestamp);

        _this->ready_reqs.push_back(req, cycles - 1);
    }

    _this->ready_reqs.trigger_next();
}

void Receiver::ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Receiver *_this = (Receiver *)__this;

    if (!_this->ready_reqs.empty())
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Popping ready req (req: %p)\n", req);

        vp::IoReq *req = (vp::IoReq *)_this->ready_reqs.pop();
        req->get_resp_port()->resp(req);
    }

    vp::IoReq *stalled_req = _this->stalled_req;
    if (stalled_req && _this->pending_reqs.size() < _this->max_pending_reqs)
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Unstalling (req: %p)\n", req);
        _this->pending_reqs.push_back(stalled_req);
        _this->stalled = false;
        _this->stalled_req = NULL;
        stalled_req->get_resp_port()->grant(stalled_req);
    }

    _this->pending_reqs.trigger_next();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Receiver(config);
}
