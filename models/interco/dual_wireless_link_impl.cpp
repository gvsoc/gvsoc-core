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


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Maximum propagation delay in the medium
#define T_SLOT 256
#define MAX_CW_SIZE 4


struct Event {
  vp::io_req *req; // link to the req to enqueue at t+d
  int port; // port of incoming request

  std::uint64_t t;  // timestamp (ticks from 0)
  std::uint64_t d;  // duration (ticks)
  std::uint64_t l; // accumulated latency in the path

  int cw_bound; // exponential multiplier

  bool is_ghosted;

  constexpr bool operator<(const Event& other) const { return t + l < other.t + other.l; }
};


// Ordered list based on the operator< defined in Event struct
using Queue = std::set<Event>;

constexpr bool are_conflicting(const Event& current, const Event& successor) {
  // if(successor.is_ghosted) {
  //   if(successor.t == current.t) {
  //     return true;
  //   }
  //   else {
  //     return false;
  //   }
  // }
  // else {
    if(successor.t == current.t) {
      return true;
    }

    if(successor.t > current.t) {
      return ( ((current.t + current.l + current.d) > (successor.t + successor.l)) );
    }
    else {
      return ( ((successor.t + successor.l + successor.d) > (current.t + current.l)) );
    }
  // }
}


// Example mutators
struct RandomMutator {
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    mutable std::mt19937 generator;
    mutable std::uniform_int_distribution<> distribution;

    RandomMutator(int min, int max)
        : generator(seed), distribution(min, max) {}

    Event& operator()(Event& e) const {
        uint64_t rand_number = distribution(generator);
        //printf("random mutator %ld\n", rand_number);
        e.t += (rand_number*T_SLOT);
        return e;
    }
};


struct DoublerMutator {
    constexpr Event& operator()(Event& e) const {
        e.t *= 2;
        return e;
    }
};


class wireless_link;


class MapEntry {
public:
  MapEntry() {}
  MapEntry(unsigned long long base, MapEntry *left, MapEntry *right);

  void insert(wireless_link *router);

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


class wireless_link : public vp::component
{

  friend class MapEntry;

public:

  wireless_link(js::config *config);

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
  void check_requests(bool is_write);
  bool port_is_scheduled(int port);
  void schedule_port(int port);
  void deschedule_port(int port);
  void medium_access_control_handler(vp::io_req *req, int port);
  void push_random_access(Queue& q, Event e);
  static void wireless_transfer(void *__this, vp::clock_event *event);
  static void dummy_handler(void *__this, vp::clock_event *event) { wireless_link *_this = (wireless_link *)__this; };//printf("executing dummy event at %ld\n", _this->get_cycles()); };
  MapEntry *firstMapEntry = NULL;
  MapEntry *defaultMapEntry = NULL;
  MapEntry *errorMapEntry = NULL;
  MapEntry *topMapEntry = NULL;
  MapEntry *externalBindingMapEntry = NULL;

  int bandwidth = 0;
  int latency = 0;

  //int nb_channels;
  //MapEntry *allocated_entries;
  //int channel_id = 0;

  int nb_slave_ports;

  int64_t scheduling[2];
  Queue scheduled_reqs[2];
  vector<vector<Event>> pending_reqs;
  vp::clock_event *wireless_transfer_read_event;
  vp::clock_event *wireless_transfer_write_event;
  vp::clock_event *cancel_read_event;
  vp::clock_event *cancel_write_event;
};


wireless_link::wireless_link(js::config *config)
: vp::component(config)
{

}


MapEntry::MapEntry(unsigned long long base, MapEntry *left, MapEntry *right) : next(NULL), base(base), lowestBase(left->lowestBase), left(left), right(right) {
}

// Event push with random access:
// tries to push the new event in its ordered location and in case
// of conflict tries to recursively resolve all of the conflicts
// by a naive remove-mutate-insert heuristic.
// WARNING: this algorithm is not guaranteed to terminate! The actual
//          probability is remote and depends on how the Mutator perturbates
//          nodes to try dodging conflicts, but this still boils down to
//          a (random, if mutation is random) local probing of the solutions
//          space.
void wireless_link::push_random_access(Queue& q, Event e) {
    // The first element already scheduled which collides with the current event
    //printf("entering [%d]: t %ld l %ld d %ld\n", e.port, e.t, e.l, e.d);
    auto successor = q.end();
    for(auto iter = q.begin(); iter != q.end(); iter++)
    {
      auto cur = *iter;
      //printf("[%d]: t %ld l %ld d %ld ghosted %d\n", cur.port, cur.t, cur.l, cur.d, cur.is_ghosted);
      if(are_conflicting(e, cur)) {
        //printf("is_conflicting\n");
        successor = iter;
        break;
      }
    }

    // If the collision is with the first request,
    // it might has been already enqueued and
    // an amendment is needed
    if(e.req->get_is_write()) {
      if ((e < *q.begin() || ((successor == q.begin()) && (!successor->is_ghosted))) && (this->wireless_transfer_write_event->is_enqueued())) {
        //printf("dequeuing %p\n", this->wireless_transfer_write_event);
        // if(!this->cancel_write_event->is_enqueued())
        //   this->event_enqueue(this->cancel_write_event, 1);
        this->event_cancel(this->wireless_transfer_write_event);
      }
    }
    else {
      if ((e < *q.begin() || ((successor == q.begin()) && (!successor->is_ghosted))) && (this->wireless_transfer_read_event->is_enqueued())) {
        //printf("dequeuing %p\n", this->wireless_transfer_read_event);
        // if(!this->cancel_read_event->is_enqueued())
        //   this->event_enqueue(this->cancel_read_event, 1);
        this->event_cancel(this->wireless_transfer_read_event);
      }
    }

    if (successor != q.end()) {
      //printf("conflict\n");
      // Ok, we have at least one conflicting element:
      // Find first non-conflicting element, a.k.a. the
      // end of the conflicting range

      srand(10);

      // 2. Mutate and re-insert one element at a time
      q.erase(successor);
      auto cur         = *successor;
      auto cur_ghosted = *successor;
      cur_ghosted.t = MIN(e.t, cur_ghosted.t);

      if(!successor->is_ghosted) {
        //printf("copying %p and ghosting %p\n", &cur, &cur_ghosted);
        cur.cw_bound += 1;
        cur.t = MAX(e.t, cur.t);

        cur_ghosted.is_ghosted = true;
        //cur_ghosted.d = cur.t - cur_ghosted.t;

        cur.t += (rand() % ((1U << MIN(cur.cw_bound, MAX_CW_SIZE))))*T_SLOT;
        //int64_t rand_max = ((1U << MIN(cur.cw_bound, MAX_CW_SIZE)) - 1);
        //printf("2. %d %d %ld\n", cur.cw_bound, MIN(cur.cw_bound, MAX_CW_SIZE), cur.t);
        //RandomMutator mut(0, rand_max);
        //cur = mut(cur);

        push_random_access(q, cur);
        //if(cur_ghosted.d != 0) {
        //}        
      }

      if(cur.t != cur_ghosted.t) {
        q.insert(cur_ghosted);
      }

      e.cw_bound += 1;
      e.t = MAX(cur_ghosted.t, e.t);
      // 3. Mutate current element
      //rand_max = ((1U << MIN(e.cw_bound, MAX_CW_SIZE)) - 1);
      e.t += (rand() % ((1U << MIN(e.cw_bound, MAX_CW_SIZE))))*T_SLOT;
      //printf("3. %d %d %ld\n", e.cw_bound, MIN(e.cw_bound, MAX_CW_SIZE), e.t);
      //RandomMutator mut2(0, rand_max);
      //e = mut2(e);
      if(cur_ghosted.is_ghosted && (e.t == cur_ghosted.t)) {
        q.erase(cur_ghosted);
      }
    }

    // // Check that the insertion is allowed, otherwise retry.
    for(auto iter = q.begin(); iter != q.end();)
    {
      while(are_conflicting(e, *iter)) {
        if(iter->is_ghosted && (e.t == iter->t)) {
          iter = q.erase(iter);
        }
        else {
          //printf("is_conflicting while the other are resolved\n");
          e.cw_bound += 1;
          e.t += (rand() % ((1U << MIN(e.cw_bound, MAX_CW_SIZE))))*T_SLOT;
          //int64_t rand_max = ((1U << MIN(e.cw_bound, MAX_CW_SIZE)) - 1);
          //printf("4. %d %d %ld\n", e.cw_bound, MIN(e.cw_bound, MAX_CW_SIZE), e.t);
          //RandomMutator mut(0, rand_max);
          //e = mut(e);
        }
      }
      iter++;
    }

    //printf("inserting\n");
    q.insert(e);

    // for(auto iter : q)
    // {
    //   printf("[%d]: t %ld l %ld d %ld ghosted %d\n", iter.port, iter.t, iter.l, iter.d, iter.is_ghosted);
    // }
}


void MapEntry::insert(wireless_link *router)
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


void wireless_link::check_requests(bool is_write)
{
  //printf("check_req %d\n", is_write);
  // Enqueue untill the end of scheduled events
  if(is_write) {
    if((!this->wireless_transfer_write_event->is_enqueued()) && (!this->scheduled_reqs[is_write].empty())) {
      struct Event next_event;
      for(auto iter = this->scheduled_reqs[is_write].begin(); iter != this->scheduled_reqs[is_write].end(); iter++) {
        auto cur = *iter;
        if(!cur.is_ghosted) {
          next_event = cur;
          break;
        }
      }
      //printf("enqueue write %p with event %p (current cycle: %ld, time: %ld, latency: %ld, duration: %ld, event latency: %ld)\n", &next_event, this->wireless_transfer_write_event, this->get_cycles(), next_event.t, next_event.l, next_event.d, (next_event.t-this->get_cycles())+next_event.l+next_event.d);
      // Execution time has to take into account the actual time
      this->event_enqueue(this->wireless_transfer_write_event, (next_event.t-this->get_cycles())+next_event.l+next_event.d);
    }
  }
  else {
    if((!this->wireless_transfer_read_event->is_enqueued()) && (!this->scheduled_reqs[is_write].empty())) {
      struct Event next_event;
      for(auto iter = this->scheduled_reqs[is_write].begin(); iter != this->scheduled_reqs[is_write].end(); iter++) {
        auto cur = *iter;
        if(!cur.is_ghosted) {
          next_event = cur;
          break;
        }
      }
      //printf("enqueue read %p with event %p (current cycle: %ld, time: %ld, latency: %ld, duration: %ld, event latency: %ld)\n", &next_event, this->wireless_transfer_read_event, this->get_cycles(), next_event.t, next_event.l, next_event.d, (next_event.t-this->get_cycles())+next_event.l+next_event.d);
      // Execution time has to take into account the actual time
      this->event_enqueue(this->wireless_transfer_read_event, (next_event.t-this->get_cycles())+next_event.l+next_event.d);
    }
  }
}


void wireless_link::wireless_transfer(void *__this, vp::clock_event *event)
{
  wireless_link *_this = (wireless_link *)__this;

  bool is_write = event->get_int(0);

  struct Event current_event;

  for(auto iter = _this->scheduled_reqs[is_write].begin(); iter != _this->scheduled_reqs[is_write].end();) {
    if(iter->is_ghosted) {
      iter = _this->scheduled_reqs[is_write].erase(iter);
    }
    else {
      current_event = *iter;
      break;
    }
    iter++;
  }
  
  vp::io_req *req = current_event.req;
  int port = current_event.port;
  //printf("firing (port %d)\n", port);

  if(req->get_is_write() != is_write) {
    _this->warning.msg(vp::trace::LEVEL_WARNING, "dir %d does not match in scheduled queue\n", is_write);
  }

  if (!_this->init) {
    _this->init = true;
    _this->init_entries();
  }

  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  bool isRead = !req->get_is_write();

  _this->trace.msg(vp::trace::LEVEL_INFO, "Respond to IO req (port: %d, offset: 0x%llx, size: 0x%llx)\n", port, offset, size);

  MapEntry *entry = _this->topMapEntry;

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
    _this->warning.msg(vp::trace::LEVEL_WARNING, "ERROR\n");
  }

  if (entry == _this->defaultMapEntry) {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Routing to default entry (target: %s)\n", entry->target_name.c_str());
  } else {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Routing to entry (target: %s)\n", entry->target_name.c_str());
  }

  // Forward the request to the target port
  if (entry->remove_offset) req->set_addr(offset - entry->remove_offset);
  if (entry->add_offset) req->set_addr(offset + entry->add_offset);

  //printf("forwarding %p (pushing port %p)\n", entry->itf, req->resp_port);

  vp::io_req_status_e result = vp::IO_REQ_OK;

  if (!entry->itf->is_bound()) {
    _this->warning.msg(vp::trace::LEVEL_WARNING, "Invalid access, trying to route to non-connected interface (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, !isRead);
  }

  // Store the address of resp_port because the stub will overwrite it
  req->arg_push(req->resp_port);
  // Forward the request
  result = entry->itf->req(req);

  // Synchronous response
  if(result == vp::IO_REQ_OK) {
    // Release the port
    vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
    if(pending_port != NULL)
    {
      //printf("responding %p (port %p, latency: %ld, duration: %ld)\n", req, pending_port, req->get_latency(), req->get_duration());
      pending_port->resp(req);
    }
    //for(auto iter : _this->scheduled_reqs)
    //{
    //  printf("[%d]: t+l+d %ld\n", iter.port, iter.t + iter.l + iter.d);
    //}
  }

  // The request might not be served yet but the forwarding has been done
  // Remove from the list the fired request
  _this->scheduled_reqs[is_write].erase(current_event);
  // Port id is now free
  _this->deschedule_port(port);
  // if(_this->scheduled_reqs[is_write].empty()) {
  //   for(int i=(is_write * _this->nb_slave_ports); i<((is_write * _this->nb_slave_ports) + _this->nb_slave_ports); i++)
  //     if(_this->port_is_scheduled(i))
  //       _this->deschedule_port(i);
  // }
  // Continue enqueuing the next request
  _this->check_requests(is_write);
  //}
}


void wireless_link::schedule_port(int port)
{
  //printf("schedule %d (value %x)\n", port, this->scheduling[(port >= this->nb_slave_ports)] | 1 << port);
  this->scheduling[(port >= this->nb_slave_ports)] |= 1UL << port;
}


void wireless_link::deschedule_port(int port)
{
  //printf("deschedule %d (value %x)\n", port, this->scheduling[(port >= this->nb_slave_ports)] & ~(1UL << port));
  this->scheduling[(port >= this->nb_slave_ports)] &= ~(1UL << port);

  if(!this->pending_reqs[port].empty()) {
    // Pop from the pending queue the next event from the same port
    auto next_event = *this->pending_reqs[port].begin();
    // Remove from the list the just scheduled request
    this->pending_reqs[port].erase(this->pending_reqs[port].begin());
    // Update the timestamp to the actual time
    uint64_t time = this->get_cycles();
    if(next_event.t + next_event.l < time) {
      next_event.t = time;
    }
    // Schedule it
    this->push_random_access(this->scheduled_reqs[next_event.req->get_is_write()], next_event);
    // Tag the port as scheduled
    this->schedule_port(port);
    //printf("schedule pending req %d %ld\n", port, this->pending_reqs[port].size());
  }
}


bool wireless_link::port_is_scheduled(int port)
{
  //printf("%d is_scheduled %d\n", port, (this->scheduling[(port >= this->nb_slave_ports)] & (1 << port)) != 0);
  return (bool)((this->scheduling[(port >= this->nb_slave_ports)] & (1UL << port)) != 0);
}


void wireless_link::medium_access_control_handler(vp::io_req *req, int port)
{
  uint64_t offset = req->get_addr();
  uint64_t size   = req->get_size();
  uint8_t *data   = req->get_data();
  bool is_write   = req->get_is_write();

  this->trace.msg(vp::trace::LEVEL_INFO, "Received IO req (port: %d, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", port, offset, size, is_write);

  // Duration of this packet in this router according to router bandwidth
  uint64_t packet_duration = ((size + this->bandwidth - 1) / this->bandwidth);

  // Cycle of reception of this packet
  uint64_t packet_time    = this->get_cycles() + req->get_full_latency();
  // Latency accumulated so far in the packet path. This is due to other synchronous modules
  uint64_t packet_latency = 0;//req->get_latency();
  //printf("new req received %p (port: %d, time: %ld, latency: %ld, duration: %ld)\n", req, port, packet_time, packet_latency, packet_duration);
  //int64_t packet_latency = req->get_full_latency();
  // Delete the latency just accounted. The latency accumulated so far is accounted in the timestamp of this packet
  req->set_latency(0);
  req->new_duration(0);
  // // Account the duration
  // if(req->get_duration() > packet_duration) req->new_duration(req->get_duration() - packet_duration);
  // else req->new_duration(0);
  // There are two lists: Ordered list for scheduled packet and a non-ordered list (per port) for pending requests
  if(port_is_scheduled(port)) {
    // New request is arrived. Just account it because the same port is already scheduled. t will be update before trying to schedule it
    this->pending_reqs[port].push_back({req, port, packet_time, this->latency + packet_duration, packet_latency, 0, false});
    //printf("new pending req %d %ld\n", port, this->pending_reqs[port].size());
  }
  else {
    // Find conflict (if there are) the request scanning the queue of on-going requests
    push_random_access(this->scheduled_reqs[is_write], {req, port, packet_time, this->latency + packet_duration, packet_latency, 0, false});
    // Tag the port as scheduled
    schedule_port(port);
    // Enqueue the first request
    this->check_requests(is_write);
  }
}


// This router has an asynchronous behavior. It accepts incoming request and serves them as gvsoc events considering latency, bandwitdh and arbitration policy
vp::io_req_status_e wireless_link::req(void *__this, vp::io_req *req, int port)
{
  wireless_link *_this = (wireless_link *)__this;
  bool is_write = req->get_is_write();

  // Block the port. Resp_port has been set by the stub and now it's locked on the slave address 
  req->resp_port->grant(req);

  // Try to transmit. Insert the incoming request and its input port in the proper list
  _this->medium_access_control_handler(req, port + (is_write * _this->nb_slave_ports));

  // PENDING response should be supported at the slave side
  return vp::IO_REQ_PENDING;
}


void wireless_link::grant(void *__this, vp::io_req *req)
{
  wireless_link *_this = (wireless_link *)__this;

  // Retrive the grant port address
  vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();

  // Grant to the input slave
  if (pending_port != NULL)
  {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Grant %p (port: %p)\n", req, pending_port);
    pending_port->grant(req);
  }

  // Push it again
  req->arg_push(pending_port);
}


void wireless_link::response(void *__this, vp::io_req *req)
{
  wireless_link *_this = (wireless_link *)__this;

  // Retrive the response port address
  vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();

  // Respond to the input slave
  if (pending_port != NULL)
  {
    //printf("responding asyncronously %p (port %p, latency: %ld, duration: %ld)\n", req, pending_port, req->get_latency(), req->get_duration());
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Response %p (port: %p)\n", req, pending_port);
    pending_port->resp(req);

  // // Pending execution can be now finilized
    //bool is_write = req->get_is_write();
  // auto current_event = *_this->scheduled_reqs[is_write].begin();
  // int port = current_event.port;
  // //Remove from the list the fired request
  // _this->scheduled_reqs[is_write].erase(current_event);

  // _this->deschedule_port(port);

    //_this->check_requests(is_write);
  }
}


int wireless_link::build()
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  nb_slave_ports = get_config_int("nb_slaves");

  //nb_channels       = get_config_int("nb_channels");
  //allocated_entries = new MapEntry[nb_channels];

  in = new vp::io_slave[nb_slave_ports];

  // Initialize the mask of the scheduled ports
  scheduling[0] = 0;
  scheduling[1] = 0;

  for (int i=0; i<nb_slave_ports; i++)
  {
    // Create an empty vector
    vector<Event> pending_reqs_read_port;
    vector<Event> pending_reqs_write_port;
    // Initialize the vector of vectors with empty vectors. Now you can address it using array reference
    pending_reqs.push_back(pending_reqs_read_port);
    pending_reqs.push_back(pending_reqs_write_port);
  }

  for (int i=0; i<nb_slave_ports; i++)
  {
    in[i].set_req_meth_muxed(&wireless_link::req, i);
    new_slave_port("input_" + std::to_string(i), &in[i]);
  }

  out.set_resp_meth(&wireless_link::response);
  out.set_grant_meth(&wireless_link::grant);
  new_master_port("out", &out);

  bandwidth = get_config_int("bandwidth");
  latency = get_config_int("latency");

  js::config *mappings = get_js_config()->get("mappings");

  this->wireless_transfer_read_event  = event_new(&this->wireless_link::wireless_transfer);
  //this->cancel_read_event = event_new(&this->wireless_link::dummy_handler);
  this->wireless_transfer_read_event->set_int(0,0);
  this->wireless_transfer_write_event = event_new(&this->wireless_link::wireless_transfer);
  //this->cancel_write_event = event_new(&this->wireless_link::dummy_handler);
  this->wireless_transfer_write_event->set_int(0,1);

  if (mappings != NULL)
  {
    for (auto& mapping: mappings->get_childs())
    {
      js::config *config = mapping.second;

      MapEntry *entry = new MapEntry();
      entry->target_name = mapping.first;

      vp::io_master *itf = new vp::io_master();

      itf->set_resp_meth(&wireless_link::response);
      itf->set_grant_meth(&wireless_link::grant);
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
      conf = config->get("id");
      if (conf) entry->id = conf->get_int();


      entry->insert(this);
    }
  }
  return 0;
}


extern "C" vp::component *vp_constructor(js::config *config)
{
  return new wireless_link(config);
}




void wireless_link::init_entries() {

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
  entry->insert((wireless_link *)get_comp());
}
