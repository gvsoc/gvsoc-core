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

class converter : public vp::Component
{

  friend class MapEntry;

public:

    converter(vp::ComponentConf &conf);

  void reset(bool active);


private:

  static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);


  static void grant(vp::Block *__this, vp::IoReq *req);

  static void response(vp::Block *__this, vp::IoReq *req);

  static void event_handler(vp::Block *__this, vp::ClockEvent *event);

  vp::IoReqStatus process_req(vp::IoReq *req);

  vp::IoReqStatus process_pending_req(vp::IoReq *req);

  void check_state();


  vp::Trace     trace;

  vp::IoMaster out;
  vp::IoSlave in;

  int output_width;
  int output_align;

  vp::IoReq *pending_req;
  vp::ClockEvent *event;

  int64_t ready_cycle;
  int ongoing_size;
  vp::IoReq *ongoing_req;
  vp::IoReq *stalled_req;
  vp::IoReq *last_stalled_req;
};

converter::converter(vp::ComponentConf &config)
: vp::Component(config)
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  in.set_req_meth(&converter::req);
  new_slave_port("input", &in);

  out.set_resp_meth(&converter::response);
  out.set_grant_meth(&converter::grant);
  new_master_port("out", &out);

  output_width = get_js_config()->get_child_int("output_width");
  output_align = get_js_config()->get_child_int("output_align");

  event = event_new(converter::event_handler);

}


void converter::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
  converter *_this = (converter *)__this;
  vp::IoReq *req = _this->pending_req;
  _this->pending_req = req->get_next();

  _this->trace.msg("Sending partial packet (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
    req, req->get_addr(), req->get_size(), req->get_is_write());

  vp::IoReqStatus err = _this->out.req(req);
  if (err == vp::IO_REQ_OK)
  {
    _this->ready_cycle = _this->clock.get_cycles() + req->get_latency() + 1;
    _this->ongoing_size -= req->get_size();
    if (_this->ongoing_size == 0)
    {
      vp::IoReq *req = _this->ongoing_req;
      _this->trace.msg("Finished handling request (req: %p)\n", req);
      _this->ongoing_req = NULL;
      req->set_latency(req->get_latency() + 1);
      req->get_resp_port()->resp(req);

      if (_this->stalled_req)
      {
        req = _this->stalled_req;
        _this->trace.msg("Unstalling request (req: %p)\n", req);
        _this->stalled_req = req->get_next();
        req->get_resp_port()->grant(req);

        _this->process_pending_req(req);
      }
    }
  }
  else
  {
    _this->ready_cycle = INT32_MAX;
  }

  _this->check_state();
}

void converter::check_state()
{
  if (pending_req)
  {
    int64_t cycle = clock.get_cycles();
    int64_t latency = 1;
    if (ready_cycle > cycle) latency = ready_cycle - cycle;
    if (!event->is_enqueued()) event_enqueue(event, latency);
  }
}

vp::IoReqStatus converter::process_pending_req(vp::IoReq *req)
{
  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  bool is_write = req->get_is_write();

  int mask = output_align - 1;

  ongoing_req = req;
  ongoing_size = size;

  while (size)
  {
    int iter_size = output_width;
    if (offset & mask) iter_size -= offset & mask;
    if (iter_size > size) iter_size = size;

    vp::IoReq *req = out.req_new(offset, data, iter_size, is_write);
    req->set_next(pending_req);
    pending_req = req;


    size -= iter_size;
    offset += iter_size;
    data += iter_size;
  }

  return vp::IO_REQ_PENDING;
}

vp::IoReqStatus converter::process_req(vp::IoReq *req)
{
  uint64_t offset = req->get_addr();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  bool is_write = req->get_is_write();

  int mask = output_align - 1;

  // Simple case where the request fit, just forward it
  if ((offset & ~mask) == ((offset + size - 1) & ~mask))
  {
    trace.msg("No conversion applied, forwarding request (req: %p)\n", req);
    return out.req_forward(req);
  }

  return this->process_pending_req(req);
}

vp::IoReqStatus converter::req(vp::Block *__this, vp::IoReq *req)
{
  converter *_this = (converter *)__this;
  uint64_t offset = req->get_addr();
  bool is_write = req->get_is_write();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();

  _this->trace.msg("Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", req, offset, size, is_write);

  if (_this->ongoing_req)
  {
    _this->trace.msg("Stalling request (req: %p)\n", req);

    if (_this->stalled_req)
      _this->last_stalled_req->set_next(req);
    else
      _this->stalled_req = req;
    req->set_next(NULL);
    _this->last_stalled_req = req;

    return vp::IO_REQ_DENIED;
  }

  if (_this->process_req(req) == vp::IO_REQ_OK)
    return vp::IO_REQ_OK;

  _this->check_state();

  return vp::IO_REQ_PENDING;
}

void converter::grant(vp::Block *__this, vp::IoReq *req)
{
}

void converter::response(vp::Block *__this, vp::IoReq *req)
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new converter(config);
}


void converter::reset(bool active)
{
  if (active)
  {
    pending_req = NULL;
    ready_cycle = 0;
    ongoing_req = NULL;
    ongoing_size = 0;
    stalled_req = NULL;
  }
}
