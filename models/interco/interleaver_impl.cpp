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

class interleaver : public vp::Component
{

public:

    interleaver(vp::ComponentConf &conf);

  static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);


  static void grant(vp::Block *__this, vp::IoReq *req);

  static void response(vp::Block *__this, vp::IoReq *req);

private:
  vp::Trace     trace;

  vp::IoMaster **out;
  vp::IoSlave **masters_in;
  vp::IoSlave in;

  int nb_slaves;
  int nb_masters;
  int interleaving_bits;
  int stage_bits;
  uint64_t offset_mask;
  uint64_t remove_offset;
};

interleaver::interleaver(vp::ComponentConf &config)
: vp::Component(config)
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  in.set_req_meth(&interleaver::req);
  new_slave_port("input", &in);

  nb_slaves = get_js_config()->get_child_int("nb_slaves");
  nb_masters = get_js_config()->get_child_int("nb_masters");
  stage_bits = get_js_config()->get_child_int("stage_bits");
  interleaving_bits = get_js_config()->get_child_int("interleaving_bits");
  remove_offset = get_js_config()->get_child_int("remove_offset");

  if (stage_bits == 0)
  {
    stage_bits = log2(nb_slaves);
  }

  offset_mask = -1;
  offset_mask &= ~((1 << (interleaving_bits + stage_bits)) - 1);

  out = new vp::IoMaster *[nb_slaves];
  for (int i=0; i<nb_slaves; i++)
  {
    out[i] = new vp::IoMaster();
    out[i]->set_resp_meth(&interleaver::response);
    out[i]->set_grant_meth(&interleaver::grant);
    new_master_port("out_" + std::to_string(i), out[i]);
  }

  masters_in = new vp::IoSlave *[nb_masters];
  for (int i=0; i<nb_masters; i++)
  {
    masters_in[i] = new vp::IoSlave();
    masters_in[i]->set_req_meth(&interleaver::req);
    new_slave_port("in_" + std::to_string(i), masters_in[i]);
  }

}

vp::IoReqStatus interleaver::req(vp::Block *__this, vp::IoReq *req)
{
  interleaver *_this = (interleaver *)__this;
  uint64_t offset = req->get_addr();
  bool is_write = req->get_is_write();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();
  int64_t latency = req->get_latency();

  uint8_t *init_data = data;
  uint64_t init_size = size;
  uint64_t init_offset = offset;

  _this->trace.msg("Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, is_write);
 
  int port_size = 1<<_this->interleaving_bits;
  int align_size = offset & (port_size - 1);
  if (align_size) align_size = port_size - align_size;

  offset -= _this->remove_offset;

  while(size) {
    
    int loop_size = port_size;
    if (align_size) {
      loop_size = align_size;
      align_size = 0;
    }
    if (loop_size > size) loop_size = size;

    int output_id = (offset >> _this->interleaving_bits) & ((1 << _this->stage_bits) - 1);
    uint64_t new_offset = ((offset & _this->offset_mask) >> _this->stage_bits) + (offset & ((1<<_this->interleaving_bits)-1));

    _this->trace.msg("Forwarding interleaved packet (port: %d, offset: 0x%x, size: 0x%x)\n", output_id, new_offset, loop_size);

    if (!_this->out[output_id]) return vp::IO_REQ_INVALID;

    req->set_addr(new_offset);
    req->set_size(loop_size);
    req->set_data(data);
    req->set_latency(0);

    vp::IoReqStatus err = _this->out[output_id]->req_forward(req);
    if (err != vp::IO_REQ_OK)
    {
      // Temporary hack, until this component supports asynchronous requests.
      // If we get an asynchronous reply and we are done, just return
      if (err == vp::IO_REQ_PENDING && size == loop_size)
      {
        return vp::IO_REQ_PENDING;
      }
      else
      {
        return vp::IO_REQ_INVALID;
      }
    }
    
    int64_t iter_latency = req->get_latency();
    if (iter_latency > latency)
    {
      latency = iter_latency;
    }
    
    size -= loop_size;
    offset += loop_size;
    if (data)
      data += loop_size;
  }

  req->set_addr(init_offset);
  req->set_size(init_size);
  req->set_data(init_data);
  req->set_latency(latency);


  return vp::IO_REQ_OK;
}

void interleaver::grant(vp::Block *__this, vp::IoReq *req)
{

}

void interleaver::response(vp::Block *__this, vp::IoReq *req)
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new interleaver(config);
}


