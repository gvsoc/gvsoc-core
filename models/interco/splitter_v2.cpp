// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Width-splitting fan-out on the io_v2 protocol.
//
// Port of splitter.cpp to io_v2. A single incoming request is carved into
// up to ``nb_outputs = input_width / output_width`` sub-requests. Each
// sub-request covers at most ``output_width`` consecutive bytes of the
// input and is forwarded to a distinct output port — the first chunk on
// ``output_0``, the second on ``output_1``, and so on. Sub-requests run
// in parallel on their respective outputs; the parent completes when the
// last byte has been accounted for.
//
// Chunk boundaries follow the ``output_width`` alignment of the input
// address: the first chunk runs from the input address up to the next
// ``output_width`` boundary, and subsequent chunks are aligned and sized
// at up to ``output_width`` bytes each.
//
// What changed vs v1:
//   - Single-master input slave port. Replies travel back via
//     ``input_itf.resp(req)`` once the parent is done; no v1
//     ``resp_port`` indirection.
//   - Status codes are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``.
//   - Only one parent request may be in flight at a time. A second CPU
//     request that arrives while a parent is still outstanding gets
//     ``IO_REQ_DENIED``, and is retried upstream via ``input_itf.retry()``
//     once the parent completes (``input_needs_retry`` flag).
//   - Downstream DENYs are handled per output via muxed master ports: the
//     denied sub-request is parked in ``stuck[id]`` and re-sent when the
//     same output fires ``retry()``. DENYs on different outputs are
//     independent.
//
// Oversize requests (``size > input_width``) cannot be covered by the
// available outputs. We refuse them up front with ``IO_REQ_DONE`` +
// ``IO_RESP_INVALID`` rather than silently hanging the master.
//
// Timing model: big-packet, zero-latency. The splitter never annotates
// ``req->latency`` and never schedules a ClockEvent. Whatever latency
// each output reports (sync ``IO_REQ_DONE`` with ``req->latency`` or
// async ``resp()`` wall-clock) is observed via the aggregate completion
// time of the parent.

#include <algorithm>
#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <interco/splitter_config.hpp>

class Splitter : public vp::Component
{
public:
    Splitter(vp::ComponentConf &conf);
    void reset(bool active) override;

    SplitterConfig cfg;

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static void            output_resp(vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id);

    vp::IoReq *alloc_sub();
    void       free_sub(vp::IoReq *sub);

    // Drop ``sub``'s bytes from its parent's remaining-size counter and
    // free ``sub``. Does *not* signal parent completion.
    void account_sub(vp::IoReq *sub);

    // Called from the async completion paths (output_resp / output_retry)
    // after ``account_sub``. If the parent is now complete, drains the
    // per-request state and fires ``resp()`` + the pending input retry.
    void maybe_finish_parent();

    vp::Trace trace;

    vp::IoSlave input_itf{&Splitter::input_req};
    std::vector<std::unique_ptr<vp::IoMaster>> outputs;

    int      nb_outputs = 0;
    uint64_t output_width = 0;

    // Current parent request being served. Exactly one at a time —
    // concurrent CPU requests from the upstream are refused with DENIED.
    vp::IoReq *pending_parent = nullptr;

    // For each output, the sub-request that was DENIED and is waiting for
    // that output's retry(). Null while the output is free.
    std::vector<vp::IoReq *> stuck;

    // Free list of sub-request objects, linked via their ``next`` field.
    vp::IoReq *free_list = nullptr;

    // Set when we refused an upstream request with DENIED. Cleared — and
    // retry() fired — once the pending parent completes.
    bool input_needs_retry = false;
};


Splitter::Splitter(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->output_width = (uint64_t)this->cfg.output_width;
    this->nb_outputs   = (int)(this->cfg.input_width / this->cfg.output_width);

    this->outputs.reserve(this->nb_outputs);
    for (int i = 0; i < this->nb_outputs; i++)
    {
        auto m = std::make_unique<vp::IoMaster>(
            i, &Splitter::output_retry, &Splitter::output_resp);
        this->new_master_port("output_" + std::to_string(i), m.get());
        this->outputs.push_back(std::move(m));
    }

    this->stuck.assign((size_t)this->nb_outputs, nullptr);

    this->new_slave_port("input", &this->input_itf);
}

void Splitter::reset(bool active)
{
    if (active)
    {
        this->pending_parent    = nullptr;
        this->input_needs_retry = false;
        std::fill(this->stuck.begin(), this->stuck.end(), nullptr);
    }
}


vp::IoReq *Splitter::alloc_sub()
{
    if (this->free_list == nullptr)
    {
        return new vp::IoReq();
    }
    vp::IoReq *sub = this->free_list;
    this->free_list = sub->get_next();
    return sub;
}

void Splitter::free_sub(vp::IoReq *sub)
{
    sub->set_next(this->free_list);
    this->free_list = sub;
}


void Splitter::account_sub(vp::IoReq *sub)
{
    vp::IoReq *parent = sub->parent;
    if (sub->get_resp_status() == vp::IO_RESP_INVALID)
    {
        parent->set_resp_status(vp::IO_RESP_INVALID);
    }
    parent->remaining_size -= sub->get_size();
    this->free_sub(sub);
}


void Splitter::maybe_finish_parent()
{
    if (this->pending_parent == nullptr ||
        this->pending_parent->remaining_size != 0)
    {
        return;
    }
    vp::IoReq *parent = this->pending_parent;
    this->pending_parent = nullptr;

    this->input_itf.resp(parent);

    if (this->input_needs_retry)
    {
        this->input_needs_retry = false;
        this->input_itf.retry();
    }
}


vp::IoReqStatus Splitter::input_req(vp::Block *__this, vp::IoReq *req)
{
    Splitter *_this = (Splitter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "IO req (req: %p, addr: 0x%llx, size: 0x%llx, write: %d)\n",
        req, (unsigned long long)req->get_addr(),
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0);

    // One parent at a time.
    if (_this->pending_parent != nullptr)
    {
        _this->input_needs_retry = true;
        return vp::IO_REQ_DENIED;
    }

    uint64_t req_size = req->get_size();

    // Refuse oversize requests up front. ``input_width`` is the maximum
    // coverage achievable in a single fan-out; anything larger would
    // silently leave bytes uncovered.
    if (req_size > (uint64_t)_this->cfg.input_width)
    {
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // Empty requests: nothing to forward, trivially done.
    if (req_size == 0)
    {
        req->set_resp_status(vp::IO_RESP_OK);
        return vp::IO_REQ_DONE;
    }

    _this->pending_parent         = req;
    req->remaining_size           = req_size;
    req->set_resp_status(vp::IO_RESP_OK);

    uint64_t addr   = req->get_addr();
    uint64_t size   = req_size;
    uint8_t *data   = req->get_data();
    auto     opcode = req->get_opcode();

    uint64_t ow      = _this->output_width;
    uint64_t ow_mask = ow - 1;

    // Fan out: at most nb_outputs chunks, each aligned at its tail to an
    // ``output_width`` boundary. The first chunk runs from ``addr`` up to
    // the next boundary; subsequent chunks are naturally aligned.
    for (int i = 0; i < _this->nb_outputs && size > 0; i++)
    {
        uint64_t port_size = ow - (addr & ow_mask);
        uint64_t iter_size = std::min(port_size, size);

        vp::IoReq *sub = _this->alloc_sub();
        sub->prepare();
        sub->parent = req;
        sub->set_addr(addr);
        sub->set_size(iter_size);
        sub->set_data(data);
        sub->set_opcode(opcode);
        sub->set_resp_status(vp::IO_RESP_OK);

        addr += iter_size;
        data += iter_size;
        size -= iter_size;

        vp::IoReqStatus st = _this->outputs[i]->req(sub);
        if (st == vp::IO_REQ_DONE)
        {
            _this->account_sub(sub);
        }
        else if (st == vp::IO_REQ_DENIED)
        {
            // Park on this output; resend on output_retry(i).
            _this->stuck[i] = sub;
        }
        // IO_REQ_GRANTED: resp will come later via output_resp.
    }

    // If everything completed inline, return DONE — no need for a later
    // resp() round-trip. Otherwise keep the parent live and return
    // GRANTED so the upstream master parks the request.
    if (req->remaining_size == 0)
    {
        _this->pending_parent = nullptr;
        return vp::IO_REQ_DONE;
    }
    return vp::IO_REQ_GRANTED;
}


void Splitter::output_resp(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    Splitter *_this = (Splitter *)__this;
    _this->account_sub(req);
    _this->maybe_finish_parent();
}


void Splitter::output_retry(vp::Block *__this, int id)
{
    Splitter *_this = (Splitter *)__this;

    vp::IoReq *sub = _this->stuck[id];
    if (sub == nullptr)
    {
        return;
    }
    _this->stuck[id] = nullptr;

    vp::IoReqStatus st = _this->outputs[id]->req(sub);
    if (st == vp::IO_REQ_DONE)
    {
        _this->account_sub(sub);
        _this->maybe_finish_parent();
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Still stuck — hold it until the next retry.
        _this->stuck[id] = sub;
    }
    // IO_REQ_GRANTED: the output will resp() later.
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Splitter(config);
}
