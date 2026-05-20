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
 * v2 traffic receiver. Mirrors interco/traffic/receiver.cpp but speaks io_v2
 * on the input port: returns IO_REQ_DENIED when full and calls input_itf.retry()
 * once when capacity returns; never tracks denied requests internally (the
 * master holds and resends them per the v2 protocol).
 */

#include <queue>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io_v2.hpp>
#include "interco/traffic/receiver.hpp"

class ReceiverV2 : public vp::Component
{
public:
    ReceiverV2(vp::ComponentConf &conf);
    ~ReceiverV2();

private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficReceiverConfig config);
    static void pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static void ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void log_access(uint64_t addr, uint64_t size, bool is_write);

    vp::Trace trace;

    vp::IoSlave input_itf;
    vp::WireSlave<TrafficReceiverConfig> control_itf;
    vp::ClockEvent pending_fsm_event;
    vp::ClockEvent ready_fsm_event;
    vp::Queue pending_reqs;
    vp::Queue ready_reqs;
    int64_t ready_timestamp = -1;
    int bandwidth = 0;
    int max_pending_reqs = 4;
    vp::Signal<bool> stalled;
    // v2 deny/retry: true when a master was denied and is owed a retry() once
    // capacity returns.
    bool owes_retry = false;
    vp::Signal<uint64_t> log_addr;
    vp::Signal<uint64_t> log_size;
    vp::Signal<bool> log_is_write;
    int64_t last_logged_access = -1;
    int nb_logged_access_in_same_cycle = 0;
    int size;
    uint8_t *mem_data;
};

ReceiverV2::ReceiverV2(vp::ComponentConf &config)
    : vp::Component(config),
      input_itf(&ReceiverV2::req),
      pending_fsm_event(this, ReceiverV2::pending_fsm_handler),
      ready_fsm_event(this, ReceiverV2::ready_fsm_handler),
      pending_reqs(this, "pending_reqs", &this->pending_fsm_event),
      ready_reqs(this, "ready_reqs", &this->ready_fsm_event),
      log_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
      log_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
      log_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      stalled(*this, "stalled", 1)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input", &this->input_itf);

    this->control_itf.set_sync_meth(&ReceiverV2::control_sync);
    this->new_slave_port("control", &this->control_itf);

    this->size = this->get_js_config()->get("mem_size")->get_int();
    if (this->size > 0)
    {
        this->mem_data = (uint8_t *)calloc(this->size, 1);
        if (this->mem_data == NULL) throw std::bad_alloc();
    }
}

ReceiverV2::~ReceiverV2()
{
    if (this->size > 0)
    {
        free(this->mem_data);
    }
}

vp::IoReqStatus ReceiverV2::req(vp::Block *__this, vp::IoReq *req)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;

    if (_this->size != 0)
    {
        if (req->get_addr() + req->get_size() > _this->size)
        {
            _this->trace.fatal("Invalid access (addr: 0x%x, size: 0x%x, mem_size: 0x%x)\n",
                req->get_addr(), req->get_size(), _this->size);
        }

        if (req->get_is_write())
        {
            memcpy(&_this->mem_data[req->get_addr()], req->get_data(), req->get_size());
        }
        else
        {
            memcpy(req->get_data(), &_this->mem_data[req->get_addr()], req->get_size());
        }
    }

    _this->log_access(req->get_addr(), req->get_size(), req->get_is_write());

    if (_this->pending_reqs.size() == _this->max_pending_reqs)
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, full — denying (req: %p)\n", req);
        _this->stalled = true;
        _this->owes_retry = true;
        return vp::IO_REQ_DENIED;
    }
    else
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, queuing (req: %p)\n", req);
        _this->pending_reqs.push_back(req);
        return vp::IO_REQ_GRANTED;
    }
}

void ReceiverV2::log_access(uint64_t addr, uint64_t size, bool is_write)
{
    int64_t cycles = this->clock.get_cycles();

    if (cycles > this->last_logged_access)
    {
        this->nb_logged_access_in_same_cycle = 0;
    }

    int64_t delay = 0;
    if (this->nb_logged_access_in_same_cycle > 0)
    {
        int64_t period = this->clock.get_period();
        delay = period - (period >> this->nb_logged_access_in_same_cycle);
    }
    this->log_addr.set_and_release(addr, 0, delay);
    this->log_size.set_and_release(size, 0, delay);
    this->log_is_write.set_and_release(is_write, 0, delay);
    this->nb_logged_access_in_same_cycle++;
    this->last_logged_access = cycles;
}

void ReceiverV2::control_sync(vp::Block *__this, TrafficReceiverConfig config)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Start (bw: %d)\n", config.bandwidth);
    _this->bandwidth = config.bandwidth;
}

void ReceiverV2::pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;

    if (!_this->pending_reqs.empty() && _this->ready_timestamp <= _this->clock.get_cycles())
    {
        vp::IoReq *req = (vp::IoReq *)_this->pending_reqs.pop();

        if (_this->bandwidth > 0)
        {
            int64_t cycles = (req->get_size() + _this->bandwidth - 1) / _this->bandwidth;
            _this->ready_timestamp = _this->clock.get_cycles() + cycles;
            _this->ready_reqs.push_back(req, cycles - 1);
        }
        else
        {
            _this->ready_reqs.push_back(req, 0);
        }
    }

    _this->pending_reqs.trigger_next();
    _this->ready_reqs.trigger_next();
}

void ReceiverV2::ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;

    if (!_this->ready_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)_this->ready_reqs.pop();
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Popping ready req (req: %p)\n", req);
        req->set_resp_status(vp::IO_RESP_OK);
        _this->input_itf.resp(req);
    }

    // v2 deny/retry: when we have capacity again, signal retry() once. The
    // master that was denied is responsible for re-sending its request.
    if (_this->owes_retry && _this->pending_reqs.size() < _this->max_pending_reqs)
    {
        _this->owes_retry = false;
        _this->stalled = false;
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Capacity returned, calling retry()\n");
        _this->input_itf.retry();
    }

    _this->pending_reqs.trigger_next();
    _this->ready_reqs.trigger_next();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ReceiverV2(config);
}
