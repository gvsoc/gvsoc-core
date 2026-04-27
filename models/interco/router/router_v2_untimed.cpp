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
 * Scope: single-mapping only. No proxy/debug access.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/mapping_tree.hpp>
#include <vp/signal.hpp>
#include <interco/router_v2/router_config.hpp>

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
    void *saved_initiator;
};

class RouterUntimed : public vp::Component
{
    friend class InputPort;
    friend class OutputPort;

public:
    RouterUntimed(vp::ComponentConf &conf);

private:
    static vp::IoReqStatus req_muxed(vp::Block *__this, vp::IoReq *req, int port);
    static void resp_muxed(vp::Block *__this, vp::IoReq *req, int id);
    static void retry_muxed(vp::Block *__this, int id);

    InFlight *alloc_inflight();
    void free_inflight(InFlight *ifl);
    InFlight *inflight_free = nullptr;

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
    if (ifl) { this->inflight_free = (InFlight *)ifl->saved_initiator; }
    else     { ifl = new InFlight(); }
    return ifl;
}

void RouterUntimed::free_inflight(InFlight *ifl)
{
    ifl->saved_initiator = (void *)this->inflight_free;
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
        ifl->saved_initiator = req->initiator;
        req->initiator = ifl;
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
    InFlight *ifl = (InFlight *)req->initiator;
    InputPort *in = ifl->input;
    req->initiator = ifl->saved_initiator;
    _this->free_inflight(ifl);
    in->itf.resp(req);
}

void RouterUntimed::retry_muxed(vp::Block *__this, int /*id*/)
{
    RouterUntimed *_this = (RouterUntimed *)__this;
    // Broadcast to every input. Masters with nothing held just ignore it.
    for (InputPort *in : _this->inputs)
    {
        in->itf.retry();
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new RouterUntimed(config);
}
