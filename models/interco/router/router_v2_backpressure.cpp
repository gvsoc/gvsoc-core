// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Synchronous address-decoding router on the io_v2 protocol.
 *
 * Timing model: latency is applied *before* forwarding the request. The router reads its
 * own availability timestamps (input and output bandwidth watermarks), computes the
 * delay, and either forwards inline or schedules a ClockEvent to perform the forward
 * later. The response path is a pure pass-through with no scheduling.
 *
 * Scope limitation: single-mapping only. Straddling mappings -> IO_RESP_INVALID.
 * No proxy/debug access (no RouterCommon subclass).
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/stats/stats.hpp>
#include <vp/mapping_tree.hpp>

class RouterBackpressure;

class BandwidthLimiter
{
public:
    BandwidthLimiter(int64_t bandwidth, int64_t latency, bool shared_rw_bandwidth);
    // Returns cycles of delay to apply to a request of `size` bytes issued at `now`,
    // without mutating state. Caller must apply the delay AND call commit() once the
    // request is actually forwarded.
    int64_t peek(int64_t now, uint64_t size, bool is_write);
    // Advances the internal watermark. Call only after a successful (non-denied) forward.
    void commit(int64_t now, uint64_t size, bool is_write);

private:
    int64_t bandwidth;
    int64_t latency;
    bool shared_rw_bandwidth;
    int64_t next_read_burst_cycle = 0;
    int64_t next_write_burst_cycle = 0;
};


class OutputPort
{
public:
    OutputPort(RouterBackpressure *top, int id, std::string name, int64_t bandwidth,
        int64_t latency, bool shared_rw_bandwidth);
    RouterBackpressure *top;
    int id;
    vp::IoMaster itf;
    BandwidthLimiter bw;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    int64_t mapping_latency = 0;
};


class InputPort
{
public:
    InputPort(RouterBackpressure *top, int id, std::string name, int64_t bandwidth,
        int64_t latency, bool shared_rw_bandwidth, int nb_outputs);
    RouterBackpressure *top;
    int id;
    vp::IoSlave itf;
    BandwidthLimiter bw;
    // Deny bit per output. Set when the output denied a forward from this input.
    // Cleared when the output retries.
    std::vector<bool> denied_on_output;
    // Pending delayed-send state for this input. At most one pending send outstanding.
    vp::IoReq *pending_req = nullptr;
    int pending_mapping_id = -1;
    vp::ClockEvent send_event;
};


class RouterBackpressure : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterBackpressure(vp::ComponentConf &conf);

private:
    // Slave callback (muxed by input port id).
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    // Master callbacks (muxed by output / mapping id).
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int id);
    static void retry_muxed(vp::Block *__this, int id);
    // Delayed-forward handler. One event per input port; the InputPort* is stashed in
    // event->get_args()[0].
    static void send_handler(vp::Block *__this, vp::ClockEvent *event);

    // Core forward path. Returns the status to hand back to the caller of
    // IoMaster::req(). `from_delayed` distinguishes the inline vs delayed-forward case.
    vp::IoReqStatus forward(InputPort *in, vp::IoReq *req, int mapping_id, bool from_delayed);

    // InFlight pool (freelist) — one InFlight per forwarded request, alive while the
    // downstream has ownership.
    struct InFlight {
        InputPort *input;
        void *saved_initiator;
        vp::IoReq *req;
        InFlight *next;
    };
    InFlight *inflight_free = nullptr;
    InFlight *alloc_inflight();
    void free_inflight(InFlight *ifl);

    vp::Trace trace;
    std::vector<InputPort *> inputs;
    std::vector<OutputPort *> entries;
    vp::MappingTree mapping_tree;
    int error_id = -1;

    vp::StatScalar stat_reads;
    vp::StatScalar stat_writes;
    vp::StatScalar stat_bytes_read;
    vp::StatScalar stat_bytes_written;
    vp::StatScalar stat_errors;
    vp::StatBw stat_read_bw;
    vp::StatBw stat_write_bw;
};


//
// BandwidthLimiter
//

BandwidthLimiter::BandwidthLimiter(int64_t bandwidth, int64_t latency, bool shared_rw_bandwidth)
    : bandwidth(bandwidth), latency(latency), shared_rw_bandwidth(shared_rw_bandwidth)
{
}

int64_t BandwidthLimiter::peek(int64_t now, uint64_t size, bool is_write)
{
    if (this->bandwidth == 0)
    {
        return this->latency;
    }
    int64_t *next_burst_cycle = (is_write || this->shared_rw_bandwidth) ?
        &this->next_write_burst_cycle : &this->next_read_burst_cycle;
    int64_t router_delay = *next_burst_cycle - now;
    if (router_delay < 0) router_delay = 0;
    return router_delay + this->latency;
}

void BandwidthLimiter::commit(int64_t now, uint64_t size, bool is_write)
{
    if (this->bandwidth == 0) return;
    int64_t *next_burst_cycle = (is_write || this->shared_rw_bandwidth) ?
        &this->next_write_burst_cycle : &this->next_read_burst_cycle;
    int64_t burst_duration = (size + this->bandwidth - 1) / this->bandwidth;
    *next_burst_cycle = std::max(now, *next_burst_cycle) + burst_duration;
}


//
// OutputPort
//

OutputPort::OutputPort(RouterBackpressure *top, int id, std::string name, int64_t bandwidth,
        int64_t latency, bool shared_rw_bandwidth)
    : top(top), id(id),
      itf(id, &RouterBackpressure::retry_muxed, &RouterBackpressure::resp_muxed),
      bw(bandwidth, latency, shared_rw_bandwidth)
{
}


//
// InputPort
//

InputPort::InputPort(RouterBackpressure *top, int id, std::string name, int64_t bandwidth,
        int64_t latency, bool shared_rw_bandwidth, int nb_outputs)
    : top(top), id(id),
      itf(id, &RouterBackpressure::req_muxed),
      bw(bandwidth, latency, shared_rw_bandwidth),
      denied_on_output(nb_outputs, false),
      send_event(top, &RouterBackpressure::send_handler)
{
    // Stash a pointer to ourselves in the event so the handler can recover us.
    this->send_event.get_args()[0] = this;
}


//
// RouterBackpressure
//

RouterBackpressure::RouterBackpressure(vp::ComponentConf &config)
    : vp::Component(config), mapping_tree(&this->trace)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->stats.register_stat(&this->stat_reads, "reads", "Number of read requests");
    this->stats.register_stat(&this->stat_writes, "writes", "Number of write requests");
    this->stats.register_stat(&this->stat_bytes_read, "bytes_read", "Total bytes read");
    this->stats.register_stat(&this->stat_bytes_written, "bytes_written", "Total bytes written");
    this->stats.register_stat(&this->stat_errors, "errors", "Requests with no valid mapping");
    this->stats.register_stat(&this->stat_read_bw, "read_bandwidth", "Average read bandwidth");
    this->stat_read_bw.set_source(&this->stat_bytes_read);
    this->stats.register_stat(&this->stat_write_bw, "write_bandwidth", "Average write bandwidth");
    this->stat_write_bw.set_source(&this->stat_bytes_written);

    int bandwidth = this->get_js_config()->get_int("bandwidth");
    int latency = this->get_js_config()->get_int("latency");
    int nb_input_port = this->get_js_config()->get_int("nb_input_port");
    bool shared_rw_bandwidth = this->get_js_config()->get_child_bool("shared_rw_bandwidth");

    // First build the mappings so we know the number of outputs for the per-input deny
    // bitmaps.
    int nb_outputs = 0;
    js::Config *mappings = this->get_js_config()->get("mappings");
    if (mappings != NULL)
    {
        nb_outputs = mappings->get_childs().size();
    }

    this->inputs.resize(nb_input_port);
    for (int i = 0; i < nb_input_port; i++)
    {
        std::string name = i == 0 ? "input" : "input_" + std::to_string(i);
        InputPort *in = new InputPort(this, i, name, bandwidth, latency,
            shared_rw_bandwidth, nb_outputs);
        this->inputs[i] = in;
        this->new_slave_port(name, &in->itf, this);
    }

    if (mappings != NULL)
    {
        int mapping_id = 0;
        for (auto &mapping : mappings->get_childs())
        {
            js::Config *mapping_config = mapping.second;
            std::string name = mapping.first;

            this->mapping_tree.insert(mapping_id, name, mapping_config);

            // OutputPort's BandwidthLimiter takes the router's global per-port latency
            // (same as InputPort). The mapping's own latency is tracked separately as
            // `mapping_latency` and added once on top of max(d_in, d_out).
            OutputPort *out = new OutputPort(this, mapping_id, name, bandwidth,
                latency, shared_rw_bandwidth);
            out->remove_offset = mapping_config->get_uint("remove_offset");
            out->add_offset = mapping_config->get_uint("add_offset");
            out->mapping_latency = mapping_config->get_int("latency");
            this->entries.push_back(out);
            this->new_master_port(name, &out->itf);

            if (name == "error")
            {
                this->error_id = mapping_id;
            }
            mapping_id++;
        }
        this->mapping_tree.build();
    }
}

RouterBackpressure::InFlight *RouterBackpressure::alloc_inflight()
{
    InFlight *ifl = this->inflight_free;
    if (ifl != nullptr)
    {
        this->inflight_free = ifl->next;
    }
    else
    {
        ifl = new InFlight();
    }
    ifl->next = nullptr;
    return ifl;
}

void RouterBackpressure::free_inflight(InFlight *ifl)
{
    ifl->next = this->inflight_free;
    this->inflight_free = ifl;
}

vp::IoReqStatus RouterBackpressure::req_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterBackpressure *_this = (RouterBackpressure *)__this;
    InputPort *in = _this->inputs[port];
    int64_t now = _this->clock.get_cycles();

    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (input: %d, offset: 0x%lx, size: 0x%lx, is_write: %d)\n",
        port, offset, size, is_write);

    // One-pending rule: reject a new request while we still have a delayed-send
    // outstanding for this input.
    if (in->pending_req != nullptr)
    {
        return vp::IO_REQ_DENIED;
    }

    if (is_write)
    {
        _this->stat_writes++;
        _this->stat_bytes_written += size;
    }
    else
    {
        _this->stat_reads++;
        _this->stat_bytes_read += size;
    }

    // Decode the mapping.
    vp::MappingTreeEntry *mapping = _this->mapping_tree.get(offset, size, is_write);
    if (!mapping || mapping->id == _this->error_id)
    {
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // Straddling: single-mapping rule only. Treat as address error, not DENIED.
    if (mapping->size != 0 && offset + size > mapping->base + mapping->size)
    {
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    OutputPort *out = _this->entries[mapping->id];

    if (!out->itf.is_bound())
    {
        _this->trace.msg(vp::Trace::LEVEL_WARNING,
            "Invalid access, target not connected (mapping: %s)\n",
            mapping->name.c_str());
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    // Compute the delay budget: max of input and output port availability, plus the
    // mapping-specific latency.
    int64_t d_in = in->bw.peek(now, size, is_write);
    int64_t d_out = out->bw.peek(now, size, is_write);
    int64_t delay = std::max(d_in, d_out) + out->mapping_latency;

    // Translate the address to the output's space.
    req->set_addr(offset - out->remove_offset + out->add_offset);

    if (delay == 0)
    {
        // Forward inline.
        return _this->forward(in, req, mapping->id, /*from_delayed=*/false);
    }
    else
    {
        // Defer the forward. The upstream sees us as GRANTED and will wait for our
        // resp() (which will come from the send_handler -> forward() path).
        in->pending_req = req;
        in->pending_mapping_id = mapping->id;
        in->send_event.enqueue(delay);
        return vp::IO_REQ_GRANTED;
    }
}

vp::IoReqStatus RouterBackpressure::forward(InputPort *in, vp::IoReq *req, int mapping_id, bool from_delayed)
{
    int64_t now = this->clock.get_cycles();
    OutputPort *out = this->entries[mapping_id];
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    // Allocate InFlight and thread it through req->initiator for the round trip.
    InFlight *ifl = this->alloc_inflight();
    ifl->input = in;
    ifl->saved_initiator = req->initiator;
    ifl->req = req;
    req->initiator = ifl;

    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Forwarding req (input: %d, mapping: %d, delayed: %d)\n",
        in->id, mapping_id, from_delayed);

    vp::IoReqStatus st = out->itf.req(req);

    switch (st)
    {
        case vp::IO_REQ_DENIED:
        {
            // Rollback: the downstream did not take the request.
            req->initiator = ifl->saved_initiator;
            this->free_inflight(ifl);
            in->denied_on_output[mapping_id] = true;
            // NO bw commit.

            if (from_delayed)
            {
                // Upstream is already holding us to GRANTED. We must not return DENIED
                // now. Keep the pending state set and wait for retry_muxed(mapping_id)
                // which will notice pending_req != NULL and retry the forward.
                in->pending_req = req;
                in->pending_mapping_id = mapping_id;
                return vp::IO_REQ_DENIED; // caller (send_handler) ignores the value
            }
            return vp::IO_REQ_DENIED;
        }
        case vp::IO_REQ_DONE:
        {
            in->bw.commit(now, size, is_write);
            out->bw.commit(now, size, is_write);
            req->initiator = ifl->saved_initiator;
            this->free_inflight(ifl);

            if (from_delayed)
            {
                // Upstream only saw GRANTED for this request. Deliver the response.
                in->itf.resp(req);
            }
            return vp::IO_REQ_DONE;
        }
        case vp::IO_REQ_GRANTED:
        {
            in->bw.commit(now, size, is_write);
            out->bw.commit(now, size, is_write);
            // ifl stays alive until resp_muxed. Upstream already holds GRANTED (either
            // from the handle_req return or from the earlier scheduled send).
            return vp::IO_REQ_GRANTED;
        }
    }
    return vp::IO_REQ_DONE; // unreachable
}

void RouterBackpressure::send_handler(vp::Block *__this, vp::ClockEvent *event)
{
    InputPort *in = (InputPort *)event->get_args()[0];
    RouterBackpressure *_this = in->top;

    vp::IoReq *req = in->pending_req;
    int mapping_id = in->pending_mapping_id;
    in->pending_req = nullptr;
    in->pending_mapping_id = -1;

    _this->forward(in, req, mapping_id, /*from_delayed=*/true);
    // forward() handles all three outcomes including restoring pending_req on DENIED.
}

void RouterBackpressure::resp_muxed(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    RouterBackpressure *_this = (RouterBackpressure *)__this;
    InFlight *ifl = (InFlight *)req->initiator;
    InputPort *in = ifl->input;
    req->initiator = ifl->saved_initiator;
    _this->free_inflight(ifl);
    in->itf.resp(req);
}

void RouterBackpressure::retry_muxed(vp::Block *__this, int id)
{
    RouterBackpressure *_this = (RouterBackpressure *)__this;

    // Walk inputs. Two cases keyed by whether the input has a pending delayed-send:
    // - pending_req != NULL: the deny happened inside the router itself during a
    //   delayed-forward attempt. Upstream is still waiting. Retry the forward now.
    // - pending_req == NULL: the deny was propagated to upstream synchronously. Signal
    //   retry() to the input's master so it can resend.
    for (InputPort *in : _this->inputs)
    {
        if (!in->denied_on_output[id]) continue;
        in->denied_on_output[id] = false;

        if (in->pending_req != nullptr && in->pending_mapping_id == id)
        {
            vp::IoReq *req = in->pending_req;
            int mapping_id = in->pending_mapping_id;
            in->pending_req = nullptr;
            in->pending_mapping_id = -1;
            _this->forward(in, req, mapping_id, /*from_delayed=*/true);
        }
        else
        {
            in->itf.retry();
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterBackpressure(config);
}
