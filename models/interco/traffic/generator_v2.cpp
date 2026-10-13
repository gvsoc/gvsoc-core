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
 *
 * The output port is IoV2Beat(width of the driven port): it binds beat slaves RAW,
 * so the generator is a beat-plane write master and follows the per-burst
 * write-acknowledgement contract (io_v2.hpp "Write acknowledgement" +
 * "Request allocation"):
 *   - every outgoing request is drawn from the shared size-0 IoReqAllocator
 *     pool (write beats cross a consumer-frees boundary on beat planes);
 *   - a GRANTED write transfers ownership of the object (buffer included) to
 *     the target, which consumes and frees it — the generator forgets the
 *     pointer at GRANT and never dereferences it again;
 *   - the burst (always submitted as a one-beat burst, is_first=is_last=true)
 *     is acknowledged exactly once, either inline (IO_REQ_DONE) or with a
 *     single data-less ack via resp() that the generator frees;
 *   - correlation is by req->initiator, which carries a per-transfer
 *     TransferRecord (beat slaves copy it onto their response beats/acks) —
 *     never by object identity, since a consumed write beat may be recycled
 *     (even as the ack itself) while its burst is still in flight.
 * On non-beat (round-trip) planes the same objects simply come back through
 * resp()/DONE and are freed by the generator, so one code path serves both.
 */

#include <cstring>
#include <stdexcept>
#include <unordered_set>
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
    // Second stream, interleaved packet by packet (interleave only).
    bool interleave = false;
    uint64_t address2 = 0;
    bool do_write2 = false;
    uint8_t *data2 = nullptr;
};

// Per-in-flight-request record. It is the initiator handle carried by
// req->initiator (and copied by beat slaves onto their distinct response
// beats / burst acks), the read fill cursor, and — recycled through the
// free_reqs queue — the outstanding-window token that paces the generator
// (nb_pending_reqs) exactly as the embedded request objects did before the
// pool port.
class TransferRecord : public vp::QueueElem
{
public:
    // Our pool-backed request. Forgotten (nullptr) as soon as ownership is
    // gone: at GRANT for writes (the target frees it), at completion
    // otherwise. Never dereferenced after that point.
    vp::IoReq *req = nullptr;
    uint8_t *data = nullptr;   // destination buffer slice (reads)
    uint64_t size = 0;         // bytes covered by this request
    uint64_t fill = 0;         // fill cursor for distinct read response beats
    bool is_write = false;
};

class GeneratorV2 : public vp::Component, TrafficGenerator
{
public:
    GeneratorV2(vp::ComponentConf &conf);
    ~GeneratorV2();

private:
    void reset(bool active) override;
    void start_transfer() override;
    static void retry_meth(vp::Block *__this, vp::IoRetryChannel);
    static vp::IoRespAck response(vp::Block *__this, vp::IoReq *req);
    static void control_sync(vp::Block *__this, TrafficGeneratorConfig *config);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void handle_req_end(TransferRecord *rec, int64_t latency = 0);
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
    // Second stream of an interleaved transfer: its cursor and bytes left,
    // and which stream sends the next packet.
    uint64_t address2 = 0;
    uint8_t *data2 = nullptr;
    uint64_t size2 = 0;
    int next_stream = 0;
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
    // Shared size-0 pool serving every outgoing request (fetched once at
    // construction). data is caller-managed on this pool: set on every
    // allocation.
    vp::IoReqAllocator *req_allocator;
    // Records of transfers currently in flight, for reset-time reclamation
    // of the pool objects we still own.
    std::unordered_set<TransferRecord *> outstanding;
    // Recycled TransferRecord tokens (outstanding-window pacing).
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
    bool config_interleave = false;
    uint64_t config_address2 = 0;
    bool config_do_write2 = false;
    // Burst legalization boundary (AXI 4KB rule); 0 disables it.
    uint64_t max_burst_size;

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
    this->max_burst_size = this->get_js_config()->get_uint("max_burst_size");

    // Requests cross consumer-frees boundaries on beat planes (write beats
    // freed by the target, acks freed by us), so they must be pool-backed.
    this->req_allocator = vp::IoReqAllocator::get(0);
}

void GeneratorV2::reset(bool active)
{
    if (active)
    {
        // Free the pool objects we still own: a DENIED-held request (never
        // accepted by the slave) and the read descriptors of in-flight
        // transfers (initiator-owned). Write requests granted away are the
        // target's to free — their record's req pointer was already
        // forgotten at GRANT, so it is never touched here. The records
        // themselves follow the pre-existing start/end lifecycle.
        for (TransferRecord *rec : this->outstanding)
        {
            if (rec->req != nullptr)
            {
                rec->req->free();
                rec->req = nullptr;
            }
        }
        this->outstanding.clear();
        this->stalled_req = nullptr;
    }
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

    // Any-plane master contract: tolerate every response shape. Correlation
    // is by req->initiator, which carries our per-transfer record (set at
    // issue; beat slaves copy it onto their distinct response beats and onto
    // the burst ack; round-trip planes preserve it on our own object) — never
    // by object identity.
    TransferRecord *rec = (TransferRecord *)req->initiator;

    if (rec->is_write)
    {
        // Per-burst write ack (io_v2.hpp "Write acknowledgement"). Three
        // shapes converge here:
        //   - round-trip (non-beat) plane: our own object comes back;
        //   - beat plane: a distinct data-less allocator-backed ack (the
        //     object we sent is gone — the target consumed and freed it, so
        //     rec->req was forgotten at GRANT and is not touched);
        //   - beat plane, recycled: the target recycled our consumed beat as
        //     the ack (same pointer — invisible by design).
        // In all three the responded object now belongs to us: read the
        // timing, free it, and complete the transfer.
        _this->traces.assert(req->is_last,
            "write burst acked with is_last == false (req=%p)", req);
        int64_t latency = req->get_latency();
        req->free();
        _this->handle_req_end(rec, latency);
        return vp::IO_RESP_ACCEPTED;
    }

    if (req == rec->req)
    {
        // Big-packet read response: our own object round-tripped and the
        // slave filled our buffer in place. Recycle it to its pool.
        int64_t latency = req->get_latency();
        rec->req = nullptr;
        req->free();
        _this->handle_req_end(rec, latency);
        return vp::IO_RESP_ACCEPTED;
    }

    // Distinct read response beat (initiator-owned convention, e.g. from the
    // FlooNoC NI): copy the payload into our buffer at the record's fill
    // cursor and free the beat. On the last beat, also free our own read
    // descriptor — the initiator owns it, the slave never frees it — and
    // complete the transfer. (The cursor lives in the record: remaining_size
    // on the request is NOT ours to use — a slave may claim it for its own
    // per-burst accounting on the very same object.)
    if (req->get_size() > 0)
    {
        memcpy(rec->data + rec->fill, req->get_data(), req->get_size());
    }
    rec->fill += req->get_size();
    bool last = req->is_last;
    int64_t latency = req->get_latency();
    req->free();
    if (last)
    {
        rec->req->free();
        rec->req = nullptr;
        _this->handle_req_end(rec, latency);
    }
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
        _this->config_interleave = config->interleave;
        _this->config_address2 = config->address2;
        _this->config_do_write2 = config->do_write2;
        if (config->interleave && config->check)
        {
            throw std::invalid_argument("interleaved transfers have no data check");
        }

        for (int i = 0; i < _this->nb_pending_reqs; i++)
        {
            // Window tokens only: the request objects themselves are drawn
            // from the shared pool at send time (see fsm_handler).
            _this->free_reqs.push_back(new TransferRecord());
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
                // Capture the duration at THIS generator's own completion.
                // The post-transfer hook below only runs once every generator
                // reached the barrier, which would report the slowest flow's
                // time for all of them and mask any unfairness between flows.
                this->duration = this->clock.get_cycles() - this->start_cycles;
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

    TransferV2 *transfer = new TransferV2(
        {this->config_address, this->config_size, this->check_write, this->packet_size, data});
    if (this->config_interleave)
    {
        transfer->interleave = true;
        transfer->address2 = this->config_address2;
        transfer->do_write2 = this->config_do_write2;
        transfer->data2 = new uint8_t[this->config_size];
    }
    this->transfers.push(transfer);
    this->fsm_event.enqueue();
}

void GeneratorV2::handle_post_transfer()
{
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
        TransferRecord *rec = (TransferRecord *)this->free_reqs.pop();
        delete rec;
    }

    this->close_transfer();
    this->busy = false;
}

void GeneratorV2::close_transfer()
{
    if (this->current_transfer)
    {
        delete[] this->current_transfer->data;
        delete[] this->current_transfer->data2;
        delete this->current_transfer;
        this->current_transfer = NULL;
    }
}

void GeneratorV2::try_send(vp::IoReq *req)
{
    // Snapshot the record BEFORE sending: a GRANTED write transfers
    // ownership of the object to the target, which may free it synchronously
    // inside req() — no field may be read after that point.
    TransferRecord *rec = (TransferRecord *)req->initiator;

    this->signal_req_addr.set_and_release(req->get_addr());
    this->signal_req_size.set_and_release(req->get_size());
    this->signal_req_is_write.set_and_release(req->get_is_write());

    vp::IoReqStatus status = this->output_itf.req(req);

    if (status == vp::IO_REQ_DENIED)
    {
        // v2 deny: ownership stays with us — hold the req; we'll resend on
        // retry().
        this->stalled = true;
        this->stalled_req = req;
    }
    else if (status == vp::IO_REQ_GRANTED)
    {
        if (rec->is_write)
        {
            // Write beat granted: ownership (buffer included) went with it.
            // On a beat plane the target consumes and frees it; on a
            // round-trip plane it comes back through response(). Either way,
            // forget the pointer — nothing may dereference it after GRANT.
            rec->req = nullptr;
        }
        this->fsm_event.enqueue();
    }
    else
    {
        // IO_REQ_DONE: inline completion (for a write, the inline burst
        // ack) — we keep the object; recycle it to its pool and complete the
        // transfer with the inline status/latency.
        int64_t latency = req->get_latency() - 1;
        rec->req = nullptr;
        req->free();
        this->handle_req_end(rec, latency);
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
        _this->next_stream = 0;
        _this->size2 = 0;
        if (_this->current_transfer->interleave)
        {
            _this->size2 = _this->current_transfer->size;
            _this->pending_size = 2 * _this->current_transfer->size;
            _this->address2 = _this->current_transfer->address2;
            _this->data2 = _this->current_transfer->data2;
        }
    }

    if (!_this->stalled && (_this->size > 0 || _this->size2 > 0) && !_this->free_reqs.empty())
    {
        // Interleaved transfer: one packet of each stream in turn, the
        // other one once a stream is done.
        bool second = _this->size2 > 0 && (_this->next_stream == 1 || _this->size == 0);
        _this->next_stream = second ? 0 : 1;
        uint64_t &address = second ? _this->address2 : _this->address;
        uint8_t *&data = second ? _this->data2 : _this->data;
        uint64_t left = second ? _this->size2 : _this->size.get();
        bool do_write = second ? _this->current_transfer->do_write2 : _this->current_transfer->do_write;

        TransferRecord *rec = (TransferRecord *)_this->free_reqs.pop();

        // Drawn per send from the shared size-0 pool: the object may end up
        // freed by a beat target (write beat) so it cannot be an embedded
        // object. data is caller-managed on this pool — set on every
        // allocation (below); no other field is reinitialized on recycle.
        vp::IoReq *req = _this->req_allocator->alloc();
        req->prepare();

        // Burst legalization, like an AXI DMA (iDMA HardwareLegalizer): a burst
        // may not exceed max_burst_size nor cross a max_burst_size boundary (the
        // AXI 4KB rule), so it always lands in a single target. Cap the chunk to
        // the requested packet_size, the bytes left before the next boundary,
        // and the bytes left in the transfer.
        uint64_t chunk = _this->current_transfer->packet_size;
        if (_this->max_burst_size > 0)
        {
            uint64_t to_boundary = _this->max_burst_size - (address % _this->max_burst_size);
            if (chunk > to_boundary) chunk = to_boundary;
        }
        if (chunk > left) chunk = left;

        req->set_size(chunk);
        req->set_addr(address);
        req->set_data(data);
        req->set_is_write(do_write);
        // Correlation handle for the initiator-owned convention: a beat
        // slave answers a read with distinct beat objects — and a write
        // burst with a single data-less ack — carrying this pointer in
        // req->initiator. The per-transfer record (not the request pointer)
        // is the key: a consumed write beat may be recycled by the pool
        // while its ack is still in flight, so object identity is not a
        // safe correlator. (remaining_size is not ours to use either —
        // slaves may claim it on the same object.)
        req->initiator = rec;
        req->is_first = true;
        req->is_last = true;
        req->burst_id = -1;

        rec->req = req;
        rec->data = data;
        rec->size = chunk;
        rec->fill = 0;
        rec->is_write = do_write;
        _this->outstanding.insert(rec);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending request (req: %p, address: 0x%llx, size: 0x%llx, packet_size: 0x%llx)\n",
            req, address, chunk, _this->current_transfer->packet_size);

        address += chunk;
        data += chunk;
        if (second) _this->size2 -= chunk;
        else _this->size -= chunk;

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

void GeneratorV2::handle_req_end(TransferRecord *rec, int64_t latency)
{
    this->pending_size -= rec->size;
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Handling req end (rec: %p, size: 0x%lx, pending_size: 0x%lx, latency: %ld)\n",
        rec, rec->size, this->pending_size.get(), latency);

    this->outstanding.erase(rec);

    if (latency < 0) latency = 0;
    this->free_reqs.push_back(rec, latency);
    this->last_req_cyclestamp = this->clock.get_cycles() + latency;

    this->fsm_event.enqueue();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new GeneratorV2(config);
}
