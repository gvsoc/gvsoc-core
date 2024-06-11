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
#include <vp/itf/wire.hpp>
#include <vp/proxy.hpp>
#include <stdio.h>
#include <math.h>
#include <vp/mapping_tree.hpp>

class router;

class Perf_counter
{
public:
    int64_t nb_read;
    int64_t nb_write;
    int64_t read_stalls;
    int64_t write_stalls;

    vp::WireSlave<uint32_t> nb_read_itf;
    vp::WireSlave<uint32_t> nb_write_itf;
    vp::WireSlave<uint32_t> read_stalls_itf;
    vp::WireSlave<uint32_t> write_stalls_itf;
    vp::WireSlave<uint32_t> stalls_itf;

    static void nb_read_sync_back(vp::Block *__this, uint32_t *value);
    static void nb_read_sync(vp::Block *__this, uint32_t value);
    static void nb_write_sync_back(vp::Block *__this, uint32_t *value);
    static void nb_write_sync(vp::Block *__this, uint32_t value);
    static void read_stalls_sync_back(vp::Block *__this, uint32_t *value);
    static void read_stalls_sync(vp::Block *__this, uint32_t value);
    static void write_stalls_sync_back(vp::Block *__this, uint32_t *value);
    static void write_stalls_sync(vp::Block *__this, uint32_t value);
    static void stalls_sync_back(vp::Block *__this, uint32_t *value);
    static void stalls_sync(vp::Block *__this, uint32_t value);
};

class MapEntry
{
public:
    vp::IoMaster itf;
    uint64_t remove_offset = 0;
    uint64_t add_offset = 0;
        uint32_t latency = 0;
        int64_t next_read_packet_time = 0;
        int64_t next_write_packet_time = 0;
};

class router : public vp::Component
{
public:
    router(vp::ComponentConf &conf);

    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    static void grant(vp::Block *__this, vp::IoReq *req);

    static void response(vp::Block *__this, vp::IoReq *req);

private:
    vp::Trace trace;

    vp::IoMaster out;
    vp::IoSlave in;
    vp::MappingTree mapping_tree;
    std::vector<MapEntry *> entries;

    std::map<int, Perf_counter *> counters;

    int bandwidth = 0;
    int latency = 0;
    vp::IoReq proxy_req;
    int error_id = -1;
};

router::router(vp::ComponentConf &config)
    : vp::Component(config), mapping_tree(&this->trace)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in.set_req_meth(&router::req);
    new_slave_port("input", &in);

    out.set_resp_meth(&router::response);
    out.set_grant_meth(&router::grant);
    new_master_port("out", &out);

    bandwidth = get_js_config()->get_child_int("bandwidth");
    latency = get_js_config()->get_child_int("latency");

    js::Config *mappings = get_js_config()->get("mappings");

    this->proxy_req.set_data(new uint8_t[4]);

    if (mappings != NULL)
    {
        int mapping_id = 0;
        for (auto &mapping : mappings->get_childs())
        {
            js::Config *config = mapping.second;

            this->mapping_tree.insert(mapping_id, mapping.first, config);

            MapEntry *entry = new MapEntry();

            entry->itf.set_resp_meth(&router::response);
            entry->itf.set_grant_meth(&router::grant);
            new_master_port(mapping.first, &entry->itf);

            entry->remove_offset = config->get_uint("remove_offset");
            entry->add_offset = config->get_uint("add_offset");

            entries.push_back(entry);

            if (mapping.first == "error")
            {
                this->error_id = mapping_id;
            }

            if (this->counters.find(mapping_id) == this->counters.end())
            {
                Perf_counter *counter = new Perf_counter();
                this->counters[mapping_id] = counter;

                counter->nb_read_itf.set_sync_back_meth(&Perf_counter::nb_read_sync_back);
                counter->nb_read_itf.set_sync_meth(&Perf_counter::nb_read_sync);
                new_slave_port("nb_read[" + std::to_string(mapping_id) + "]", &counter->nb_read_itf, (void *)counter);

                counter->nb_write_itf.set_sync_back_meth(&Perf_counter::nb_write_sync_back);
                counter->nb_write_itf.set_sync_meth(&Perf_counter::nb_write_sync);
                new_slave_port("nb_write[" + std::to_string(mapping_id) + "]", &counter->nb_write_itf, (void *)counter);

                counter->read_stalls_itf.set_sync_back_meth(&Perf_counter::read_stalls_sync_back);
                counter->read_stalls_itf.set_sync_meth(&Perf_counter::read_stalls_sync);
                new_slave_port("read_stalls[" + std::to_string(mapping_id) + "]", &counter->read_stalls_itf, (void *)counter);

                counter->write_stalls_itf.set_sync_back_meth(&Perf_counter::write_stalls_sync_back);
                counter->write_stalls_itf.set_sync_meth(&Perf_counter::write_stalls_sync);
                new_slave_port("write_stalls[" + std::to_string(mapping_id) + "]", &counter->write_stalls_itf, (void *)counter);

                counter->stalls_itf.set_sync_back_meth(&Perf_counter::stalls_sync_back);
                counter->stalls_itf.set_sync_meth(&Perf_counter::stalls_sync);
                new_slave_port("stalls[" + std::to_string(mapping_id) + "]", &counter->stalls_itf, (void *)counter);
            }

            mapping_id++;
        }

        this->mapping_tree.build();
    }
}

vp::IoReqStatus router::req(vp::Block *__this, vp::IoReq *req)
{
    router *_this = (router *)__this;

    // Since this is a a synchronous router, we will iterate sending requests to entries until
    // the whole requests has been processed

    // Consider the result as OK by default, this will be overriden if any of the sub requests
    // is failing
    vp::IoReqStatus result = vp::IO_REQ_OK;

    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool is_write = req->get_is_write();

    // Save the requests  information as we will need to restore them at the end since we will
    // use the request for smaller requests
    uint64_t req_size = size;
    uint64_t req_offset = offset;
    uint8_t *req_data = data;

    while (size)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d, bandwidth: %d)\n",
                         offset, size, req->get_is_write(), _this->bandwidth);

        vp::MappingTreeEntry *mapping = _this->mapping_tree.get(offset, size, req->get_is_write());

        if (!mapping || mapping->id == _this->error_id)
        {
            return vp::IO_REQ_INVALID;
        }

        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Routing to entry (target: %s)\n", mapping->name.c_str());

        MapEntry *entry = _this->entries[mapping->id];

        if (_this->bandwidth != 0)
        {
            // Duration of this packet in this router according to router bandwidth
            int64_t packet_duration = (size + _this->bandwidth - 1) / _this->bandwidth;

            // Update packet duration
            // This will update it only if it is bigger than the current duration, in case there is a
            // slower router on the path
            req->set_duration(packet_duration);

            // Update the request latency.
            int64_t latency = req->get_latency();
            // First check if the latency should be increased due to bandwidth
            int64_t *next_packet_time = req->get_is_write() ? &entry->next_write_packet_time : &entry->next_read_packet_time;
            int64_t router_latency = *next_packet_time - _this->clock.get_cycles();
            if (router_latency > latency)
            {
                latency = router_latency;
            }

            // Then apply the router latency
            req->set_latency(latency + entry->latency + _this->latency);

            // Update the bandwidth information
            int64_t router_time = _this->clock.get_cycles();
            if (router_time < *next_packet_time)
            {
                router_time = *next_packet_time;
            }
            *next_packet_time = router_time + packet_duration;
        }
        else
        {
            req->inc_latency(entry->latency + _this->latency);
        }

        int iter_size = mapping->size - (offset - mapping->base);

        if (iter_size > size)
        {
            iter_size = size;
        }

        // Forward the request to the target port
        req->set_addr(offset - entry->remove_offset + entry->add_offset);
        req->set_size(iter_size);
        req->set_data(data);
        req->set_addr(offset - entry->remove_offset);

        if (!entry->itf.is_bound())
        {
            _this->trace.msg(vp::Trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
                offset, size, is_write);
            return vp::IO_REQ_INVALID;
        }

        req->arg_push(req->resp_port);
        result = entry->itf.req(req);
        if (result == vp::IO_REQ_OK)
        {
            req->arg_pop();
        }
        else
        {
            if (result == vp::IO_REQ_INVALID)
            {
                req->arg_pop();
                break;
            }
            else
            {
                if (iter_size != size)
                {
                    _this->trace.force_warning("Unsupported asynchronous response in synchronous router (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size,
                        is_write);
                }
            }
        }


        int64_t latency = req->get_latency();
        int64_t duration = req->get_duration();
        if (duration > 1)
            latency += duration - 1;

        Perf_counter *counter = _this->counters[mapping->id];

        if (!is_write)
            counter->read_stalls += latency;
        else
            counter->write_stalls += latency;

        if (!is_write)
            counter->nb_read++;
        else
            counter->nb_write++;

        size -= iter_size;
        offset += iter_size;
        data += iter_size;
    }

    // Restore request information now that we are done
    if (result == vp::IO_REQ_OK)
    {
        req->set_addr(req_offset);
        req->set_size(req_size);
        req->set_data(req_data);
    }

    return result;
}

void router::grant(vp::Block *__this, vp::IoReq *req)
{
    vp::IoSlave *port = (vp::IoSlave *)req->arg_pop();
    port->grant(req);
    req->arg_push(port);
}

void router::response(vp::Block *__this, vp::IoReq *req)
{
    vp::IoSlave *port = (vp::IoSlave *)req->arg_pop();
    port->resp(req);
}

std::string router::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    if (args[0] == "mem_write" || args[0] == "mem_read")
    {
        int error = 0;
        bool is_write = args[0] == "mem_write";
        long long int addr = strtoll(args[1].c_str(), NULL, 0);
        long long int size = strtoll(args[2].c_str(), NULL, 0);

        uint8_t *buffer = new uint8_t[size];

        if (is_write)
        {
            int read_size = fread(buffer, 1, size, req_file);
            if (read_size != size)
            {
                error = 1;
            }
        }

        vp::IoReq *req = &this->proxy_req;
        req->init();
        req->set_data((uint8_t *)buffer);
        req->set_is_write(is_write);
        req->set_size(size);
        req->set_addr(addr);
        req->set_debug(true);

        vp::IoReqStatus result = router::req(this, req);
        error |= result != vp::IO_REQ_OK;

        if (!is_write)
        {
            error = proxy->send_payload(reply_file, cmd_req, buffer, size);
        }

        delete[] buffer;

        return "err=" + std::to_string(error);
    }
    return "err=1";
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new router(config);
}

void Perf_counter::nb_read_sync_back(vp::Block *__this, uint32_t *value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    *value = _this->nb_read;
}

void Perf_counter::nb_read_sync(vp::Block *__this, uint32_t value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    _this->nb_read = value;
}

void Perf_counter::nb_write_sync_back(vp::Block *__this, uint32_t *value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    *value = _this->nb_write;
}

void Perf_counter::nb_write_sync(vp::Block *__this, uint32_t value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    _this->nb_write = value;
}

void Perf_counter::read_stalls_sync_back(vp::Block *__this, uint32_t *value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    *value = _this->read_stalls;
}

void Perf_counter::read_stalls_sync(vp::Block *__this, uint32_t value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    _this->read_stalls = value;
}

void Perf_counter::write_stalls_sync_back(vp::Block *__this, uint32_t *value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    *value = _this->write_stalls;
}

void Perf_counter::write_stalls_sync(vp::Block *__this, uint32_t value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    _this->write_stalls = value;
}

void Perf_counter::stalls_sync_back(vp::Block *__this, uint32_t *value)
{
    Perf_counter *_this = (Perf_counter *)__this;

    *value = _this->read_stalls + _this->write_stalls;
}

void Perf_counter::stalls_sync(vp::Block *__this, uint32_t value)
{
    Perf_counter *_this = (Perf_counter *)__this;
    _this->read_stalls = value;
    _this->write_stalls = value;
}
