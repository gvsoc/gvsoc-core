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
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <math.h>
#include <vp/mapping_tree.hpp>
#include "router_common.hpp"

class Router;


/**
 * @brief Bandwidth limiter
 *
 * This is used on both input and output ports to impact requests latency and durations so that
 * the specified bandwidth is respected in average.
 *
 */
class BandwidthLimiter
{
public:
    // Overall bandwidth to be respected, and global latency to be applied to each request
    BandwidthLimiter(Router *top, int64_t bandwidth, int64_t latency);
    // Can be called on any request going through the limiter to add the fixed latency and impact
    // the latency and duration with the current utilization of the limiter with respect to the
    // bandwidth
    void apply_bandwidth(int64_t cycles, vp::IoReq *req);

private:
    Router *top;
    // Bandwidth in bytes per cycle to be respected
    int64_t bandwidth;
    // Fixed latency to be added to each request
    int64_t latency;
    // Cyclestamp at which the next read burst can go through the limiter. Used to delay a request
    // which arrives before this cyclestamp.
    int64_t next_read_burst_cycle = 0;
    // Cyclestamp at which the write read burst can go through the limiter. Used to delay a request
    // which arrives before this cyclestamp.
    int64_t next_write_burst_cycle = 0;
};



/**
 * @brief OutputPort
 *
 * This represents a possible OutputPort in the memory map.
 * It is used to implement the bandwidth limiter, store the OutputPort interface and other mapping
 * information.
 */
class OutputPort
{
public:
    OutputPort(Router *top, int64_t bandwidth, int64_t latency);
    // OutputPort output IO interface where requests matching the mapping should be forwarded
    vp::IoMaster itf;
    // Bandwidth limiter to impact request timing
    BandwidthLimiter bw_limiter;
    // Offset to be removed when request is forwarded
    uint64_t remove_offset = 0;
    // Offset to be added when request is forwarded
    uint64_t add_offset = 0;
};


class InputPort
{
public:
    InputPort(Router *top, int64_t bandwidth, int64_t latency);
    vp::IoSlave itf;
    BandwidthLimiter bw_limiter;
};


/**
 * @brief Synchronous router
 *
 * This models an AXI-like router that is capable of routing memory-mapped requests from an input
 * port to output ports based on the request address.
 */
class Router : public RouterCommon
{
    friend class BandwidthLimiter;

public:
    Router(vp::ComponentConf &conf);

private:
    // Incoming requests are received here. The port indicates from which input port it is received.
    vp::IoReqStatus handle_req(vp::IoReq *req, int port) override;
    // Interface callback where incoming requests are received. Just a wrapper for handle_req
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req, int port);
    // Asynchronous response are received here in case a request is spread over multiple mappings.
    static void response(vp::Block *__this, vp::IoReq *req);
    // Called to handle the end of a request, either because it was handled synchronously or through
    // the response callback
    void handle_entry_req_end(vp::IoReq *req);

    // Constants giving the position of the temporary arguments stored in the requests.
    static constexpr int REQ_REM_SIZE = 0;
    static constexpr int REQ_CYCLES = 1;
    static constexpr int REQ_LATENCY = 2;
    static constexpr int REQ_DURATION = 3;
    static constexpr int REQ_NB_ARGS = 4;

    // This component trace
    vp::Trace trace;

    // Array of input ports
    std::vector<InputPort *> inputs;
    // Array of output ports
    std::vector<OutputPort *> entries;
    // Tree of mappings
    vp::MappingTree mapping_tree;
    // Gives the ID of the error mapping, the one returning an error when a request is matching
    // this mapping
    int error_id = -1;
};



Router::Router(vp::ComponentConf &config)
    : RouterCommon(config), mapping_tree(&this->trace)
{
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    int bandwidth = this->get_js_config()->get_int("bandwidth");
    int latency = this->get_js_config()->get_int("latency");
    int nb_input_port = this->get_js_config()->get_int("nb_input_port");

    // Instantiates input ports
    this->inputs.resize(nb_input_port);
    for (int i=0; i<nb_input_port; i++)
    {
        InputPort *input_port = new InputPort(this, bandwidth, latency);
        this->inputs[i] = input_port;

        vp::IoSlave *input = &input_port->itf;
        std::string name = i == 0 ? "input" : "input_" + std::to_string(i);
        input->set_req_meth_muxed(&Router::req, i);
        this->new_slave_port(name, input, this);
    }

    // Then mappings and output ports
    js::Config *mappings = this->get_js_config()->get("mappings");
    if (mappings != NULL)
    {
        int mapping_id = 0;
        for (auto &mapping : mappings->get_childs())
        {
            js::Config *config = mapping.second;
            std::string name = mapping.first;

            this->mapping_tree.insert(mapping_id, name, config);

            OutputPort *entry = new OutputPort(this, bandwidth, config->get_int("latency"));

            entry->itf.set_resp_meth(&Router::response);
            this->new_master_port(name, &entry->itf);

            entry->remove_offset = config->get_uint("remove_offset");
            entry->add_offset = config->get_uint("add_offset");

            this->entries.push_back(entry);

            if (name == "error")
            {
                this->error_id = mapping_id;
            }

            mapping_id++;
        }

        this->mapping_tree.build();
    }
}

vp::IoReqStatus Router::req(vp::Block *__this, vp::IoReq *req, int port)
{
    Router *_this = (Router *)__this;
    return _this->handle_req(req, port);
}

vp::IoReqStatus Router::handle_req(vp::IoReq *req, int port)
{
    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool is_write = req->get_is_write();

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        offset, size, is_write);

    // First apply the bandwidth limitation coming from the input port
    this->inputs[port]->bw_limiter.apply_bandwidth(this->clock.get_cycles(), req);

    // Get the mapping from the tree
    vp::MappingTreeEntry *mapping = this->mapping_tree.get(offset, size, req->get_is_write());

    // In case no mapping was found, or we hit the error mapping, return an error
    if (!mapping || mapping->id == this->error_id)
    {
        return vp::IO_REQ_INVALID;
    }

    // First check if we are in the common case where the requests is entirely within the mapping.
    // This can happen either if it is the default one (size equal 0) or base and size fully fits
    // the mapping
    if (mapping->size == 0 || offset + size <= mapping->base + mapping->size)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Routing to entry (OutputPort: %s)\n", mapping->name.c_str());

        OutputPort *entry = this->entries[mapping->id];

        // The mapping may exist and not be connected, return an error in this case
        if (!entry->itf.is_bound())
        {
            this->trace.msg(vp::Trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
                offset, size, is_write);
            return vp::IO_REQ_INVALID;
        }

        // Apply the bandwidth limitation to the output port
        entry->bw_limiter.apply_bandwidth(this->clock.get_cycles(), req);

        // Translate the offset of the output request based on entry information
        req->set_addr(offset - entry->remove_offset + entry->add_offset);

        // And forward to the next target. Note than whatever the kind of response, synchronous
        // or asynchronous, this router is not involved anymore in the request since it is
        // forwarded.
        return entry->itf.req_forward(req);
    }
    else
    {
        // Slow and rare case where we have to split the request into several smaller requests.

        // Consider the result as OK by default, this will be overriden if any of the sub requests
        // is failing
        req->status = vp::IO_REQ_OK;

        // Allocate arguments in the request, they will be used to store information that we will
        // need in the response callback
        req->arg_alloc(Router::REQ_NB_ARGS);
        *(int64_t *)req->arg_get(Router::REQ_REM_SIZE) = size;
        *(int64_t *)req->arg_get(Router::REQ_CYCLES) = this->clock.get_cycles();
        *(int64_t *)req->arg_get(Router::REQ_LATENCY) = 0;

        // Now go through entries matching the access and send one new request for each
        while (size)
        {
            vp::MappingTreeEntry *mapping = this->mapping_tree.get(offset, size, is_write);

            if (!mapping || mapping->id == this->error_id)
            {
                return vp::IO_REQ_INVALID;
            }

            this->trace.msg(vp::Trace::LEVEL_TRACE, "Routing to entry (OutputPort: %s)\n", mapping->name.c_str());

            OutputPort *entry = this->entries[mapping->id];

            if (!entry->itf.is_bound())
            {
                this->trace.msg(vp::Trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
                    offset, size, is_write);
                return vp::IO_REQ_INVALID;
            }

            // Compute the size of the request which falls into this entry
            int iter_size = std::min(mapping->size - (offset - mapping->base), size);

            // We need one new request per entry
            vp::IoReq *entry_req = entry->itf.req_new(
                offset - entry->remove_offset + entry->add_offset, data, iter_size, is_write
            );

            // Allocate one argument and store the parent request in it so that we can update it
            // even if the request is handled asynchronously
            entry_req->arg_alloc(2);
            *(vp::IoReq **)entry_req->arg_get(0) = req;
            *(int *)entry_req->arg_get(1) = mapping->id;

            // Apply the bandwidth limitation to the child request.
            // This is important to update the router bandwidth information and also to impact
            // child request so that we can then impact the parent request
            entry->bw_limiter.apply_bandwidth(this->clock.get_cycles(), entry_req);

            // Now send the request. We cannot forward it since we need to know when the parent
            // request can be replied.
            vp::IoReqStatus status = entry->itf.req(entry_req);
            if (status == vp::IO_REQ_OK || status == vp::IO_REQ_INVALID)
            {
                // In case we receive a synchronous reply, the request is no more needed, it can
                // be accounted and freed
                this->handle_entry_req_end(entry_req);

                // If one child request is failing, the whole parent request is failing
                if (status == vp::IO_REQ_INVALID)
                {
                    req->status = vp::IO_REQ_INVALID;
                }

                entry->itf.req_del(entry_req);
            }

            // Updated current burst to go to next entry
            size -= iter_size;
            offset += iter_size;
            data += iter_size;
        }

        // Either return OK or INVALID if there parent request is over, or PENDING if some child
        // requests are still pending
        if (*(int64_t *)req->arg_get(Router::REQ_REM_SIZE) == 0)
        {
            // Release the argument now that we are done with the request to not disturb the caller
            // if it needs to allocate arguments
            req->arg_free(Router::REQ_NB_ARGS);
            return req->status;
        }
        else
        {
            return vp::IO_REQ_PENDING;
        }
    }
}

void Router::response(vp::Block *__this, vp::IoReq *req)
{
    Router *_this = (Router *)__this;

    // We get here in case the input port request is spread over several and one of the child
    // request was handled asynchronously.

    // Account the request
    _this->handle_entry_req_end(req);

    // And reply to the parent request in case it is over
    vp::IoReq *parent_req = *(vp::IoReq **)req->arg_get(0);

    // If one child request is failing, the whole parent request is failing
    if (req->status == vp::IO_REQ_INVALID)
    {
        parent_req->status = vp::IO_REQ_INVALID;
    }

    int port = *(int *)req->arg_get(1);
    if (*(int64_t *)parent_req->arg_get(Router::REQ_REM_SIZE) == 0)
    {
        // Release the argument now that we are done with the request to not disturb the caller
        // if it needs to allocate arguments
        parent_req->arg_free(Router::REQ_NB_ARGS);
        // Notify the initiator about the response
        parent_req->resp_port->resp(parent_req);
    }

    // Child request is no more needed
    _this->entries[port]->itf.req_del(req);
}

void Router::handle_entry_req_end(vp::IoReq *entry_req)
{
    // This is called when a child request is over and should be accounted on the parent request
    vp::IoReq *req = *(vp::IoReq **)entry_req->arg_get(0);

    // First compute the latency of the request, which is the cycle between now and the time where
    // it was sent
    int64_t latency = this->clock.get_cycles() - *(int64_t *)req->arg_get(Router::REQ_CYCLES);

    // Timing model is that all child requests are sent at the same time and the latency of the
    // parent request is the longest one amongst the child requests.
    *(int64_t *)req->arg_get(Router::REQ_LATENCY) =
        std::max(*(int64_t *)req->arg_get(Router::REQ_LATENCY), latency);

    // The same for the duration
    *(int64_t *)req->arg_get(Router::REQ_DURATION) =
        std::max(*(uint64_t *)req->arg_get(Router::REQ_DURATION), entry_req->get_duration());

    // Finally remove the child size from the parent request
    *(int64_t *)req->arg_get(Router::REQ_REM_SIZE) -= entry_req->get_size();
}

OutputPort::OutputPort(Router *top, int64_t bandwidth, int64_t latency)
    : bw_limiter(top, bandwidth, latency)
{
}

InputPort::InputPort(Router *top, int64_t bandwidth, int64_t latency)
    : bw_limiter(top, bandwidth, latency)
{
}

BandwidthLimiter::BandwidthLimiter(Router *top, int64_t bandwidth, int64_t latency)
{
    this->top = top;
    this->latency = latency;
    this->bandwidth = bandwidth;
}

void BandwidthLimiter::apply_bandwidth(int64_t cycles, vp::IoReq *req)
{
    uint64_t size = req->get_size();

    if (this->bandwidth != 0)
    {
        // Bandwidth was specified

        // Duration in cycles of this burst in this router according to router bandwidth
        int64_t burst_duration = (size + this->bandwidth - 1) / this->bandwidth;

        // Update burst duration
        // This will update it only if it is bigger than the current duration, in case there is a
        // slower router on the path
        req->set_duration(burst_duration);

        // Now we need to compute the start cycle of the burst, which is its latency.
        // First get the cyclestamp where the router becomes available, due to previous requests
        int64_t *next_burst_cycle = req->get_is_write() ?
            &this->next_write_burst_cycle : &this->next_read_burst_cycle;
        int64_t router_latency = *next_burst_cycle - cycles;

        // Then compare that to the request latency and take the highest to properly delay the
        // request in case the bandwidth is reached.
        int64_t latency = std::max((int64_t)req->get_latency(), router_latency);

        // Apply the computed latency and add the fixed one
        req->set_latency(latency + this->latency);

        // Update the bandwidth information by appending the new burst right after the previous one.
        *next_burst_cycle = std::max(cycles, *next_burst_cycle) + burst_duration;

        this->top->trace.msg(vp::Trace::LEVEL_TRACE, "Updating %s burst bandwidth cyclestamp (bandwidth: %d, next_burst: %d)\n",
            req->get_is_write() ? "write" : "read", this->bandwidth, *next_burst_cycle);
    }
    else
    {
        // No bandwidth was specified, just add the specified latency
        req->inc_latency(this->latency);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Router(config);
}
