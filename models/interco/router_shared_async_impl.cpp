/*
 * Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


class MapEntry;

struct Event {
  // Link to the req to enqueue at t+l+d
  vp::io_req *req;
  // Port of incoming request (slave port)
  int port;
  // Timestamp (ticks from 0)
  int64_t t;
  // Duration (ticks)
  int64_t d;
  // Accumulated latency in the path
  int64_t l;
  // Entry (master port)
  int id;

  constexpr bool operator<(const Event& other) const { return t < other.t; }
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
  void schedule_port(int offset, int id, bool is_write);
  void deschedule_port(int offset, int port, bool is_write);
  bool port_is_scheduled(int offset, int port, bool is_write);
  void forwarding_protocol(vp::io_req *req, int port);
  void push_round_robin(vector<Event>& scheduled_reqs, vector<vector<Event>>& pending_reqs, vector<Queue>& pending_services, Event e);
  void (router_shared_async::*arbiter_handler_meth)(vector<Event>& scheduled_reqs, vector<vector<Event>>& pending_reqs, vector<Queue>& pending_services, Event e);
  static void forwarding_handler(void *__this, vp::clock_event *event);
  static void responding_handler(void *__this, vp::clock_event *event);
  void check_forwarding_req(bool is_write);
  void check_responding_req(vp::io_req *req, int id, int port);
  MapEntry *firstMapEntry = NULL;
  MapEntry *defaultMapEntry = NULL;
  MapEntry *errorMapEntry = NULL;
  MapEntry *topMapEntry = NULL;
  MapEntry *externalBindingMapEntry = NULL;

  MapEntry *get_entry(vp::io_req *req);

  bool broadcast;
  int nb_entries;
  int nb_ports;
  int bandwidth = 0;
  //int latency = 0;

  int cmd_latency = 0;
  int resp_latency = 0;

  uint64_t scheduling_write;
  vector<Event> scheduled_write_reqs;
  vector<vector<Event>> pending_write_reqs;
  vector<Queue> pending_write_services;

  uint64_t scheduling_read;
  vector<Event> scheduled_read_reqs;
  vector<vector<Event>> pending_read_reqs;
  vector<Queue> pending_read_services;

  vp::clock_event **forwarding_event;
  //vp::clock_event **responding_event;

  vector<vector<Event>> pending_write_resp;
  vector<vector<Event>> pending_read_resp;

  vector<vector<vp::clock_event *>> enqueued_write_resp;
  vector<vector<vp::clock_event *>> enqueued_read_resp;

  int64_t *next_packet_id_time;
  int64_t *next_packet_port_time;
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
void router_shared_async::push_round_robin(vector<Event>& scheduled_reqs, vector<vector<Event>>& pending_reqs, vector<Queue>& pending_services, Event e) {
  bool is_write = e.req->get_is_write();
  // Schedule only one request at a time for this master. The others are enqueued to be serialized 
  if(!port_is_scheduled(MASK_OFFSET_MASTERS, e.id, is_write)) {
    //printf("scheduled_reqs -> inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
    //e.l = this->get_cycles();
    scheduled_reqs.push_back(e);
    schedule_port(MASK_OFFSET_MASTERS, e.id, is_write);
  }
  else {
    // Extract the slave port id of the scheduled request for current master entry id
    auto scheduled_event = find_if(scheduled_reqs.begin(), scheduled_reqs.end(),
      [&](struct Event element){ return (element.id == e.id) ; });
    int scheduled_port   = scheduled_event->port;
    // On-going request to this master entry id
    //uint64_t scheduled_time = scheduled_event->t + scheduled_event->l + scheduled_event->d;
    uint64_t scheduled_time = scheduled_event->t + scheduled_event->d;

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
        cur.l = this->get_cycles();
        // Next scheduling time is the previous plus the latency and duration of the request
        cur.t = scheduled_time;
        // The protocol does not add any dead cycle
        //scheduled_time = cur.t + cur.l + cur.d;
        scheduled_time = cur.t + cur.d;
	//printf("pending_services -> arbitration and inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
        pending_services[e.id].insert(cur);
      }
    }
    else {
      //printf("pending_services -> inserting %p (port: %d, id: %d)\n", e.req, e.port, e.id);
      //e.l = this->get_cycles();
      e.t = scheduled_time;
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


void router_shared_async::check_forwarding_req(bool is_write)
{
  //printf("check_req %d\n", is_write);
  if(is_write) {
    for(auto& next_event : this->scheduled_write_reqs) {
      int id = next_event.id;
      // Enqueue untill the end of scheduled events
      if(!this->forwarding_event[id]->is_enqueued())
      {
        //printf("enqueue write req %p (event id: %d, id: %d, current cycle: %ld, latency: %ld)\n", next_event.req, id, id, this->get_cycles(), (next_event.t-this->get_cycles())+next_event.l+next_event.d);
        // Execution time has to take into account the actual time
        //this->event_enqueue(this->forwarding_event[id], (next_event.t-this->get_cycles())+next_event.l+next_event.d);
        int64_t event_latency = next_event.d;
        //if((this->get_cycles() - next_event.l) < this->latency) {
	  //event_latency += (this->latency - (this->get_cycles() - next_event.l));
	if((this->get_cycles() - next_event.l) < this->cmd_latency) {
          event_latency += (this->cmd_latency - (this->get_cycles() - next_event.l));
        }
        this->event_enqueue(this->forwarding_event[id], event_latency);
        //printf("enqueue write req %p (event id: %d, id: %d, current cycle: %ld, t: %ld, l: %ld, d: %ld, latency: %ld)\n", next_event.req, id, id, this->get_cycles(), next_event.t, next_event.l, next_event.d, event_latency);
      }
    }
  }
  else {
    for(auto& next_event : this->scheduled_read_reqs) {
      int id = next_event.id;
      // Enqueue untill the end of scheduled events
      if(!this->forwarding_event[id + this->nb_entries]->is_enqueued())
      {
        //printf("enqueue read req %p (event id: %d, id: %d, current cycle: %ld, latency: %ld)\n", next_event.req, id + this->nb_entries, id, this->get_cycles(), (next_event.t-this->get_cycles())+next_event.l+next_event.d);
        // Execution time has to take into account the actual time
        //this->event_enqueue(this->forwarding_event[id + this->nb_entries], (next_event.t-this->get_cycles())+next_event.l+next_event.d);
        int64_t event_latency = next_event.d;
        //if((this->get_cycles() - next_event.l) < this->latency) {
          //event_latency += (this->latency - (this->get_cycles() - next_event.l));
        if((this->get_cycles() - next_event.l) < this->cmd_latency) {
          event_latency += (this->cmd_latency - (this->get_cycles() - next_event.l));
        }
        this->event_enqueue(this->forwarding_event[id + this->nb_entries], event_latency);
        //printf("enqueue read req %p (event id: %d, id: %d, current cycle: %ld, t: %ld, l: %ld, d: %ld, latency: %ld)\n", next_event.req, id + this->nb_entries, id, this->get_cycles(), next_event.t, next_event.l, next_event.d, event_latency);
      }
    }
  }
}


void router_shared_async::check_responding_req(vp::io_req *req, int id, int port)
{
  bool is_write = req->get_is_write();
  //printf("check resp %d\n", is_write);
  uint64_t size = req->get_size();
  // Duration of this packet in this router according to router bandwidth
  int event_latency = ((size + this->bandwidth - 1) / this->bandwidth);

  if(is_write) {
    if(req->get_entry() == this) {
      auto current_event = find_if(this->enqueued_write_resp[port].begin(), this->enqueued_write_resp[port].end(),
        [&](vp::clock_event *element){ return ((intptr_t)element->get_args()[0] == id) ; });
      // The request might not be served yet but the forwarding has been done
      // Remove from the list the fired request
      if(current_event != this->enqueued_write_resp[port].end()) {
        //printf("pushing pending write response %p (port: %d, id: %d)\n", req, port, id);
        this->pending_write_resp[id].push_back({req, port, 0, 0, 0, id});
      }
      else {
        vp::clock_event *responding_event = event_new(router_shared_async::responding_handler);
        responding_event->get_args()[0] = (void *)((intptr_t)id);
        responding_event->get_args()[1] = (void *)((intptr_t)port);
        responding_event->get_args()[2] = (void *)req;
   //if(!this->responding_event[id]->is_enqueued()) {
      //if((this->get_cycles() - this->next_packet_id_time[id]) < this->resp_latency) {
        //event_latency += this->resp_latency - (this->get_cycles() - this->next_packet_id_time[id]);
      //}

      //this->responding_event[id]->get_args()[1] = (void *)req;

        int new_latency = 0;
      //if((this->get_cycles() - this->next_packet_port_time[port]) < 0) {
	//new_latency = this->next_packet_port_time[port] - this->get_cycles();
      //if((this->get_cycles() + this->resp_port) < this->next_packet_port_time[port]) {
	//new_latency = this->next_packet_port_time[port] - (this->get_cycles() + this->resp_latency);
        for(auto iter : this->enqueued_write_resp[port]) {
	  int64_t iter_time = (intptr_t)iter->get_args()[3];
	  int64_t iter_dur  = (intptr_t)iter->get_args()[4];
	//int port_latency = MIN((iter.t + iter.d) - this->get_cycles(), event_latency);
          int port_latency = MAX(0, MIN((iter_time + iter_dur) - (this->get_cycles() + this->resp_latency), event_latency));
	  if(port_latency > 0) {
	    //printf("rescheduling write event %p (port: %d, id: %d, previous latency: %ld, additional latency: %d)\n", iter, port, (int)((intptr_t)iter->get_args()[0]), iter_dur, port_latency);
	    iter_dur += port_latency;
	    new_latency += port_latency;
	    this->event_cancel(iter);
	    this->event_enqueue(iter, iter_dur - (this->get_cycles() - iter_time));
	  //this->event_reenqueue(this->responding_event[iter.id], port_latency);
	  }
        }
      //this->event_enqueue(this->responding_event[id], event_latency + new_latency);
        this->event_enqueue(responding_event, this->resp_latency + event_latency + new_latency);
        //printf("enqueueing new write response event %p (port: %d, id: %d, latency: %d)\n", responding_event, port, id, this->resp_latency + event_latency + new_latency);
      //this->enqueued_write_resp[port].push_back({req, port, this->get_cycles(), event_latency, 0, id});
        responding_event->get_args()[3] = (void *)((intptr_t)this->get_cycles());
        responding_event->get_args()[4] = (void *)((intptr_t)(this->resp_latency + event_latency + new_latency));
        this->enqueued_write_resp[port].push_back(responding_event);
      }
      //this->next_packet_id_time[id]     = this->get_cycles() + event_latency;
      //this->next_packet_port_time[port] = this->get_cycles() + event_latency + new_latency;
      //}
    //else {
      //printf("pushing pending write response %p (port: %d, id: %d)\n", req, port, id);
      //this->pending_write_resp[id].push_back({req, port, 0, 0, 0, id});
    //}
    }
    else {
      vp::clock_event *responding_event = event_new(router_shared_async::responding_handler);
      responding_event->get_args()[0] = (void *)((intptr_t)id);
      responding_event->get_args()[1] = (void *)((intptr_t)port);
      responding_event->get_args()[2] = (void *)req;
   //if(!this->responding_event[id]->is_enqueued()) {
      //if((this->get_cycles() - this->next_packet_id_time[id]) < this->resp_latency) {
        //event_latency += this->resp_latency - (this->get_cycles() - this->next_packet_id_time[id]);
      //}

      //this->responding_event[id]->get_args()[1] = (void *)req;

      int new_latency = 0;
      //if((this->get_cycles() - this->next_packet_port_time[port]) < 0) {
	//new_latency = this->next_packet_port_time[port] - this->get_cycles();
      //if((this->get_cycles() + this->resp_port) < this->next_packet_port_time[port]) {
	//new_latency = this->next_packet_port_time[port] - (this->get_cycles() + this->resp_latency);
      for(auto iter : this->enqueued_write_resp[port]) {
	int64_t iter_time = (intptr_t)iter->get_args()[3];
	int64_t iter_dur  = (intptr_t)iter->get_args()[4];
	//int port_latency = MIN((iter.t + iter.d) - this->get_cycles(), event_latency);
        int port_latency = MAX(0, MIN((iter_time + iter_dur) - (this->get_cycles() + this->resp_latency), event_latency));
	if(port_latency > 0) {
	    //printf("rescheduling write event %p (port: %d, id: %d, previous latency: %ld, additional latency: %d)\n", iter, port, (int)((intptr_t)iter->get_args()[0]), iter_dur, port_latency);
	  iter_dur += port_latency;
	  new_latency += port_latency;
	  this->event_cancel(iter);
	  this->event_enqueue(iter, iter_dur - (this->get_cycles() - iter_time));
	  //this->event_reenqueue(this->responding_event[iter.id], port_latency);
	}
      }
      //this->event_enqueue(this->responding_event[id], event_latency + new_latency);
      this->event_enqueue(responding_event, this->resp_latency + event_latency + new_latency);
        //printf("enqueueing new write response event %p (port: %d, id: %d, latency: %d)\n", responding_event, port, id, this->resp_latency + event_latency + new_latency);
      //this->enqueued_write_resp[port].push_back({req, port, this->get_cycles(), event_latency, 0, id});
      responding_event->get_args()[3] = (void *)((intptr_t)this->get_cycles());
      responding_event->get_args()[4] = (void *)((intptr_t)(this->resp_latency + event_latency + new_latency));
      this->enqueued_write_resp[port].push_back(responding_event);
    }
      //this->next_packet_id_time[id]     = this->get_cycles() + event_latency;
      //this->next_packet_port_time[port] = this->get_cycles() + event_latency + new_latency;
      //}
    //else {
      //printf("pushing pending write response %p (port: %d, id: %d)\n", req, port, id);
      //this->pending_write_resp[id].push_back({req, port, 0, 0, 0, id});
    //}
  }
  else {
    if(req->get_entry() == this) {
      auto current_event = find_if(this->enqueued_read_resp[port].begin(), this->enqueued_read_resp[port].end(),
        [&](vp::clock_event *element){ return ((intptr_t)(element->get_args()[0]) == id) ; });
      // The request might not be served yet but the forwarding has been done
      // Remove from the list the fired request
      if(current_event != this->enqueued_read_resp[port].end()) {
        //printf("pushing pending write response %p (port: %d, id: %d)\n", req, port, id);
        this->pending_read_resp[id].push_back({req, port, 0, 0, 0, id});
      }
      else {
        vp::clock_event *responding_event = event_new(router_shared_async::responding_handler);
        responding_event->get_args()[0] = (void *)((intptr_t)id);
        responding_event->get_args()[1] = (void *)((intptr_t)port);
        responding_event->get_args()[2] = (void *)req;
   //if(!this->responding_event[id + this->nb_entries]->is_enqueued()) {
      //if((this->get_cycles() - this->next_packet_id_time[id + this->nb_entries]) < this->resp_latency) {
        //event_latency += this->resp_latency - (this->get_cycles() - this->next_packet_id_time[id + this->nb_entries]);
      //}

      //this->responding_event[id + this->nb_entries]->get_args()[1] = (void *)req;

        int new_latency = 0;
      //if((this->get_cycles() - this->next_packet_port_time[port + this->nb_ports]) < 0) {
	//new_latency = this->next_packet_port_time[port + this->nb_ports] - this->get_cycles();
        for(auto iter : this->enqueued_read_resp[port]) {
	//int port_latency = MIN((iter.t + iter.d) - this->get_cycles(), event_latency);
	//iter.d += port_latency;
	//new_latency += port_latency;
  	  int64_t iter_time = (intptr_t)iter->get_args()[3];
	  int64_t iter_dur  = (intptr_t)iter->get_args()[4];
	  int port_latency = MAX(0, MIN((iter_time + iter_dur) - (this->get_cycles() + this->resp_latency), event_latency));
	  if(port_latency > 0) {
            //printf("rescheduling read event %p (port: %d, id: %d, previous latency: %ld, additional latency: %d)\n", iter, port, (int)((intptr_t)iter->get_args()[0]), iter_dur, port_latency);
	    iter_dur += port_latency;
	    new_latency += port_latency;
	    this->event_cancel(iter);
	    this->event_enqueue(iter, iter_dur - (this->get_cycles() - iter_time));
          //this->event_reenqueue(this->responding_event[iter.id + this->nb_entries], port_latency);
	  }
        }

      //this->event_enqueue(this->responding_event[id + this->nb_entries], event_latency + new_latency);
        this->event_enqueue(responding_event, this->resp_latency + event_latency + new_latency);
        //printf("enqueueing new read response event %p (port: %d, id: %d, latency: %d)\n", responding_event, port, id, this->resp_latency + event_latency + new_latency);
      //this->enqueued_read_resp[port].push_back({req, port, this->get_cycles(), event_latency, 0, id});
        responding_event->get_args()[3] = (void *)((intptr_t)this->get_cycles());
        responding_event->get_args()[4] = (void *)((intptr_t)(this->resp_latency + event_latency + new_latency));
        this->enqueued_read_resp[port].push_back(responding_event);
      //this->next_packet_id_time[id + this->nb_entries]   = this->get_cycles() + event_latency;
      //this->next_packet_port_time[port + this->nb_ports] = this->get_cycles() + event_latency + new_latency;
      }
   // else {
      //printf("pushing pending read response %p (port: %d, id: %d)\n", req, port, id);
      //this->pending_read_resp[id].push_back({req, port, 0, 0, 0, id});
    }
    else {
      vp::clock_event *responding_event = event_new(router_shared_async::responding_handler);
      responding_event->get_args()[0] = (void *)((intptr_t)id);
      responding_event->get_args()[1] = (void *)((intptr_t)port);
      responding_event->get_args()[2] = (void *)req;
   //if(!this->responding_event[id + this->nb_entries]->is_enqueued()) {
      //if((this->get_cycles() - this->next_packet_id_time[id + this->nb_entries]) < this->resp_latency) {
        //event_latency += this->resp_latency - (this->get_cycles() - this->next_packet_id_time[id + this->nb_entries]);
      //}

      //this->responding_event[id + this->nb_entries]->get_args()[1] = (void *)req;

      int new_latency = 0;
      //if((this->get_cycles() - this->next_packet_port_time[port + this->nb_ports]) < 0) {
	//new_latency = this->next_packet_port_time[port + this->nb_ports] - this->get_cycles();
      for(auto iter : this->enqueued_read_resp[port]) {
	//int port_latency = MIN((iter.t + iter.d) - this->get_cycles(), event_latency);
	//iter.d += port_latency;
	//new_latency += port_latency;
        int64_t iter_time = (intptr_t)iter->get_args()[3];
	int64_t iter_dur  = (intptr_t)iter->get_args()[4];
	int port_latency = MAX(0, MIN((iter_time + iter_dur) - (this->get_cycles() + this->resp_latency), event_latency));
	if(port_latency > 0) {
            //printf("rescheduling read event %p (port: %d, id: %d, previous latency: %ld, additional latency: %d)\n", iter, port, (int)((intptr_t)iter->get_args()[0]), iter_dur, port_latency);
	  iter_dur += port_latency;
	  new_latency += port_latency;
	  this->event_cancel(iter);
	  this->event_enqueue(iter, iter_dur - (this->get_cycles() - iter_time));
          //this->event_reenqueue(this->responding_event[iter.id + this->nb_entries], port_latency);
	}
      }

      //this->event_enqueue(this->responding_event[id + this->nb_entries], event_latency + new_latency);
      this->event_enqueue(responding_event, this->resp_latency + event_latency + new_latency);
        //printf("enqueueing new read response event %p (port: %d, id: %d, latency: %d)\n", responding_event, port, id, this->resp_latency + event_latency + new_latency);
      //this->enqueued_read_resp[port].push_back({req, port, this->get_cycles(), event_latency, 0, id});
      responding_event->get_args()[3] = (void *)((intptr_t)this->get_cycles());
      responding_event->get_args()[4] = (void *)((intptr_t)(this->resp_latency + event_latency + new_latency));
      this->enqueued_read_resp[port].push_back(responding_event);
      //this->next_packet_id_time[id + this->nb_entries]   = this->get_cycles() + event_latency;
      //this->next_packet_port_time[port + this->nb_ports] = this->get_cycles() + event_latency 
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


void router_shared_async::responding_handler(void *__this, vp::clock_event *event)
{
  router_shared_async *_this = (router_shared_async *)__this;

  vp::io_req *req = (vp::io_req *)event->get_args()[2];
  int id = event->get_int(0);

  vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
  int port = (intptr_t)(req->arg_pop());
  //int id   = (intptr_t)(req->arg_pop());

  bool is_write = req->get_is_write();

  if(is_write) {
    // Retrive the information about the caller from the id
    auto current_event = find_if(_this->enqueued_write_resp[port].begin(), _this->enqueued_write_resp[port].end(),
      [&](vp::clock_event *element){ return (element == event) ; });
    // The request might not be served yet but the forwarding has been done
    // Remove from the list the fired request
    if(current_event != _this->enqueued_write_resp[port].end()) {
      _this->enqueued_write_resp[port].erase(current_event);
    }
    else {
      printf("event %p does not match in the enqueued write queue\n", event);
    }
  }
  else {
    // Retrive the information about the caller from the id
    auto current_event = find_if(_this->enqueued_read_resp[port].begin(), _this->enqueued_read_resp[port].end(),
      [&](vp::clock_event *element){ return (element == event) ; });
    // The request might not be served yet but the forwarding has been done
    // Remove from the list the fired request
    if(current_event != _this->enqueued_read_resp[port].end()) {
      _this->enqueued_read_resp[port].erase(current_event);
    }
    else {
      printf("event %p does not match in the enqueued read queue\n", event);
    }
  }

  if(pending_port != NULL)
  {
    //printf("async response %p (pending port: %p, port: %d, id: %d)\n", req, pending_port, port, id);
    pending_port->resp(req);
  }

  _this->event_del(event);

  if(req->get_entry() == _this) {
    if(is_write) {
      if(!_this->pending_write_resp[id].empty()) {
        auto pending_resp = *_this->pending_write_resp[id].begin();
        _this->pending_write_resp[id].erase(_this->pending_write_resp[id].begin());
        //printf("enqueueing pending write response %p (port: %d, id: %d)\n", pending_resp.req, pending_resp.port, pending_resp.id);
        _this->check_responding_req(pending_resp.req, pending_resp.id, pending_resp.port);
      }
    }
    else {
      if(!_this->pending_read_resp[id].empty()) {
        auto pending_resp = *_this->pending_read_resp[id].begin();
        _this->pending_read_resp[id].erase(_this->pending_read_resp[id].begin());
        //printf("enqueueing pending read response %p (port: %d, id: %d)\n", pending_resp.req, pending_resp.port, pending_resp.id);
        _this->check_responding_req(pending_resp.req, pending_resp.id, pending_resp.port);
      }
    }
  }
}


// Multiple callbacks might be scheduled from different callers
void router_shared_async::forwarding_handler(void *__this, vp::clock_event *event)
{
  router_shared_async *_this = (router_shared_async *)__this;

  // Retrive the channel id
  bool is_write = event->get_int(0);
  // Retrive the caller id
  int id = event->get_int(1);

  vp::io_req *req;
  int port = 0;

  if(is_write) {
    // Retrive the information about the caller from the id
    auto current_event = find_if(_this->scheduled_write_reqs.begin(), _this->scheduled_write_reqs.end(),
      [&](struct Event element){ return (element.id == id) ; });
    if(current_event != _this->scheduled_write_reqs.end()) {
      req  = current_event->req;
      port = current_event->port;
      // The request might not be served yet but the forwarding has been done
      // Remove from the list the fired request
      _this->scheduled_write_reqs.erase(current_event);
    }
    else {
      printf("ID %d does not match in scheduled write queue\n", id);
    }
  }
  else {
    // Retrive the information about the caller from the id
    auto current_event = find_if(_this->scheduled_read_reqs.begin(), _this->scheduled_read_reqs.end(),
      [&](struct Event element){ return (element.id == id) ; });
    if(current_event != _this->scheduled_read_reqs.end()) {
      req  = current_event->req;
      port = current_event->port;
      // The request might not be served yet but the forwarding has been done
      // Remove from the list the fired request
      _this->scheduled_read_reqs.erase(current_event);
    }
    else {
      printf("ID %d does not match in scheduled read queue\n", id);
    }
  }

  //printf("firing %p (port: %d, id: %d, is_write: %d)\n", req, port, id, is_write);

  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();

  _this->trace.msg(vp::trace::LEVEL_INFO, "Respond IO req %p (id: %d, port: %d, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", req, id, port, offset, size, is_write);

  MapEntry *entry = _this->get_entry(req);

  //printf("forwarding %p (pushing port %p)\n", entry->itf, req->resp_port);

  // Store the slave and master port information. Cast two times to avoid warning related to different size of the elements
  req->arg_push((void *)(intptr_t)id);
  req->arg_push((void *)(intptr_t)port);
  // Store the address of resp_port because the stub will overwrite it
  req->arg_push(req->resp_port);
  // Forward the request
  vp::io_req_status_e result = entry->itf->req(req);

  // Synchronous response
  if(result == vp::IO_REQ_OK) {
    // Release the port
    vp::io_slave *pending_port = (vp::io_slave *)req->arg_pop();
    int port = (intptr_t)(req->arg_pop());
    int id   = (intptr_t)(req->arg_pop());

    if(req->get_entry() == NULL) {
      req->set_entry(_this);
    }

    if(pending_port != NULL)
    {
      //printf("sync response %p (port %p)\n", req, pending_port);
      //pending_port->resp(req);
      req->arg_push((void *)(intptr_t)port);
      req->arg_push(pending_port);
      _this->check_responding_req(req, id, port);
    }
  }
  // Master port id is now free
  _this->deschedule_port(MASK_OFFSET_MASTERS, id, is_write);
  // Slave port id is now free
  _this->deschedule_port(MASK_OFFSET_SLAVES, port, is_write);

  // Continue enqueuing the next request
  _this->check_forwarding_req(is_write);
}


void router_shared_async::schedule_port(int offset, int id, bool is_write)
{
  if(is_write) {
    //printf("schedule write id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ((0x1UL << (id + offset))), this->scheduling_write, this->scheduling_write | ((0x1UL << (id + offset))));
    this->scheduling_write |= ((0x1UL << (id + offset)));
  }
  else {
    //printf("schedule read id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ((0x1UL << (id + offset))), this->scheduling_read, this->scheduling_read | ((0x1UL << (id + offset))));
    this->scheduling_read |= ((0x1UL << (id + offset)));
  }
}


void router_shared_async::deschedule_port(int offset, int id, bool is_write)
{
  if(is_write) {
    //printf("deschedule write id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ~((0x1UL << (id + offset))), this->scheduling_write, this->scheduling_write & ~((0x1UL << id) << offset));
    this->scheduling_write &= ~((0x1UL << (id + offset)));
  }
  else {
    //printf("deschedule read id %d at offset %d (mask: 0x%lx, value 0x%lx, new value: 0x%lx)\n", id, offset, ~((0x1UL << (id + offset))), this->scheduling_read, this->scheduling_read & ~((0x1UL << id) << offset));
    this->scheduling_read &= ~((0x1UL << (id + offset)));
  }

  if(is_write) {
    if((offset == MASK_OFFSET_MASTERS) && (!this->pending_write_services[id].empty())) {
      // Pop from the pending queue the next event from the same port
      auto next_event = *this->pending_write_services[id].begin();
      // Update the timestamp to the actual time
      //next_event.t = this->get_cycles();
      // Schedule it
      this->scheduled_write_reqs.push_back(next_event);
      // Remove from the list the just scheduled request
      this->pending_write_services[id].erase(this->pending_write_services[id].begin());
      //printf("schedule pending write service (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_write_services[id].size());
      // Tag the port as scheduled
      this->schedule_port(offset, id, is_write);
    }
    else if((offset == MASK_OFFSET_SLAVES) && (!this->pending_write_reqs[id].empty())) {
      // Pop from the pending queue the next event from the same port
      auto next_event = *this->pending_write_reqs[id].begin();
      // Update the timestamp to the actual time
      //next_event.t = this->get_cycles();
      // Schedule it
      (this->*arbiter_handler_meth)(this->scheduled_write_reqs, this->pending_write_reqs, this->pending_write_services, next_event);
      // Remove from the list the just scheduled request
      this->pending_write_reqs[id].erase(this->pending_write_reqs[id].begin());
      //printf("schedule pending write req (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_write_reqs[id].size());
      // Tag the port as scheduled
      this->schedule_port(offset, id, is_write);
    }
  }
  else {
    if((offset == MASK_OFFSET_MASTERS) && (!this->pending_read_services[id].empty())) {
      // Pop from the pending queue the next event from the same port
      auto next_event = *this->pending_read_services[id].begin();
      // Update the timestamp to the actual time
      //next_event.t = this->get_cycles();
      // Schedule it
      this->scheduled_read_reqs.push_back(next_event);
      // Remove from the list the just scheduled request
      this->pending_read_services[id].erase(this->pending_read_services[id].begin());
      //printf("schedule pending read service (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_read_services[id].size());
      // Tag the port as scheduled
      this->schedule_port(offset, id, is_write);
    }
    else if((offset == MASK_OFFSET_SLAVES) && (!this->pending_read_reqs[id].empty())) {
      // Pop from the pending queue the next event from the same port
      auto next_event = *this->pending_read_reqs[id].begin();
      // Update the timestamp to the actual time
      //next_event.t = this->get_cycles();
      // Schedule it
      (this->*arbiter_handler_meth)(this->scheduled_read_reqs, this->pending_read_reqs, this->pending_read_services, next_event);
      // Remove from the list the just scheduled request
      this->pending_read_reqs[id].erase(this->pending_read_reqs[id].begin());
      //printf("schedule pending read req (id: %d, offset: %d, size: %ld)\n", id, offset, this->pending_read_reqs[id].size());
      // Tag the port as scheduled
      this->schedule_port(offset, id, is_write);
    }
  }
}


bool router_shared_async::port_is_scheduled(int offset, int id, bool is_write)
{
  if(is_write) {
    //printf("write id %d at offset %d is scheduled: %d\n", id, offset, (this->scheduling_write & ((0x1UL << id) << offset)) != 0);
    return (bool)((this->scheduling_write & ((0x1UL << (id + offset)))) != 0);
  }
  else {
    //printf("read id %d at offset %d is scheduled: %d\n", id, offset, (this->scheduling_read & ((0x1UL << id) << offset)) != 0);
    return (bool)((this->scheduling_read & ((0x1UL << (id + offset)))) != 0);
  }
}


void router_shared_async::forwarding_protocol(vp::io_req *req, int port)
{
  uint64_t offset = req->get_addr();
  uint64_t size   = req->get_size();
  uint8_t *data   = req->get_data();
  bool is_write   = req->get_is_write();

  // Duration of this packet in this router according to router bandwidth
  //int64_t packet_duration = ((size + this->bandwidth - 1) / this->bandwidth);
  int64_t packet_duration = 1;

  // Cycle of reception of this packet
  int64_t packet_time    = this->get_cycles();
  // // Latency accumulated so far in the packet path. This is due to other synchronous modules
  // int64_t packet_latency = req->get_latency();
  // // Delete the latency just accounted. The latency accumulated so far is accounted in the timestamp of this packet
  // req->set_latency(0);
  //int64_t packet_latency = this->latency;

  // Get the target port
  MapEntry *entry = this->get_entry(req);

  this->trace.msg(vp::trace::LEVEL_INFO, "Received IO req %p (id: %d, port: %d, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", req, entry->id, port, offset, size, is_write);

  //printf("req %p (port %d, id %d, is_write: %d)\n", req, port, entry->id, is_write);

  // There are two lists: Ordered list for scheduled packet and a non-ordered list (per port) for pending requests
  if(port_is_scheduled(MASK_OFFSET_SLAVES, port, is_write)) {
    // New request is arrived. Just account it because the same port is already scheduled. t will be update before trying to schedule it
    if(is_write) {
      //this->pending_write_reqs[port].push_back({req, port, 0, this->latency + packet_duration, packet_latency, entry->id});
      this->pending_write_reqs[port].push_back({req, port, packet_time, packet_duration, packet_time, entry->id});
      //printf("new pending write req %p (size: %ld, port: %d, id: %d)\n", req, this->pending_write_reqs[port].size(), port, entry->id);
    }
    else {
      //this->pending_read_reqs[port].push_back({req, port, 0, this->latency + packet_duration, packet_latency, entry->id});
      this->pending_read_reqs[port].push_back({req, port, packet_time, packet_duration, packet_time, entry->id});
      //printf("new pending read req %p (size: %ld, port: %d, id: %d)\n", req, this->pending_read_reqs[port].size(), port, entry->id);
    }
  }
  else {
    // Find conflict (if there are) the request scanning the queue of on-going requests
    if(is_write) {
      //(this->*arbiter_handler_meth)(this->scheduled_write_reqs, this->pending_write_reqs, this->pending_write_services, {req, port, packet_time, this->latency + packet_duration, packet_latency, entry->id});
      (this->*arbiter_handler_meth)(this->scheduled_write_reqs, this->pending_write_reqs, this->pending_write_services, {req, port, packet_time, packet_duration, packet_time, entry->id});
    }
    else {
      //(this->*arbiter_handler_meth)(this->scheduled_read_reqs, this->pending_read_reqs, this->pending_read_services, {req, port, packet_time, this->latency + packet_duration, packet_latency, entry->id});
      (this->*arbiter_handler_meth)(this->scheduled_read_reqs, this->pending_read_reqs, this->pending_read_services, {req, port, packet_time, packet_duration, packet_time, entry->id});
    }
    // Tag the port as scheduled
    schedule_port(MASK_OFFSET_SLAVES, port, is_write);
  }

  // Enqueue the first request
  this->check_forwarding_req(is_write);
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

  //int is_write = req->get_is_write();
  if(req->get_entry() == NULL) {
     req->set_entry(_this);
  }

  // Respond to the input slave
  if (pending_port != NULL)
  {
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Response %p (port: %p)\n", req, pending_port);
    //pending_port->resp(req);
    req->arg_push((void *)(intptr_t)port);
    req->arg_push(pending_port);
    //printf("receiving response %p (port: %d, id: %d)\n", req, port, id);
    _this->check_responding_req(req, id, port);
  }

  // Master port id is now free
  //_this->deschedule_port(MASK_OFFSET_MASTERS, id, is_write);
  // Slave port id is now free
  //_this->deschedule_port(MASK_OFFSET_SLAVES, port, is_write);

  // Continue enqueuing the next request
  //_this->check_requests(is_write);
}


int router_shared_async::build()
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  this->nb_ports = get_config_int("nb_slaves");

  // Define the type of channel (shared or private). By default the channel is private
  broadcast = false;
  // if(get_config_bool("broadcast") != NULL) {
  //   broadcast = get_config_bool("broadcast");
  // }

  in = new vp::io_slave[nb_ports];
  this->nb_entries = 0;
  // Fully connected crossbar
  if(broadcast) {
    this->nb_entries = 1;
  }

  // Initialize the mask of the scheduled ports. It supports up to 32 slave ports and 32 master ports.
  // The MSBs are reserved for the masters, the LSBs for the slaves. Use the SYMBOL (MASK_OFFSET_*) to address them.
  scheduling_write = 0;
  scheduling_read  = 0;

  this->next_packet_port_time = new int64_t[(this->nb_ports * 2)];
  for (int i=0; i<this->nb_ports; i++)
  {
    // Create an empty vector
    vector<Event> pending_write_reqs_port;
    vector<Event> pending_read_reqs_port;
    // Initialize the vector of vectors with empty vectors. Now you can address it using array reference
    pending_write_reqs.push_back(pending_write_reqs_port);
    pending_read_reqs.push_back(pending_read_reqs_port);

    // Create an empty vector
    vector<vp::clock_event *> enqueued_write_resp_port;
    vector<vp::clock_event *> enqueued_read_resp_port;
    // Initialize the vector of vectors with empty vectors. Now you can address it using array reference
    enqueued_write_resp.push_back(enqueued_write_resp_port);
    enqueued_read_resp.push_back(enqueued_read_resp_port);

    // Write channel
    this->next_packet_port_time[i] = 0;
    // Read channel
    this->next_packet_port_time[(i + this->nb_ports)] = 0;

    in[i].set_req_meth_muxed(&router_shared_async::req, i);
    new_slave_port("input_" + std::to_string(i), &in[i]);
  }

  arbiter_handler_meth = &router_shared_async::push_round_robin;

  out.set_resp_meth(&router_shared_async::response);
  out.set_grant_meth(&router_shared_async::grant);
  new_master_port("out", &out);

  bandwidth = get_config_int("bandwidth");
  //latency = get_config_int("latency");
  cmd_latency = get_config_int("cmd_latency");
  resp_latency = get_config_int("resp_latency");

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

  // Channel read and channel write
  this->forwarding_event = new vp::clock_event*[(this->nb_entries * 2)];
  //this->responding_event = new vp::clock_event*[(this->nb_entries * 2)];
  this->next_packet_id_time = new int64_t[(this->nb_entries * 2)];
  for(int i=0; i<this->nb_entries; i++)
  {
    // Write channel
    this->forwarding_event[i] = event_new(router_shared_async::forwarding_handler);
    // is_write
    this->forwarding_event[i]->set_int(0, true);
   // Caller id
    this->forwarding_event[i]->set_int(1, i);
    // Read channel
    this->forwarding_event[(i + this->nb_entries)] = event_new(router_shared_async::forwarding_handler);
    // is_write
    this->forwarding_event[(i + this->nb_entries)]->set_int(0, false);
    // Caller id
    this->forwarding_event[(i + this->nb_entries)]->set_int(1, i);
/*
    // Write channel
    this->responding_event[i] = event_new(router_shared_async::responding_handler);
    // Caller id
    this->responding_event[i]->set_int(0, i);
    // Read channel
    this->responding_event[(i + this->nb_entries)] = event_new(router_shared_async::responding_handler);
    // Caller id
    this->responding_event[(i + this->nb_entries)]->set_int(0, i);
*/
    // Write channel
    this->next_packet_id_time[i] = 0;
    // Read channel
    this->next_packet_id_time[(i + this->nb_entries)] = 0;

    // Create an empty set
    set<Event> pending_write_services_port;
    set<Event> pending_read_services_port;
    // Initialize the vector of sets with empty sets. Now you can address it using array reference
    pending_write_services.push_back(pending_write_services_port);
    pending_read_services.push_back(pending_read_services_port);

    // Create an empty vector
    vector<Event> pending_write_resp_port;
    vector<Event> pending_read_resp_port;
    // Initialize the vector of vectors with empty vectors. Now you can address it using array reference
    pending_write_resp.push_back(pending_write_resp_port);
    pending_read_resp.push_back(pending_read_resp_port);
  }

  return 0;
}


extern "C" vp::component *vp_constructor(js::config *config)
{
  return new router_shared_async(config);
}


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
