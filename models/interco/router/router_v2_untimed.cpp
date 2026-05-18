// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Untimed address-decoding router on the io_v2 protocol.
 *
 * No timing model. Every status returned by the downstream (DONE / GRANTED /
 * DENIED) is propagated back to the master unchanged. `retry()` from any output is
 * broadcast to every input — masters that have nothing pending just ignore it.
 * `resp()` uses a minimal InFlight record to route back to the originating input;
 * this record is only allocated on the GRANTED path (the DONE and DENIED paths don't
 * touch initiator).
 *
 * Scope: single-mapping only.
 *
 * Proxy access (gvsoc_control mem_read / mem_write) is supported via the
 * shared helper in "proxy_command.hpp". DENIED replies fall back to a
 * "wait-for-retry then re-dispatch" loop; GRANTED replies block until
 * the variant's resp callback notifies the proxy waiter installed in the
 * forwarded request's InFlight slot.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/mapping_tree.hpp>
#include <vp/signal.hpp>
#include <vp/proxy.hpp>
#include <interco/router_v2/router_config.hpp>
#include <unordered_map>
#include <vector>

#include "proxy_command.hpp"

class RouterUntimed;

class OutputPort
{
public:
    OutputPort(RouterUntimed *top, int id, std::string name);
    void log_access(uint64_t addr, uint64_t size);
    RouterUntimed *top;
    int id;
    vp::IoMaster itf;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
    // Per-mapping VCD traces — show pre-translation addr / size of each request
    // forwarded through this output. Multiple accesses in the same cycle are
    // spread by a sub-cycle delay so the GUI can show them all.
    vp::Signal<uint64_t> current_addr;
    vp::Signal<uint64_t> current_size;
    int64_t last_logged_access = -1;
    int nb_logged_access_in_same_cycle = 0;
};

class InputPort
{
public:
    InputPort(RouterUntimed *top, int id, std::string name);
    RouterUntimed *top;
    int id;
    vp::IoSlave itf;
};

struct InFlight
{
    InputPort *input;
    // Free-list link used while parked in inflight_free; meaningless once
    // alloc_inflight() hands the entry out.
    InFlight *next_free;
    // Non-null for proxy-originated requests: resp_muxed signals this
    // ProxyWaiter instead of calling back the master via input->itf.
    vp_router_v2_proxy::ProxyWaiter *proxy_waiter = nullptr;
};

class RouterUntimed : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterUntimed(vp::ComponentConf &conf);
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file,
        FILE *reply_file, std::vector<std::string> args,
        std::string cmd_req) override;

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int id);
    static void retry_muxed(vp::Block *__this, int id);

    // Mapping + forward used by handle_command for proxy mem_read/mem_write.
    // On IO_REQ_GRANTED, installs an InFlight whose `proxy_waiter` is the
    // ProxyWaiter passed in, so resp_muxed can route the response back to
    // the proxy thread waiting on it.
    vp::IoReqStatus dispatch_proxy_req(vp::IoReq *req,
        vp_router_v2_proxy::ProxyWaiter *waiter);

    InFlight *alloc_inflight();
    void free_inflight(InFlight *ifl);
    InFlight *inflight_free = nullptr;
    // Per-router in-flight map. A previous design stashed the InFlight* in
    // req->initiator, but that breaks two ways: (a) the iDMA's idma_be_axi
    // back-end uses req->initiator to carry its own BurstInfo pointer; the
    // untimed router stomped on it and only restored on the first
    // resp_muxed, so a subsequent resp_muxed for the same req (asymmetric
    // read case where one forward yields N response beats) dereferenced the
    // restored iDMA pointer as an InFlight* and crashed. (b) Even without
    // that consumer, two cascaded routers that both stashed into
    // req->initiator clobbered each other. Keying by req* sidesteps both.
    std::unordered_map<vp::IoReq *, InFlight *> in_flight_map;

    // Set by handle_proxy_command while it is waiting for the next
    // retry_muxed broadcast after a DENIED. Cleared right after.
    vp_router_v2_proxy::ProxyWaiter *proxy_retry_waiter = nullptr;
    std::mutex proxy_retry_mu;

    RouterConfig cfg;
    vp::MappingTree mapping_tree;
    int error_id = -1;
    std::vector<InputPort *> inputs;
    std::vector<OutputPort *> entries;

    vp::Trace trace;
};


OutputPort::OutputPort(RouterUntimed *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterUntimed::retry_muxed, &RouterUntimed::resp_muxed),
      current_addr(*top, name + "/addr", 64, vp::SignalCommon::ResetKind::HighZ),
      current_size(*top, name + "/size", 64, vp::SignalCommon::ResetKind::HighZ)
{
}

void OutputPort::log_access(uint64_t addr, uint64_t size)
{
    int64_t cycles = this->top->clock.get_cycles();
    if (cycles > this->last_logged_access)
        this->nb_logged_access_in_same_cycle = 0;

    int64_t delay = 0;
    if (this->nb_logged_access_in_same_cycle > 0)
    {
        int64_t period = this->top->clock.get_period();
        delay = period - (period >> this->nb_logged_access_in_same_cycle);
    }
    this->current_addr.set_and_release(addr, 0, delay);
    this->current_size.set_and_release(size, 0, delay);
    this->nb_logged_access_in_same_cycle++;
    this->last_logged_access = cycles;
}

InputPort::InputPort(RouterUntimed *top, int id, std::string name)
    : top(top), id(id),
      itf(id, &RouterUntimed::req_muxed)
{
}


RouterUntimed::RouterUntimed(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      mapping_tree(&this->trace)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->inputs.resize(this->cfg.nb_input_port);
    for (int i = 0; i < this->cfg.nb_input_port; i++)
    {
        std::string name = i == 0 ? "input" : "input_" + std::to_string(i);
        InputPort *in = new InputPort(this, i, name);
        this->inputs[i] = in;
        this->new_slave_port(name, &in->itf, this);
    }

    for (int mapping_id = 0; mapping_id < (int)this->cfg.mappings_count; mapping_id++)
    {
        const RouterMapping &m = this->cfg.mappings[mapping_id];
        std::string name = m.name ? m.name : "";

        this->mapping_tree.insert(mapping_id, name, m.base, m.size);

        OutputPort *out = new OutputPort(this, mapping_id, name);
        out->remove_offset = m.remove_offset;
        out->add_offset = m.add_offset;
        this->entries.push_back(out);
        this->new_master_port(name, &out->itf);

        if (m.is_error) this->error_id = mapping_id;
    }
    this->mapping_tree.build();
}

InFlight *RouterUntimed::alloc_inflight()
{
    InFlight *ifl = this->inflight_free;
    if (ifl) { this->inflight_free = ifl->next_free; }
    else     { ifl = new InFlight(); }
    ifl->proxy_waiter = nullptr;
    return ifl;
}

void RouterUntimed::free_inflight(InFlight *ifl)
{
    ifl->next_free = this->inflight_free;
    this->inflight_free = ifl;
}

vp::IoReqStatus RouterUntimed::req_muxed(vp::Block *__this, vp::IoReq *req, int port)
{
    RouterUntimed *_this = (RouterUntimed *)__this;
    InputPort *in = _this->inputs[port];
    uint64_t size = req->get_size();

    vp::MappingTreeEntry *mapping = _this->mapping_tree.get(
        req->get_addr(), size, req->get_is_write());
    bool straddles = mapping && mapping->size != 0 &&
        req->get_addr() + size > mapping->base + mapping->size;
    if (!mapping || mapping->id == _this->error_id || straddles ||
        !_this->entries[mapping->id]->itf.is_bound())
    {
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }

    OutputPort *out = _this->entries[mapping->id];
    uint64_t original_addr = req->get_addr();
    out->log_access(original_addr, size);
    req->set_addr(original_addr - out->remove_offset + out->add_offset);

    vp::IoReqStatus st = out->itf.req(req);

    if (st == vp::IO_REQ_DONE)
    {
        // Hot path: no InFlight, no initiator touch.
        return vp::IO_REQ_DONE;
    }
    if (st == vp::IO_REQ_GRANTED)
    {
        // Install InFlight after the forward so resp_muxed can route back. Safe:
        // resp_muxed won't fire until we return.
        InFlight *ifl = _this->alloc_inflight();
        ifl->input = in;
        _this->in_flight_map[req] = ifl;
        return vp::IO_REQ_GRANTED;
    }
    // DENIED: propagate to master unchanged. Master handles the protocol from here.
    // Restore address since target rejected the req.
    req->set_addr(original_addr);
    return vp::IO_REQ_DENIED;
}

void RouterUntimed::resp_muxed(vp::Block *__this, vp::IoReq *req, int /*id*/)
{
    RouterUntimed *_this = (RouterUntimed *)__this;
    auto it = _this->in_flight_map.find(req);
    vp_assert(it != _this->in_flight_map.end(), &_this->trace,
        "resp_muxed: no in-flight entry for req=%p\n", req);
    InFlight *ifl = it->second;
    vp_router_v2_proxy::ProxyWaiter *waiter = ifl->proxy_waiter;
    InputPort *in = ifl->input;
    // For asymmetric reads (1 forward → N response beats) the same req object
    // visits us multiple times; only retire the slot on the last beat.
    if (req->is_last)
    {
        _this->in_flight_map.erase(it);
        _this->free_inflight(ifl);
    }
    if (waiter)
    {
        vp_router_v2_proxy::notify_replied(waiter);
    }
    else
    {
        in->itf.resp(req);
    }
}

void RouterUntimed::retry_muxed(vp::Block *__this, int /*id*/)
{
    RouterUntimed *_this = (RouterUntimed *)__this;
    // Broadcast to every input. Masters with nothing held just ignore it.
    for (InputPort *in : _this->inputs)
    {
        in->itf.retry();
    }
    // Wake any proxy thread that was waiting for retry after a DENIED.
    vp_router_v2_proxy::ProxyWaiter *waiter = nullptr;
    {
        std::lock_guard<std::mutex> lk(_this->proxy_retry_mu);
        waiter = _this->proxy_retry_waiter;
        _this->proxy_retry_waiter = nullptr;
    }
    if (waiter)
    {
        vp_router_v2_proxy::notify_retry(waiter);
    }
}

vp::IoReqStatus RouterUntimed::dispatch_proxy_req(vp::IoReq *req,
    vp_router_v2_proxy::ProxyWaiter *waiter)
{
    uint64_t addr = req->get_addr();
    uint64_t size = req->get_size();
    vp::MappingTreeEntry *mapping = this->mapping_tree.get(
        addr, size, req->get_is_write());
    bool straddles = mapping && mapping->size != 0 &&
        addr + size > mapping->base + mapping->size;
    if (!mapping || mapping->id == this->error_id || straddles ||
        !this->entries[mapping->id]->itf.is_bound())
    {
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
    }
    OutputPort *out = this->entries[mapping->id];
    uint64_t original_addr = addr;
    req->set_addr(addr - out->remove_offset + out->add_offset);
    vp::IoReqStatus st = out->itf.req(req);

    if (st == vp::IO_REQ_GRANTED)
    {
        InFlight *ifl = this->alloc_inflight();
        ifl->input = nullptr;
        ifl->proxy_waiter = waiter;
        this->in_flight_map[req] = ifl;
        return st;
    }
    if (st == vp::IO_REQ_DENIED)
    {
        // Target rejected the req; restore the address for the next try.
        req->set_addr(original_addr);
    }
    return st;
}

std::string RouterUntimed::handle_command(gv::GvProxy *proxy, FILE *req_file,
    FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    return vp_router_v2_proxy::handle_proxy_command(
        proxy, this->get_launcher(), req_file, reply_file, args, cmd_req,
        [this](vp::IoReq *r, vp_router_v2_proxy::ProxyWaiter *w) {
            return this->dispatch_proxy_req(r, w);
        },
        [this](vp_router_v2_proxy::ProxyWaiter *w) {
            std::lock_guard<std::mutex> lk(this->proxy_retry_mu);
            this->proxy_retry_waiter = w;
        });
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterUntimed(config);
}
