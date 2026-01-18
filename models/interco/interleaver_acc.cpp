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
#include <vp/itf/io_acc.hpp>
#include <stdio.h>
#include <math.h>

class interleaver : public vp::Component
{

public:

    interleaver(vp::ComponentConf &conf);

  static vp::IoAccReqStatus req(vp::Block *__this, vp::IoAccReq *req);


  static void retry(vp::Block *__this);

  static void response(vp::Block *__this, vp::IoAccReq *req);

private:
  vp::Trace     trace;

  vp::IoAccMaster **out;
  vp::IoAccSlave **masters_in;
  vp::IoAccSlave in;

  int nb_slaves;
  int nb_masters;
  int interleaving_bits;
  int stage_bits;
  int enable_shift;
  uint64_t offset_mask;
  uint64_t remove_offset;
  bool offset_translation;
};

interleaver::interleaver(vp::ComponentConf &config)
: vp::Component(config), in(&interleaver::req)
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  new_slave_port("input", &in);

  nb_slaves = get_js_config()->get_child_int("nb_slaves");
  nb_masters = get_js_config()->get_child_int("nb_masters");
  stage_bits = get_js_config()->get_child_int("stage_bits");
  interleaving_bits = get_js_config()->get_child_int("interleaving_bits");
  remove_offset = get_js_config()->get_child_int("remove_offset");
  enable_shift = get_js_config()->get_child_int("enable_shift");
  offset_translation = get_js_config()->get_child_bool("offset_translation");

  if (stage_bits == 0)
  {
    stage_bits = log2(nb_slaves);
  }

  offset_mask = -1;
  offset_mask &= ~((1 << (interleaving_bits + stage_bits)) - 1);

  out = new vp::IoAccMaster *[nb_slaves];
  for (int i=0; i<nb_slaves; i++)
  {
    out[i] = new vp::IoAccMaster(&interleaver::retry, &interleaver::response);
    new_master_port("out_" + std::to_string(i), out[i]);
  }

  masters_in = new vp::IoAccSlave *[nb_masters];
  for (int i=0; i<nb_masters; i++)
  {
    masters_in[i] = new vp::IoAccSlave(&interleaver::req);
    new_slave_port("in_" + std::to_string(i), masters_in[i]);
  }

}

vp::IoAccReqStatus interleaver::req(vp::Block *__this, vp::IoAccReq *req)
{
  interleaver *_this = (interleaver *)__this;
  uint64_t offset = req->get_addr();
  bool is_write = req->get_is_write();
  uint64_t size = req->get_size();
  uint8_t *data = req->get_data();

  uint8_t *init_data = data;
  uint64_t init_size = size;
  uint64_t init_offset = offset;

  _this->trace.msg("Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
      offset, size, is_write);

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
    uint64_t new_offset;
    if (_this->offset_translation)
    {
        new_offset = ((offset & _this->offset_mask) >> _this->stage_bits) + (offset & ((1<<_this->interleaving_bits)-1));
    }
    else if (_this->enable_shift)
    {
      output_id = (offset >> (_this->interleaving_bits + _this->enable_shift)) & ((1 << _this->stage_bits) - 1);
      new_offset = ((offset >> _this->enable_shift) & (-1ULL << _this->interleaving_bits)) | (offset & ((1ULL << _this->interleaving_bits) - 1));
    }
    else
    {
      new_offset = offset;
    }

    _this->trace.msg("Forwarding interleaved packet (port: %d, offset: 0x%x, size: 0x%x)\n", output_id, new_offset, loop_size);

    if (!_this->out[output_id])
    {
        req->status = vp::IO_ACC_RESP_INVALID;
        return vp::IO_ACC_REQ_DONE;
    }

    req->set_addr(new_offset);
    req->set_size(loop_size);
    req->set_data(data);

    // vp::IoAccReqStatus err = _this->out[output_id]->req_forward(req);
    // if (err != vp::vp::IoAccReq_OK)
    // {
    //   // Temporary hack, until this component supports asynchronous requests.
    //   // If we get an asynchronous reply and we are done, just return
    //   if (err == vp::vp::IoAccReq_PENDING && size == loop_size)
    //   {
    //     return vp::vp::IoAccReq_PENDING;
    //   }
    //   else if(err == vp::vp::IoAccReq_DENIED && size == loop_size)
    //   {
    //     return vp::vp::IoAccReq_DENIED;
    //   }
    //   else
    //   {
    //     return vp::vp::IoAccReq_INVALID;
    //   }
    // }

    size -= loop_size;
    offset += loop_size;
    if (data)
      data += loop_size;
  }

  req->set_addr(init_offset);
  req->set_size(init_size);
  req->set_data(init_data);

  req->status = vp::IO_ACC_RESP_OK;
  return vp::IO_ACC_REQ_DONE;
}

void interleaver::retry(vp::Block *__this)
{

}

void interleaver::response(vp::Block *__this, vp::IoAccReq *req)
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new interleaver(config);
}
