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

class router;

class Perf_counter {
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

class MapEntry {
public:
  MapEntry() {}
  MapEntry(unsigned long long base, MapEntry *left, MapEntry *right);

  void insert(router *router);

  string target_name;
  MapEntry *next = NULL;
  int id = -1;
  unsigned long long base = 0;
  unsigned long long lowestBase = 0;
  unsigned long long size = 0;
  unsigned long long remove_offset = 0;
  unsigned long long add_offset = 0;
  uint32_t latency = 0;
  int64_t next_read_packet_time = 0;
  int64_t next_write_packet_time = 0;
  MapEntry *left = NULL;
  MapEntry *right = NULL;
  vp::IoSlave *port = NULL;
  vp::IoMaster *itf = NULL;
};

class io_master_map : public vp::IoMaster
{

  inline void bind_to(vp::Port *port, js::Config *config);

};

class router : public vp::Component
{

  friend class MapEntry;

public:

  router(vp::ComponentConf &conf);

  std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req);

  static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);


  static void grant(vp::Block *__this, vp::IoReq *req);

  static void response(vp::Block *__this, vp::IoReq *req);

private:
  vp::Trace     trace;

  io_master_map out;
  vp::IoSlave in;
  bool init = false;

  void init_entries();
  MapEntry *firstMapEntry = NULL;
  MapEntry *defaultMapEntry = NULL;
  MapEntry *errorMapEntry = NULL;
  MapEntry *topMapEntry = NULL;
  MapEntry *externalBindingMapEntry = NULL;

  std::map<int, Perf_counter *> counters;

  int bandwidth = 0;
  int latency = 0;
  vp::IoReq proxy_req;
};

router::router(vp::ComponentConf &config)
: vp::Component(config)
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
    for (auto& mapping: mappings->get_childs())
    {
      js::Config *config = mapping.second;

      MapEntry *entry = new MapEntry();
      entry->target_name = mapping.first;

      vp::IoMaster *itf = new vp::IoMaster();

      itf->set_resp_meth(&router::response);
      itf->set_grant_meth(&router::grant);
      new_master_port(mapping.first, itf);

      if (mapping.first == "error")
        entry->itf = NULL;
      else
        entry->itf = itf;

      js::Config *conf;
      conf = config->get("base");
      if (conf) entry->base = conf->get_int();
      conf = config->get("size");
      if (conf) entry->size = conf->get_int();
      conf = config->get("remove_offset");
      if (conf) entry->remove_offset = conf->get_int();
      conf = config->get("add_offset");
      if (conf) entry->add_offset = conf->get_int();
      conf = config->get("latency");
      if (conf) entry->latency = conf->get_int();
      conf = config->get("id");
      if (conf) entry->id = conf->get_int();

      if (this->counters.find(entry->id) == this->counters.end())
      {
        Perf_counter *counter = new Perf_counter();
        this->counters[entry->id] = counter;

        counter->nb_read_itf.set_sync_back_meth(&Perf_counter::nb_read_sync_back);
        counter->nb_read_itf.set_sync_meth(&Perf_counter::nb_read_sync);
        new_slave_port("nb_read[" + std::to_string(entry->id) + "]", &counter->nb_read_itf, (void *)counter);

        counter->nb_write_itf.set_sync_back_meth(&Perf_counter::nb_write_sync_back);
        counter->nb_write_itf.set_sync_meth(&Perf_counter::nb_write_sync);
        new_slave_port("nb_write[" + std::to_string(entry->id) + "]", &counter->nb_write_itf, (void *)counter);

        counter->read_stalls_itf.set_sync_back_meth(&Perf_counter::read_stalls_sync_back);
        counter->read_stalls_itf.set_sync_meth(&Perf_counter::read_stalls_sync);
        new_slave_port("read_stalls[" + std::to_string(entry->id) + "]", &counter->read_stalls_itf, (void *)counter);

        counter->write_stalls_itf.set_sync_back_meth(&Perf_counter::write_stalls_sync_back);
        counter->write_stalls_itf.set_sync_meth(&Perf_counter::write_stalls_sync);
        new_slave_port("write_stalls[" + std::to_string(entry->id) + "]", &counter->write_stalls_itf, (void *)counter);

        counter->stalls_itf.set_sync_back_meth(&Perf_counter::stalls_sync_back);
        counter->stalls_itf.set_sync_meth(&Perf_counter::stalls_sync);
        new_slave_port("stalls[" + std::to_string(entry->id) + "]", &counter->stalls_itf, (void *)counter);
      }  

      entry->insert(this);
    }
  }

}

MapEntry::MapEntry(unsigned long long base, MapEntry *left, MapEntry *right) : next(NULL), base(base), lowestBase(left->lowestBase), left(left), right(right) {
}

void MapEntry::insert(router *router)
{
  lowestBase = base;

  if (size != 0) {
    if (port != NULL || itf != NULL) {    
      MapEntry *current = router->firstMapEntry;
      MapEntry *prev = NULL;

      while(current && current->base < base) {
        prev = current;
        current = current->next;
      }
      
      if (prev == NULL) {
        next = router->firstMapEntry;
        router->firstMapEntry = this;
      } else {
        next = current;
        prev->next = this;
      }
    } else {
      router->errorMapEntry = this;
    }
  } else {
    router->defaultMapEntry = this;
  }
}

vp::IoReqStatus router::req(vp::Block *__this, vp::IoReq *req)
{
  router *_this = (router *)__this;
  vp::IoReqStatus result;
  
  if (!_this->init)
  {
    _this->init = true;
    _this->init_entries();
  }

  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  uint64_t req_size = size;
  uint64_t req_offset = offset;
  uint8_t *req_data = data;

  int count = 0;
  while (size)
  {
    MapEntry *entry = _this->topMapEntry;
    bool isRead = !req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received IO req (offset: 0x%llx, size: 0x%llx, isRead: %d, bandwidth: %d)\n",
        offset, size, isRead, _this->bandwidth);

    if (entry)
    {
      while(1) {
      // The entry does not have any child, this means we are at a final entry
        if (entry->left == NULL) break;

        if (offset >= entry->base) entry = entry->right;
        else entry = entry->left;
      }

      if (entry && (offset < entry->base || offset > entry->base + entry->size - 1)) {
        entry = NULL;
      }
    }

    if (!entry) {
      if (_this->errorMapEntry && offset >= _this->errorMapEntry->base && offset + size - 1 <= _this->errorMapEntry->base + _this->errorMapEntry->size - 1) {
      } else {
        entry = _this->defaultMapEntry;
      }
    }

    if (!entry) {
      //_this->trace.msg(&warning, "Invalid access (offset: 0x%llx, size: 0x%llx, isRead: %d)\n", offset, size, isRead);
      return vp::IO_REQ_INVALID;
    }

    if (entry == _this->defaultMapEntry) {
      _this->trace.msg(vp::Trace::LEVEL_TRACE, "Routing to default entry (target: %s)\n", entry->target_name.c_str());
    } else {
      _this->trace.msg(vp::Trace::LEVEL_TRACE, "Routing to entry (target: %s)\n", entry->target_name.c_str());
    }
    
    if (!req->is_debug())
    {
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
    }

    int iter_size = entry == _this->defaultMapEntry ? size : entry->size - (offset - entry->base);

    if (iter_size > size)
    {
      iter_size = size;
    }

    // Forward the request to the target port
    req->set_addr(offset);
    req->set_size(iter_size);
    req->set_data(data);
    if (entry->remove_offset) req->set_addr(offset - entry->remove_offset);
    if (entry->add_offset) req->set_addr(offset + entry->add_offset);

    result = vp::IO_REQ_OK;
    if (entry->port)
    {
      req->arg_push(NULL);
      result = _this->out.req(req, entry->port);
      if (result == vp::IO_REQ_OK)
        req->arg_pop();
    }
    else if (entry->itf)
    {
      if (!entry->itf->is_bound())
      {
        _this->trace.msg(vp::Trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, !isRead);
        return vp::IO_REQ_INVALID;
      }
      req->arg_push(req->resp_port);
      result = entry->itf->req(req);
      if (result == vp::IO_REQ_OK)
        req->arg_pop();
    }

    if (entry->id != -1) 
    {
      int64_t latency = req->get_latency();
      int64_t duration = req->get_duration();
      if (duration > 1) latency += duration - 1;

      Perf_counter *counter = _this->counters[entry->id];

      if (isRead)
        counter->read_stalls += latency;
      else
        counter->write_stalls += latency;
    
      if (isRead)
        counter->nb_read++;
      else
        counter->nb_write++;

    }

    size -= iter_size;
    offset += iter_size;
    data += iter_size;
  }

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
  router *_this = (router *)__this;

  vp::IoSlave *port = (vp::IoSlave *)req->arg_pop();
  if (port != NULL)
  {
    port->grant(req);
  }

  req->arg_push(port);
}

void router::response(vp::Block *__this, vp::IoReq *req)
{
  vp::IoSlave *port = (vp::IoSlave *)req->arg_pop();
  if (port != NULL)
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




#define max(a, b) ((a) > (b) ? (a) : (b))

void router::init_entries() {

  MapEntry *current = firstMapEntry;
  trace.msg(vp::Trace::LEVEL_INFO, "Building router table\n");
  while(current) {
    trace.msg(vp::Trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> %s\n", current->base, current->base + current->size, current->target_name.c_str());
    current = current->next;
  }
  if (errorMapEntry != NULL) {
    trace.msg(vp::Trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> ERROR\n", errorMapEntry->base, errorMapEntry->base + errorMapEntry->size);
  }
  if (defaultMapEntry != NULL) {
    trace.msg(vp::Trace::LEVEL_INFO, "       -     :      -     -> %s\n", defaultMapEntry->target_name.c_str());
  }

  MapEntry *firstInLevel = firstMapEntry;

  // Loop until we merged everything into a single entry
  // Start with the routing table entries
  current = firstMapEntry;

  // The first loop is here to loop until we have merged everything to a single entry
  while(1) {

    // Gives the map entry that should be on the left side of the next created entry
    MapEntry *left = NULL;

    // Gives the last allocated entry where the entry should be inserted
    MapEntry *currentInLevel=NULL;

    // The second loop is iterating on a single level to merge entries 2 by 2
    while(current) {
      if (left == NULL) left = current;
      else {

        MapEntry *entry = new MapEntry(current->lowestBase, left, current);

        left = NULL;
        if (currentInLevel) {
          currentInLevel->next = entry;
        } else {
          firstInLevel = entry;
        }
        currentInLevel = entry;
      }

      current = current->next;
    }

    current = firstInLevel;

    // Stop in case we got a single entry
    if (currentInLevel == NULL) break;

    // In case an entry is alone, insert it at the end, it will be merged with an upper entry
    if (left != NULL) {
      currentInLevel->next = left;
    }
  }

  topMapEntry = firstInLevel;
}

inline void io_master_map::bind_to(vp::Port *_port, js::Config *config)
{
  MapEntry *entry = new MapEntry();
  js::Config *conf;
  entry->port = (vp::IoSlave *)_port;
  if (config)
  {
    conf = config->get("base");
    if (conf) entry->base = conf->get_int();
    conf = config->get("size");
    if (conf) entry->size = conf->get_int();
    conf = config->get("remove_offset");
    if (conf) entry->remove_offset = conf->get_int();
    conf = config->get("add_offset");
    if (conf) entry->add_offset = conf->get_int();
    conf = config->get("latency");
    if (conf) entry->latency = conf->get_int();
  }
  entry->insert((router *)get_comp());
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
