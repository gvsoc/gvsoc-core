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
 * v2 traffic generator. Mirrors interco/traffic/generator.cpp but speaks
 * io_v2 on the output port: IO_REQ_DONE/GRANTED/DENIED, retry() handshake,
 * no arg-stack. The control wire interface is identical to v1 and reuses the
 * v1 TrafficGenerator* types from generator.hpp (which does not include io.hpp).
 */

#include <cstring>
#include <stdexcept>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/queue.hpp>
#include "interco/traffic/generator.hpp"

class TransferV2
{
public:
    uint64_t address;
    uint64_t size;
    bool do_write;
    size_t packet_size;
    uint8_t *data;
};

class GeneratorV2 : public vp::Component, TrafficGenerator
{
public:
    GeneratorV2(vp::ComponentConf &conf);
    ~GeneratorV2();

private:
    void start_transfer() override;
    static void retry_meth(vp::Block *__this, vp::IoRetryChannel);
    static vp::IoRespAck response(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficGeneratorConfig *config);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void handle_req_end(vp::IoReq *req, int64_t latency = 0);
    void handle_step();
    void handle_transfer();
    void handle_post_transfer();
    void handle_end();
    void close_transfer();
    void try_send(vp::IoReq *req);

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
    // True while waiting on retry() from the slave after a DENIED.
    vp::Signal<bool> stalled;
    vp::Signal<bool> busy;
    vp::Signal<uint64_t> signal_req_addr;
    vp::Signal<uint64_t> signal_req_size;
    vp::Signal<bool> signal_req_is_write;
    int64_t last_req_cyclestamp = 0;

    int nb_pending_reqs;
    vp::Queue free_reqs;
    std::queue<TransferV2 *> transfers;
    TransferV2 *current_transfer = NULL;
    uint8_t *ref_data;
    bool check_status;
    int64_t start_cycles;
    int64_t duration;
    int step;
    uint64_t config_size;
    uint64_t config_address;
    size_t packet_size;

    // v2: the slave only sends retry() (no req argument) — the master must
    // hold the request that was denied and re-send it.
    vp::IoReq *stalled_req = nullptr;
    // Per-gen sync-counter latches: ensure each generator increments
    // sync->nb_*_done at most once per case, and parks at the case until the
    // sync barrier is satisfied. Without these, step++ unconditionally and a
    // generator that runs out of work between cross-gen barriers (e.g. when
    // it finishes a phase early) skips through cases 1-3 in a few cycles,
    // bumping the counters prematurely and triggering handle_end while it
    // still has reqs in flight from the next phase.
    bool sync_step1_done = false;
    bool sync_step2_done = false;
    bool sync_step3_done = false;
};

GeneratorV2::GeneratorV2(vp::ComponentConf &config)
    : vp::Component(config),
      output_itf(&GeneratorV2::retry_meth, &GeneratorV2::response),
      fsm_event(this, GeneratorV2::fsm_handler),
      size(*this, "send_size", 64), pending_size(*this, "pending_size", 64),
      signal_req_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
      signal_req_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
      signal_req_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      busy(*this, "busy", 1, true, false),
      stalled(*this, "stalled", 1),
      free_reqs(this, "free_reqs")
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_master_port("output", &this->output_itf);

    this->control_itf.set_sync_meth(&GeneratorV2::control_sync);
    this->new_slave_port("control", &this->control_itf);

    this->nb_pending_reqs = this->get_js_config()->get_int("nb_pending_reqs");
}

GeneratorV2::~GeneratorV2()
{
}

void GeneratorV2::start_transfer()
{
    this->handle_step();
    this->fsm_event.enqueue();
}

void GeneratorV2::retry_meth(vp::Block *__this, vp::IoRetryChannel)
{
    GeneratorV2 *_this = (GeneratorV2 *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received retry\n");
    _this->stalled = false;

    // Retry the denied request immediately if we held one back; otherwise the
    // FSM picks up where it left off.
    if (_this->stalled_req)
    {
        vp::IoReq *r = _this->stalled_req;
        _this->stalled_req = nullptr;
        _this->try_send(r);
    }
    _this->fsm_event.enqueue();
}

vp::IoRespAck GeneratorV2::response(vp::Block *__this, vp::IoReq *req)
{
    GeneratorV2 *_this = (GeneratorV2 *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received response (req: %p)\n", req);
    _this->handle_req_end(req, req->get_latency());
    return vp::IO_RESP_ACCEPTED;
}

void GeneratorV2::control_sync(vp::Block *__this, TrafficGeneratorConfig *config)
{
    GeneratorV2 *_this = (GeneratorV2 *)__this;

    if (config->is_start)
    {
        config->sync->add_generator(_this);

        _this->check_status = false;
        _this->busy = true;
        _this->sync = config->sync;
        _this->stalled = false;
        _this->stalled_req = nullptr;
        _this->check = config->check;
        _this->check_write = config->do_write;
        _this->step = 0;
        _this->sync_step1_done = false;
        _this->sync_step2_done = false;
        _this->sync_step3_done = false;
        _this->config_size = config->size;
        _this->config_address = config->address;
        _this->packet_size = config->packet_size;

        for (int i = 0; i < _this->nb_pending_reqs; i++)
        {
            vp::IoReq *req = new vp::IoReq();
            _this->free_reqs.push_back(req);
        }

        if (config->check)
        {
            _this->ref_data = new uint8_t[config->size];
            for (int i = 0; i < config->size; i++)
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

void GeneratorV2::handle_step()
{
    switch (this->step)
    {
        case 0:
        {
            if (this->check && !this->check_write)
            {
                uint8_t *data = new uint8_t[this->config_size];
                memcpy(data, this->ref_data, this->config_size);
                this->transfers.push(new TransferV2(
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
            if (!this->sync_step1_done)
            {
                this->sync_step1_done = true;
                this->sync->nb_pre_check_done++;
                if (this->sync->nb_pre_check_done == this->sync->generators.size())
                {
                    for (TrafficGenerator *generator : this->sync->generators)
                    {
                        ((GeneratorV2 *)generator)->handle_transfer();
                    }
                }
            }
            // Only advance once every generator has reached this barrier;
            // otherwise this generator parks here, allowing other gens to
            // catch up before any of them moves on to the transfer phase.
            if (this->sync->nb_pre_check_done == this->sync->generators.size())
            {
                this->step++;
            }
            break;
        }

        case 2:
        {
            if (!this->sync_step2_done)
            {
                this->sync_step2_done = true;
                this->sync->nb_transfers_done++;
                if (this->sync->nb_transfers_done == this->sync->generators.size())
                {
                    for (TrafficGenerator *generator : this->sync->generators)
                    {
                        ((GeneratorV2 *)generator)->handle_post_transfer();
                    }
                }
            }
            if (this->sync->nb_transfers_done == this->sync->generators.size())
            {
                this->step++;
            }
            break;
        }

        case 3:
        {
            if (!this->sync_step3_done)
            {
                this->sync_step3_done = true;
                this->sync->nb_post_check_done++;
                if (this->sync->nb_post_check_done == this->sync->generators.size())
                {
                    for (TrafficGenerator *generator : this->sync->generators)
                    {
                        ((GeneratorV2 *)generator)->handle_end();
                    }
                    this->sync->event->enqueue();
                }
            }
            if (this->sync->nb_post_check_done == this->sync->generators.size())
            {
                this->step++;
            }
            break;
        }
    }
}

void GeneratorV2::handle_transfer()
{
    this->start_cycles = this->clock.get_cycles();

    uint8_t *data = new uint8_t[this->config_size];

    if (this->check && this->check_write)
    {
        memcpy(data, this->ref_data, this->config_size);
    }

    this->close_transfer();

    this->transfers.push(new TransferV2(
        {this->config_address, this->config_size, this->check_write, this->packet_size, data}));
    this->fsm_event.enqueue();
}

void GeneratorV2::handle_post_transfer()
{
    this->duration = this->clock.get_cycles() - this->start_cycles;

    if (this->check && this->check_write)
    {
        this->close_transfer();

        uint8_t *data = new uint8_t[this->config_size];
        this->transfers.push(new TransferV2(
            {this->config_address, this->config_size, !this->check_write, this->packet_size, data}));
        this->fsm_event.enqueue();
    }
}

void GeneratorV2::handle_end()
{
    if (this->check)
    {
        this->check_status |= std::memcmp(this->ref_data, this->current_transfer->data,
            this->current_transfer->size) != 0;
    }

    for (int i = 0; i < this->nb_pending_reqs; i++)
    {
        vp::IoReq *req = (vp::IoReq *)this->free_reqs.pop();
        delete req;
    }

    this->close_transfer();
    this->busy = false;
}

void GeneratorV2::close_transfer()
{
    if (this->current_transfer)
    {
        delete[] this->current_transfer->data;
        delete this->current_transfer;
        this->current_transfer = NULL;
    }
}

void GeneratorV2::try_send(vp::IoReq *req)
{
    this->signal_req_addr.set_and_release(req->get_addr());
    this->signal_req_size.set_and_release(req->get_size());
    this->signal_req_is_write.set_and_release(req->get_is_write());

    vp::IoReqStatus status = this->output_itf.req(req);

    if (status == vp::IO_REQ_DENIED)
    {
        // v2 deny: hold the req; we'll resend on retry().
        this->stalled = true;
        this->stalled_req = req;
    }
    else if (status == vp::IO_REQ_GRANTED)
    {
        this->fsm_event.enqueue();
    }
    else
    {
        // IO_REQ_DONE: response is already complete in this call.
        this->handle_req_end(req, req->get_latency() - 1);
    }
}

void GeneratorV2::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    GeneratorV2 *_this = (GeneratorV2 *)__this;

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

        req->set_size(_this->current_transfer->packet_size);
        req->set_addr(_this->address);
        req->set_data(_this->data);
        req->set_is_write(_this->current_transfer->do_write);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending request (req: %p, address: 0x%llx, size: 0x%llx, packet_size: 0x%llx)\n",
            req, _this->address, _this->current_transfer->packet_size, _this->current_transfer->packet_size);

        _this->address += _this->current_transfer->packet_size;
        _this->data += _this->current_transfer->packet_size;
        _this->size -= _this->current_transfer->packet_size;

        _this->try_send(req);
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

void GeneratorV2::handle_req_end(vp::IoReq *req, int64_t latency)
{
    this->pending_size -= req->get_size();
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Handling req end (req: %p, size: 0x%x, pending_size: 0x%x, latency: %ld)\n",
        req, req->get_size(), this->pending_size.get(), latency);

    if (latency < 0) latency = 0;
    this->free_reqs.push_back(req, latency);
    this->last_req_cyclestamp = this->clock.get_cycles() + latency;

    this->fsm_event.enqueue();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new GeneratorV2(config);
}
