/*
 * Copyright (C) 2023 GreenWaves Technologies, SAS, ETH Zurich and
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
 *          Nazareno Bruschi, UniBo (nazareno.bruschi@unibo.it)
 */


#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>

#define MASK_OFFSET_SLAVES  0
#define MASK_OFFSET_MASTERS 32

class MapEntry;

struct Event {
  // Link to the req to enqueue at t+l+d
  vp::io_req *req;
  // Port of incoming request (slave port)
  int port;
  // Timestamp (ticks from 0)
  std::int64_t t;
  // Duration (ticks)
  std::int64_t d;
  // Accumulated latency in the path
  std::int64_t l;
  // Entry (master port)
  int id;

  constexpr bool operator<(const Event& other) const { return t + l < other.t + other.l; }
};


// Ordered list based on the operator< defined in Event struct
using Queue = std::set<Event>;


class router_shared_async;


class MapEntry {
public:
  MapEntry() {}
  MapEntry(unsigned long long base, MapEntry *left, MapEntry *right);

  void insert(router_shared_async *router);

  string target_name;
  MapEntry *next = NULL;
  int id = -1;
  unsigned long long base = 0;
  unsigned long long lowestBase = 0;
  unsigned long long size = 0;
  unsigned long long remove_offset = 0;
  unsigned long long add_offset = 0;
  uint32_t latency = 0;
  MapEntry *left = NULL;
  MapEntry *right = NULL;
  vp::io_slave *port = NULL;
  vp::io_master *itf = NULL;
};


class io_master_map : public vp::io_master
{

  inline void bind_to(vp::port *port, vp::config *config);

};


class router_shared_async : public vp::component
{

  friend class MapEntry;

public:

  router_shared_async(js::config *config);

  int build();

  static vp::io_req_status_e req(void *__this, vp::io_req *req, int port);


  static void grant(void *_this, vp::io_req *req);

  static void response(void *_this, vp::io_req *req);

private:
  vp::trace     trace;

  io_master_map out;
  vp::io_slave *in;
  bool init = false;

  void init_entries();
  void check_requests();
  void schedule_port(int offset, int port);
  void deschedule_port(int offset, int port);
  void forwarding_protocol(vp::io_req *req, int port);
  bool port_is_scheduled(int offset, int port);
  void push_round_robin(vector<Event>& q, Event e);
  void (router_shared_async::*arbiter_handler_meth)(vector<Event>& q, Event e);
  static void forwarding_handler(void *__this, vp::clock_event *event);
  MapEntry *firstMapEntry = NULL;
  MapEntry *defaultMapEntry = NULL;
  MapEntry *errorMapEntry = NULL;
  MapEntry *topMapEntry = NULL;
  MapEntry *externalBindingMapEntry = NULL;

  MapEntry *get_entry(vp::io_req *req);

  int broadcast;
  int nb_entries;
  int bandwidth = 0;
  int latency = 0;

  std::uint64_t scheduling;
  vector<Event> scheduled_reqs;
  vector<vector<Event>> pending_reqs;
  vector<Queue> pending_services;
  vp::clock_event **forwarding_event;
};


router_shared_async::router_shared_async(js::config *config)
: vp::component(config)
{

}


MapEntry::MapEntry(unsigned long long base, MapEntry *left, MapEntry *right) : next(NULL), base(base), lowestBase(left->lowestBase), left(left), right(right) {
}


// Event push with Round Robin arbitration:
// tries to push the new event in its ordered location and in case
// of conflict tries to resolve serializing all of the conflicts
// with a Round Robin mechanism.
// Multiple slaves might be served simultaneously if the requests have different masters.
// Otherwise, this transaction triggers at least one conflict to resolve.
void router_shared_async::push_round_robin(vector<Event>& q, Event e) {
  // Schedule only one request at a time for this master. The others are enqueued to be serialized 
  if(!port_is_scheduled(MASK_OFFSET_MASTERS, e.id)) {
    //printf("scheduled_reqs -> inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
    //e.t = e.t + e.l + e.d;
    q.push_back(e);
    schedule_port(MASK_OFFSET_MASTERS, e.id);
  }
  else {
    // Extract the slave port id of the scheduled request for current master entry id
    auto scheduled_event = find_if(q.begin(), q.end(),
      [&](struct Event element){ return (element.id == e.id) ; });
    int scheduled_port   = scheduled_event->port;
    // On-going request to this master entry id
    int scheduled_time   = scheduled_event->t + scheduled_event->l + scheduled_event->d;

    // If there are pending requests
    if(!pending_services[e.id].empty()) {
      // Buffer to store all the pending requests for this master port
      vector<struct Event> v;
      // Move the pending requests in the buffer vector
      move(pending_services[e.id].begin(), pending_services[e.id].end(), back_inserter(v));
      // Erase the pending set for this master
      pending_services[e.id].erase(pending_services[e.id].begin(), pending_services[e.id].end());
      // Round Robin algorithm. The scheduled port is crucial to determine the order of the others
      if(e.port > scheduled_port) {
        // If the new arrival is greater than the scheduled, it starts to search the position from left to right
        auto next_id = find_if(
          v.begin(), v.end(),
          [&](struct Event element){ return (element.port > e.port) ; });

        if(next_id != v.end()) {
          v.insert(next_id, e);
        }
        else {
          auto prev_id = find_if(
            v.rbegin(), v.rend(),
            [&](struct Event element){ return (element.port < e.port) ; });
          v.insert(prev_id.base(), e);
        }
      }
      else {
        // If it is less than the the scheduled, it starts to search the position form right to left
        auto next_id = find_if(
          v.rbegin(), v.rend(),
          [&](struct Event element){ return (element.port < e.port) ; });

        if(next_id != v.rend()) {
          v.insert(next_id.base(), e);
        }
        else {
          auto prev_id = find_if(
              v.rbegin(), v.rend(),
              [&](struct Event element){ return (((this->nb_entries - 1) - element.port) <= e.port) ; });
          v.insert(prev_id.base(), e);
        }
      }

      // Assign new value to the scheduling time and insert in the pending requests queue
      for(auto& cur : v) {
        // Next scheduling time is the previous plus the latency and duration of the request
        cur.t = scheduled_time + 1;// + cur.l + cur.d;
        // The protocol does not add any dead cycle
        scheduled_time = cur.t + cur.l + cur.d;
	      //printf("pending_services -> arbitration and inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
        pending_services[e.id].insert(cur);
      }
    }
    else {
      //printf("pending_services -> inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
      e.t = scheduled_time + 1;// + e.l + e.d;
      pending_services[e.id].insert(e);
    }
  }
}


void MapEntry::insert(router_shared_async *router)
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


void router_shared_async::check_requests()
{
  //printf("check_req\n");
  for(auto& next_event : this->scheduled_reqs) {
    int id = next_event.id;
    // Enqueue untill the end of scheduled events
    if(!this->forwarding_event[id]->is_enqueued())
    {
      //printf("enqueue req %p (id: %d, current cycle: %ld, latency: %ld)\n", next_event.req, id, this->get_cycles(), (next_event.t-this->get_cycles())+next_event.l+next_event.d);
      // Execution time has to take into account the actual time
      this->event_enqueue(this->forwarding_event[id], (next_event.t-this->get_cycles())+next_event.l+next_event.d);
    }
  }
}


MapEntry *router_shared_async::get_entry(vp::io_req *req)
{
  if (!this->init)
  {
    this->init = true;
    this->init_entries();
  }

  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  bool isRead = !req->get_is_write();

  MapEntry *entry = this->topMapEntry;

  if (entry) {
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
    if (this->errorMapEntry && offset >= this->errorMapEntry->base && offset + size - 1 <= this->errorMapEntry->base + this->errorMapEntry->size - 1) {
    } else {
      entry = this->defaultMapEntry;
    }
  }

  if (!entry) {
    this->warning.msg(vp::trace::LEVEL_WARNING, "ERROR\n");
  }

  if (entry == this->defaultMapEntry) {
    this->trace.msg(vp::trace::LEVEL_TRACE, "Routing to default entry (target: %s)\n", entry->target_name.c_str());
  } else {
    this->trace.msg(vp::trace::LEVEL_TRACE, "Routing to entry (target: %s)\n", entry->target_name.c_str());
  }
  
  if (entry->remove_offset) req->set_addr(offset - entry->remove_offset);
  if (entry->add_offset) req->set_addr(offset + entry->add_offset);

  if (!entry->itf->is_bound()) {
    this->warning.msg(vp::trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, !isRead);
  }

  return entry;
}


// Multiple callbacks might be scheduled from different callers
void router_shared_async::forwarding_handler(void *__this, vp::clock_event *event)
{
  router_shared_async *_this = (router_shared_async *)__this;

  // Retrive the caller id
  int id = event->get_int(0);

  // Retrive the information about the caller from the id
  auto current_event = find_if(_this->scheduled_reqs.begin(), _this->scheduled_reqs.end(),
    [&](struct Event element){ return (element.id == id) ; });
  vp::io_req *req = current_event->req;
  int port = current_event->port;
  //printf("firing %p (port: %d, id: %d)\n", req, port, id);

  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  bool isRead = !req->get_is_write();

  _this->trace.msg(vp::trace::LEVEL_INFO, "Respond to IO req (id: %d, port: %d, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", id, port, offset, size, !isRead);

  MapEntry *entry = _this->get_entry(req);

  //printf("forwarding %p (pushing port %p)\n", entry->itf, req->resp_port);

  // The request might not be served yet but the forwarding has been done
  // Remove from the list the fired request
  _this->scheduled_reqs.erase(current_event);

  // Store the slave and master port information. Cast two times to avoid warning related to different size of the elements
  req->arg_push((void *)(intptr_t)id);
  req->arg_push((void *)(intptr_t)port);
  // Store the address of resp_port because the stub will overwrite it
  req->arg_push(req->resp_port);
  // Forward the request
  vp::io_req_status_e result = entry->itf->req(req);

  // Synchronous response
  if(result == vp::IO_REQ_OK) {
    //printf("Sync response\n");
    // Release the port
    vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
    int port = (intptr_t)(req->arg_pop());
    int id   = (intptr_t)(req->arg_pop());

    // Master port id is now free
    _this->deschedule_port(MASK_OFFSET_MASTERS, id);
    // Slave port id is now free
    _this->deschedule_port(MASK_OFFSET_SLAVES, port);

    if(pending_port != NULL)
    {
      //printf("responding %p (port %p)\n", req, pending_port);
      pending_port->resp(req);
    }

    // Continue enqueuing the next request
    _this->check_requests();
  }
}


void router_shared_async::schedule_port(int offset, int id)
{
  //printf("schedule id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ((0x1UL << (id + offset))), this->scheduling, this->scheduling | ((0x1UL << (id + offset))));
  this->scheduling |= ((0x1UL << (id + offset)));
}


void router_shared_async::deschedule_port(int offset, int id)
{
  //printf("deschedule id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ~((0x1UL << (id + offset))), this->scheduling, this->scheduling & ~((0x1UL << id) << offset));
  this->scheduling &= ~((0x1UL << (id + offset)));

  if((offset == MASK_OFFSET_MASTERS) && (!this->pending_services[id].empty())) {
    // Pop from the pending queue the next event from the same port
    auto next_event = *this->pending_services[id].begin();
    // Update the timestamp to the actual time
    next_event.t = this->get_cycles();
    // Schedule it
    this->scheduled_reqs.push_back(next_event);
    // Tag the port as scheduled
    this->schedule_port(offset, id);
    // Remove from the list the just scheduled request
    this->pending_services[id].erase(this->pending_services[id].begin());
    //printf("schedule pending service (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_services[id].size());
  }
  else if((offset == MASK_OFFSET_SLAVES) && (!this->pending_reqs[id].empty())) {
    // Pop from the pending queue the next event from the same port
    auto next_event = *this->pending_reqs[id].begin();
    // Update the timestamp to the actual time
    next_event.t = this->get_cycles();
    // Schedule it
    (this->*arbiter_handler_meth)(this->scheduled_reqs, next_event);
    // Tag the port as scheduled
    this->schedule_port(offset, id);
    // Remove from the list the just scheduled request
    this->pending_reqs[id].erase(this->pending_reqs[id].begin());
    //printf("schedule pending req (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_reqs[id].size());
  }
}


bool router_shared_async::port_is_scheduled(int offset, int id)
{
  //printf("id %d at offset %d is scheduled: %d\n", id, offset, (this->scheduling & ((0x1UL << id) << offset)) != 0);
  return (bool)((this->scheduling & ((0x1UL << (id + offset)))) != 0);
}


void router_shared_async::forwarding_protocol(vp::io_req *req, int port)
{
  uint64_t offset = req->get_addr();
  uint64_t size   = req->get_size();
  uint8_t *data   = req->get_data();

  this->trace.msg(vp::trace::LEVEL_INFO, "Received IO req (port: %d, offset: 0x%llx, size: 0x%llx)\n", port, offset, size);

  // Duration of this packet in this router according to router bandwidth
  int64_t packet_duration = ((size + this->bandwidth - 1) / this->bandwidth);

  // Cycle of reception of this packet
  int64_t packet_time    = this->get_cycles();
  // Latency accumulated so far in the packet path. This is due to other synchronous modules
  int64_t packet_latency = req->get_latency();
  // Delete the latency just accounted. The latency accumulated so far is accounted in the timestamp of this packet
  req->set_latency(0);

  // Get the target port
  MapEntry *entry = this->get_entry(req);

  //printf("req %p (port %d, id %d)\n", req, port, entry->id);

  // There are two lists: Ordered list for scheduled packet and a non-ordered list (per port) for pending requests
  if(port_is_scheduled(MASK_OFFSET_SLAVES, port)) {
    // New request is arrived. Just account it because the same port is already scheduled. t will be update before trying to schedule it
    this->pending_reqs[port].push_back({req, port, 0, this->latency + packet_duration, packet_latency, entry->id});
    //printf("new pending req %p (size: %ld, port: %d, id: %d)\n", req, this->pending_reqs[port].size(), port, entry->id);
  }
  else {
    // Find conflict (if there are) the request scanning the queue of on-going requests
    (this->*arbiter_handler_meth)(this->scheduled_reqs, {req, port, packet_time, this->latency + packet_duration, packet_latency, entry->id});
    // Tag the port as scheduled
    schedule_port(MASK_OFFSET_SLAVES, port);
  }

  // Enqueue the first request
  this->check_requests();
}


// This router has an asynchronous behavior. It accepts incoming request and serves them as gvsoc events considering latency, bandwitdh and arbitration policy
vp::io_req_status_e router_shared_async::req(void *__this, vp::io_req *req, int port)
{
  router_shared_async *_this = (router_shared_async *)__this;

  // Block the port. Resp_port has been set by the stub and now it's locked on the slave address 
  req->resp_port->grant(req);

  // Try to transmit. Insert the incoming request and its input port in the proper list
  _this->forwarding_protocol(req, port);

  // PENDING response should be supported at the slave side
  return vp::IO_REQ_PENDING;
}


void router_shared_async::grant(void *__this, vp::io_req *req)
{
  router_shared_async *_this = (router_shared_async *)__this;

  // Retrive the grant port address
  vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
  // Clean the request stack of this object before granting recursively
  int port = (intptr_t)(req->arg_pop());
  int id   = (intptr_t)(req->arg_pop());

  // Grant to the input slave
  if (pending_port != NULL)
  {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Grant %p (port: %p)\n", req, pending_port);
    pending_port->grant(req);
  }

  // Push back them again
  req->arg_push((void *)(intptr_t)id);
  req->arg_push((void *)(intptr_t)port);
  req->arg_push(pending_port);
}


void router_shared_async::response(void *__this, vp::io_req *req)
{
  router_shared_async *_this = (router_shared_async *)__this;

  // Retrive the response port address
  vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
  int port = (intptr_t)(req->arg_pop());
  int id   = (intptr_t)(req->arg_pop());

  // Master port id is now free
  _this->deschedule_port(MASK_OFFSET_MASTERS, id);
  // Slave port id is now free
  _this->deschedule_port(MASK_OFFSET_SLAVES, port);

  // Respond to the input slave
  if (pending_port != NULL)
  {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Response %p (port: %p)\n", req, pending_port);
    pending_port->resp(req);
  }

  // Continue enqueuing the next request
  _this->check_requests();
}


int router_shared_async::build()
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  int nb_slave_ports = get_config_int("nb_slaves");

  // Define the type of channel (shared or private). By default the channel is private
  broadcast = false;
  // if(get_config_bool("broadcast") != NULL) {
  //   broadcast = get_config_bool("broadcast");
  // }

  in = new vp::io_slave[nb_slave_ports];
  this->nb_entries = 0;
  // Fully connected crossbar
  if(broadcast) {
    this->nb_entries = 1;
  }

  // Initialize the mask of the scheduled ports. It supports up to 32 slave ports and 32 master ports.
  // The MSBs are reserved for the masters, the LSBs for the slaves. Use the SYMBOL (MASK_OFFSET_*) to address them.
  scheduling = 0;

  for (int i=0; i<nb_slave_ports; i++)
  {
    // Create an empty vector
    vector<Event> pending_reqs_port;
    // Initialize the vector of vectors with empty vectors. Now you can address it using array reference
    pending_reqs.push_back(pending_reqs_port);

    in[i].set_req_meth_muxed(&router_shared_async::req, i);
    new_slave_port("input_" + std::to_string(i), &in[i]);
  }

  arbiter_handler_meth = &router_shared_async::push_round_robin;

  out.set_resp_meth(&router_shared_async::response);
  out.set_grant_meth(&router_shared_async::grant);
  new_master_port("out", &out);

  bandwidth = get_config_int("bandwidth");
  latency = get_config_int("latency");

  js::config *mappings = get_js_config()->get("mappings");

  if (mappings != NULL)
  {
    int nb_entries = 0;

    for (auto& mapping: mappings->get_childs())
    {
      js::config *config = mapping.second;

      MapEntry *entry = new MapEntry();
      entry->target_name = mapping.first;

      vp::io_master *itf = new vp::io_master();

      itf->set_resp_meth(&router_shared_async::response);
      itf->set_grant_meth(&router_shared_async::grant);
      new_master_port(mapping.first, itf);

      if (mapping.first == "error")
        entry->itf = NULL;
      else
        entry->itf = itf;

      js::config *conf;
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
      // conf = config->get("id");
      // if (conf) entry->id = conf->get_int();
      // // TOCHECK
      entry->id = nb_entries;


      entry->insert(this);

      nb_entries += 1;
    }

    // Fully connected crossbar
    if(!broadcast) {
      this->nb_entries = nb_entries;
    }
  }

  this->forwarding_event = new vp::clock_event*[this->nb_entries];
  for(int i=0; i<this->nb_entries; i++)
  {
    this->forwarding_event[i] = event_new(router_shared_async::forwarding_handler);
    this->forwarding_event[i]->set_int(0, i);
    // Create an empty set
    set<Event> pending_services_port;
    // Initialize the vector of sets with empty sets. Now you can address it using array reference
    pending_services.push_back(pending_services_port);
  }

  return 0;
}


extern "C" vp::component *vp_constructor(js::config *config)
{
  return new router_shared_async(config);
}


#define max(a, b) ((a) > (b) ? (a) : (b))


void router_shared_async::init_entries() {

  MapEntry *current = firstMapEntry;
  trace.msg(vp::trace::LEVEL_INFO, "Building router table\n");
  while(current) {
    trace.msg(vp::trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> %s\n", current->base, current->base + current->size, current->target_name.c_str());
    current = current->next;
  }
  if (errorMapEntry != NULL) {
    trace.msg(vp::trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> ERROR\n", errorMapEntry->base, errorMapEntry->base + errorMapEntry->size);
  }
  if (defaultMapEntry != NULL) {
    trace.msg(vp::trace::LEVEL_INFO, "       -     :      -     -> %s\n", defaultMapEntry->target_name.c_str());
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


inline void io_master_map::bind_to(vp::port *_port, vp::config *config)
{
  MapEntry *entry = new MapEntry();
  vp::config *conf;
  entry->port = (vp::io_slave *)_port;
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
  entry->insert((router_shared_async *)get_comp());
}
