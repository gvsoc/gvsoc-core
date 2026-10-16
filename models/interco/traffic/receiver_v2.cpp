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
 *
 * Per-burst write-ack contract note: this slave keeps the classic per-request
 * round-trip (it resp()s the master's own request, writes included) and does
 * NOT implement the beat-plane per-burst write acknowledgement. That is
 * correct because its input port is declared IoV2SingleReq (receiver_v2.py):
 * when a beat master binds it (e.g. the FlooNoC NI target output, IoV2Beat,
 * in the floonoc tests), the framework auto-inserts the beat-to-single-req
 * converter in between, so raw write beats never reach this component — it
 * always sits on the round-trip plane.
 *
 * Reads and writes are two independent channels, like the AR/R and AW/W/B
 * channels of an AXI slave: each has its own pending queue, pacing and
 * response queue, so a read and a write are served in the same cycle.
 */

#include <algorithm>
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
    // One channel: reads (0) or writes (1).
    struct Channel
    {
        Channel(ReceiverV2 *top, std::string name, vp::ClockEventMeth *pending_meth,
            vp::ClockEventMeth *ready_meth);
        vp::ClockEvent pending_fsm_event;
        vp::ClockEvent ready_fsm_event;
        vp::Queue pending_reqs;
        vp::Queue ready_reqs;
        int64_t ready_timestamp = -1;
        int64_t pace_rem = 0;
        // v2 deny/retry: true when a master was denied and is owed a retry()
        // once capacity returns.
        bool owes_retry = false;
    };

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficReceiverConfig config);
    template<int ch> static void pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    template<int ch> static void ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void log_access(uint64_t addr, uint64_t size, bool is_write);

    vp::Trace trace;

    vp::IoSlave input_itf;
    vp::WireSlave<TrafficReceiverConfig> control_itf;
    Channel channels[2];
    int bandwidth = 0;
    // Pacing rate is <bandwidth> bytes per <bandwidth_period> cycles; the
    // division remainder is carried over to the next request so the average
    // rate is exact even when requests are smaller than the rate quantum.
    int bandwidth_period = 1;
    // Fixed pipelined response delay, on top of the pacing.
    int latency = 0;
    // Per channel.
    int max_pending_reqs = 4;
    vp::Signal<bool> stalled;
    vp::Signal<uint64_t> log_addr;
    vp::Signal<uint64_t> log_size;
    vp::Signal<bool> log_is_write;
    int64_t last_logged_access = -1;
    int nb_logged_access_in_same_cycle = 0;
    int size;
    uint8_t *mem_data;
};

ReceiverV2::Channel::Channel(ReceiverV2 *top, std::string name, vp::ClockEventMeth *pending_meth,
    vp::ClockEventMeth *ready_meth)
    : pending_fsm_event(top, pending_meth), ready_fsm_event(top, ready_meth),
      pending_reqs(top, name + "_pending_reqs", &this->pending_fsm_event),
      ready_reqs(top, name + "_ready_reqs", &this->ready_fsm_event)
{
}

ReceiverV2::ReceiverV2(vp::ComponentConf &config)
    : vp::Component(config),
      input_itf(&ReceiverV2::req),
      channels{
        Channel(this, "read", ReceiverV2::pending_fsm_handler<0>, ReceiverV2::ready_fsm_handler<0>),
        Channel(this, "write", ReceiverV2::pending_fsm_handler<1>, ReceiverV2::ready_fsm_handler<1>)},
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

    Channel &channel = _this->channels[req->get_is_write()];
    if (channel.pending_reqs.size() == _this->max_pending_reqs)
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, full — denying (req: %p)\n", req);
        _this->stalled = true;
        channel.owes_retry = true;
        return vp::IO_REQ_DENIED;
    }
    else
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received req, queuing (req: %p)\n", req);
        channel.pending_reqs.push_back(req);
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
    if (config.mem != nullptr)
    {
        *config.mem = _this->size > 0 ? _this->mem_data : nullptr;
        *config.mem_size = _this->size;
        return;
    }
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Start (bw: %d per %d cycles, latency: %d)\n",
        config.bandwidth, config.bandwidth_period, config.latency);
    _this->bandwidth = config.bandwidth;
    _this->bandwidth_period = config.bandwidth_period;
    _this->latency = config.latency;
    for (Channel &channel : _this->channels)
    {
        channel.pace_rem = 0;
    }
}

template<int ch>
void ReceiverV2::pending_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;
    Channel &channel = _this->channels[ch];

    if (!channel.pending_reqs.empty() && channel.ready_timestamp <= _this->clock.get_cycles())
    {
        vp::IoReq *req = (vp::IoReq *)channel.pending_reqs.pop();

        if (_this->bandwidth > 0)
        {
            int64_t num = (int64_t)req->get_size() * _this->bandwidth_period + channel.pace_rem;
            int64_t cycles = num / _this->bandwidth;
            channel.pace_rem = num % _this->bandwidth;
            channel.ready_timestamp = _this->clock.get_cycles() + cycles;
            channel.ready_reqs.push_back(req,
                std::max((int64_t)0, cycles - 1 + _this->latency));
        }
        else
        {
            channel.ready_reqs.push_back(req, _this->latency);
        }
    }

    channel.pending_reqs.trigger_next();
    channel.ready_reqs.trigger_next();
}

template<int ch>
void ReceiverV2::ready_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    ReceiverV2 *_this = (ReceiverV2 *)__this;
    Channel &channel = _this->channels[ch];

    if (!channel.ready_reqs.empty())
    {
        vp::IoReq *req = (vp::IoReq *)channel.ready_reqs.pop();
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Popping ready req (req: %p)\n", req);
        req->set_resp_status(vp::IO_RESP_OK);
        _this->input_itf.resp(req);
    }

    // v2 deny/retry: when we have capacity again, signal retry() once. The
    // master that was denied is responsible for re-sending its request.
    if (channel.owes_retry && channel.pending_reqs.size() < _this->max_pending_reqs)
    {
        channel.owes_retry = false;
        _this->stalled = _this->channels[!ch].owes_retry;
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Capacity returned, calling retry()\n");
        _this->input_itf.retry(ch ? vp::IO_RETRY_WRITE : vp::IO_RETRY_READ);
    }

    channel.pending_reqs.trigger_next();
    channel.ready_reqs.trigger_next();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ReceiverV2(config);
}
