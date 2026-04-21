// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Bandwidth-based address-decoding router on the io_v2 protocol.
 *
 * Fastest of the three router variants. Every incoming request is forwarded to the
 * output inline — there is no scheduling of ClockEvents, no internal watermark-based
 * queueing. Timing is reported purely via `req->latency`:
 *
 *     logical_latency = max(0, next_available - now) + router_latency + mapping_latency
 *                     + ceil(size / bandwidth)
 *     req->inc_latency(logical_latency)
 *     next_available  := now + logical_latency  (moved forward by this request's cost)
 *
 * A latency-aware initiator reads `req->get_latency()` and paces itself accordingly
 * (e.g. advances its internal cycle counter by that much before issuing the next
 * request). Wall-clock on the simulator stays at `now`.
 *
 * The only time a request is NOT forwarded inline is when the output is stalled
 * because a previous downstream DENY has not yet been retried. In that case the
 * request is placed in the input's queue; when `retry_muxed` fires, queued requests
 * are drained and re-forwarded.
 *
 * Scope: single-mapping only (straddling mappings -> IO_RESP_INVALID). No
 * proxy/debug access.
 */

#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/stats/stats.hpp>
#include <vp/mapping_tree.hpp>

class RouterBandwidth;

class OutputPort
{
public:
    OutputPort(RouterBandwidth *top, int id, std::string name);
    RouterBandwidth *top;
    int id;
    vp::IoMaster itf;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    int64_t mapping_latency = 0;
    // Bandwidth watermark: earliest (logical) cycle at which this output can accept
    // the next request. Advanced on every accepted forward.
    int64_t next_available_cycle = 0;
    // True when the downstream returned DENIED for the most recent forward; cleared
    // by retry_muxed.
    bool stalled = false;
};

struct QueuedReq
{
    vp::IoReq *req;
    int output_id;
};

class InputPort
{
public:
    InputPort(RouterBandwidth *top, int id, std::string name);
    RouterBandwidth *top;
    int id;
    vp::IoSlave itf;
    // Bandwidth watermark for this input. Tracks pure throughput on the input side;
    // does not include router or mapping latency.
    int64_t next_available_cycle = 0;
    // Requests that were accepted by the router (GRANTED to master) but couldn't be
    // forwarded because some output was stalled. Drained when the output retries.
    std::deque<QueuedReq> queue;
};

struct InFlight
{
    InputPort *input;
    void *saved_initiator;
};

class RouterBandwidth : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterBandwidth(vp::ComponentConf &conf);

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int id);
    static void retry_muxed(vp::Block *__this, int id);

    // Core forward: compute and apply the latency annotation, then call out.itf.req.
    // Returns the status to propagate to the caller. On DENIED, side-effect: output
    // is marked stalled and the latency annotation is rolled back.
    vp::IoReqStatus forward_inline(InputPort *in, vp::IoReq *req, OutputPort *out,
                                    int64_t now);

    // Try to drain an input's queue after its blocking output has been retried.
    void drain_queue(InputPort *in);

    InFlight *alloc_inflight();
    void free_inflight(InFlight *ifl);
    InFlight *inflight_free = nullptr;

    int64_t bandwidth;
    int64_t latency;
    bool shared_rw_bandwidth;
    int nb_input_port;
    vp::MappingTree mapping_tree;
    int error_id = -1;
    std::vector<InputPort *> inputs;
    std::vector<OutputPort *> entries;

    vp::Trace trace;
    vp::StatScalar stat_reads;
    vp::StatScalar stat_writes;
    vp::StatScalar stat_bytes_read;
    vp::StatScalar stat_bytes_written;
    vp::StatScalar stat_errors;
};


//
// OutputPort / InputPort
//

OutputPort::OutputPort(RouterBandwidth *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterBandwidth::retry_muxed, &RouterBandwidth::resp_muxed)
{
}

InputPort::InputPort(RouterBandwidth *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterBandwidth::req_muxed)
{
}


//
// RouterBandwidth
//

RouterBandwidth::RouterBandwidth(vp::ComponentConf &config)
    : vp::Component(config),
      mapping_tree(&this->trace)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->stats.register_stat(&this->stat_reads, "reads", "Number of read requests");
    this->stats.register_stat(&this->stat_writes, "writes", "Number of write requests");
    this->stats.register_stat(&this->stat_bytes_read, "bytes_read", "Total bytes read");
    this->stats.register_stat(&this->stat_bytes_written, "bytes_written", "Total bytes written");
    this->stats.register_stat(&this->stat_errors, "errors", "Requests with no valid mapping");

    this->bandwidth = this->get_js_config()->get_int("bandwidth");
    this->latency = this->get_js_config()->get_int("latency");
    this->shared_rw_bandwidth = this->get_js_config()->get_child_bool("shared_rw_bandwidth");
    this->nb_input_port = this->get_js_config()->get_int("nb_input_port");

    this->inputs.resize(this->nb_input_port);
    for (int i = 0; i < this->nb_input_port; i++)
    {
        std::string name = i == 0 ? "input" : "input_" + std::to_string(i);
        InputPort *in = new InputPort(this, i, name);
        this->inputs[i] = in;
        this->new_slave_port(name, &in->itf, this);
    }

    js::Config *mappings = this->get_js_config()->get("mappings");
    if (mappings != NULL)
    {
        int mapping_id = 0;
        for (auto &mapping : mappings->get_childs())
        {
            js::Config *mapping_config = mapping.second;
            std::string name = mapping.first;

            this->mapping_tree.insert(mapping_id, name, mapping_config);

            OutputPort *out = new OutputPort(this, mapping_id, name);
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

InFlight *RouterBandwidth::alloc_inflight()
{
    InFlight *ifl = this->inflight_free;
    if (ifl) { this->inflight_free = (InFlight *)ifl->saved_initiator; }
    else     { ifl = new InFlight(); }
    return ifl;
}

void RouterBandwidth::free_inflight(InFlight *ifl)
{
    // Reuse saved_initiator as freelist link — it will be overwritten on next alloc.
    ifl->saved_initiator = (void *)this->inflight_free;
    this->inflight_free = ifl;
}

vp::IoReqStatus RouterBandwidth::forward_inline(InputPort *in, vp::IoReq *req,
                                                 OutputPort *out, int64_t now)
{
    uint64_t size = req->get_size();

    // Compute bandwidth waits on both sides. A single initiator could exceed
    // `bandwidth` by spreading traffic across multiple outputs — the per-input
    // watermark prevents that. A single output could be oversubscribed by multiple
    // initiators — the per-output watermark prevents that. The request must wait for
    // whichever is later.
    int64_t wait_in  = in->next_available_cycle  - now;  if (wait_in  < 0) wait_in  = 0;
    int64_t wait_out = out->next_available_cycle - now;  if (wait_out < 0) wait_out = 0;
    int64_t wait = std::max(wait_in, wait_out);

    int64_t burst_duration = 0;
    if (this->bandwidth > 0)
    {
        burst_duration = ((int64_t)size + this->bandwidth - 1) / this->bandwidth;
    }

    // Router and mapping latency are output-side only — they don't stall the input
    // channel.
    int64_t logical_latency = wait + this->latency + out->mapping_latency + burst_duration;

    // Translate the address — the only mutation we do BEFORE the forward, so
    // rollback on DENIED is one line.
    uint64_t original_addr = req->get_addr();
    req->set_addr(original_addr - out->remove_offset + out->add_offset);

    vp::IoReqStatus st = out->itf.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Hot path: no InFlight, no initiator juggling, no watermark save/restore.
        // Just annotate and advance.
        req->inc_latency(logical_latency);
        in->next_available_cycle  = now + wait + burst_duration;
        out->next_available_cycle = now + logical_latency;
        return vp::IO_REQ_DONE;
    }
    if (st == vp::IO_REQ_GRANTED)
    {
        // Install InFlight AFTER the forward. Safe because resp_muxed can't fire
        // until this function returns (same stack).
        InFlight *ifl = this->alloc_inflight();
        ifl->input = in;
        ifl->saved_initiator = req->initiator;
        req->initiator = ifl;
        req->inc_latency(logical_latency);
        in->next_available_cycle  = now + wait + burst_duration;
        out->next_available_cycle = now + logical_latency;
        return vp::IO_REQ_GRANTED;
    }
    // DENIED: restore the addr, mark the output stalled. No watermark advance
    // (nothing actually went through), no InFlight to free, no latency to undo.
    req->set_addr(original_addr);
    out->stalled = true;
    return vp::IO_REQ_DENIED;
}

vp::IoReqStatus RouterBandwidth::req_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterBandwidth *_this = (RouterBandwidth *)__this;
    InputPort *in = _this->inputs[port];
    uint64_t size = req->get_size();
    int64_t now = _this->clock.get_cycles();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Req arrived (input: %d, addr: 0x%lx, size: %lu, write: %d)\n",
        port, req->get_addr(), size, req->get_is_write() ? 1 : 0);

    vp::MappingTreeEntry *mapping = _this->mapping_tree.get(
        req->get_addr(), size, req->get_is_write());
    bool straddles = mapping && mapping->size != 0 &&
        req->get_addr() + size > mapping->base + mapping->size;
    if (!mapping || mapping->id == _this->error_id || straddles ||
        !_this->entries[mapping->id]->itf.is_bound())
    {
        _this->stat_errors++;
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    if (req->get_is_write()) { _this->stat_writes++; _this->stat_bytes_written += size; }
    else                     { _this->stat_reads++;  _this->stat_bytes_read    += size; }

    OutputPort *out = _this->entries[mapping->id];

    // If the output is currently stalled (from a prior DENY that's still pending
    // retry), queue this request and acknowledge the master. We'll forward on retry.
    if (out->stalled)
    {
        in->queue.push_back({req, (int)mapping->id});
        return vp::IO_REQ_GRANTED;
    }

    vp::IoReqStatus st = _this->forward_inline(in, req, out, now);
    if (st == vp::IO_REQ_DENIED)
    {
        // The downstream just stalled our output. forward_inline already marked the
        // output stalled and rolled the annotation back. Absorb the request into the
        // queue so the master sees GRANTED instead of DENIED.
        in->queue.push_back({req, (int)mapping->id});
        return vp::IO_REQ_GRANTED;
    }
    return st;
}

void RouterBandwidth::drain_queue(InputPort *in)
{
    while (!in->queue.empty())
    {
        QueuedReq &q = in->queue.front();
        OutputPort *out = this->entries[q.output_id];
        if (out->stalled) return;   // another DENY on this or later forward

        vp::IoReq *req = q.req;
        in->queue.pop_front();

        int64_t now = this->clock.get_cycles();
        vp::IoReqStatus st = this->forward_inline(in, req, out, now);

        if (st == vp::IO_REQ_DONE)
        {
            // Deliver the (latency-annotated) response to the master.
            in->itf.resp(req);
            continue;
        }
        if (st == vp::IO_REQ_GRANTED)
        {
            // Response will come via resp_muxed.
            continue;
        }
        // DENIED again — put back at head, stop draining.
        in->queue.push_front({req, q.output_id});
        return;
    }
}

void RouterBandwidth::resp_muxed(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    RouterBandwidth *_this = (RouterBandwidth *)__this;
    InFlight *ifl = (InFlight *)req->initiator;
    InputPort *in = ifl->input;
    req->initiator = ifl->saved_initiator;
    _this->free_inflight(ifl);
    in->itf.resp(req);
}

void RouterBandwidth::retry_muxed(vp::Block *__this, int id)
{
    RouterBandwidth *_this = (RouterBandwidth *)__this;
    _this->entries[id]->stalled = false;
    for (InputPort *in : _this->inputs)
    {
        if (!in->queue.empty() && in->queue.front().output_id == id)
        {
            _this->drain_queue(in);
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterBandwidth(config);
}
