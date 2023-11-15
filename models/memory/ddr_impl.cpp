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
#include <string.h>

class ddr : public vp::Component
{

  friend class gvsoc_tlm_br;

public:

  ddr(vp::ComponentConf &conf);

  static vp::IoReqStatus req(void *__this, vp::IoReq *req);

protected:
  vp::IoReq *first_pending_reqs = NULL;
  vp::IoReq *first_stalled_req = NULL;

private:

  vp::Trace     trace;
  vp::IoSlave in;

  uint64_t size = 0;
  int max_reqs = 4;
  int current_reqs = 0;
  int count = 0;
  vp::IoReq *last_pending_reqs = NULL;
};/* vim: set ts=2 sw=2 expandtab:*/
/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */


ddr::ddr(vp::ComponentConf &config)
: vp::Component(config)
{
  traces.new_trace("trace", &trace, vp::DEBUG);

  in.set_req_meth(&ddr::req);
  new_slave_port("input", &in);

  size = get_js_config()->get_child_int("size");

  trace.msg("Building ddr (size: 0x%lx)\n", size);


}

vp::IoReqStatus ddr::req(void *__this, vp::IoReq *req)
{
  ddr *_this = (ddr *)__this;

  uint64_t offset = req->get_addr();
  uint8_t *data = req->get_data();
  uint64_t size = req->get_size();

  _this->trace.msg("ddr access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

  if (offset + size > _this->size) {
    _this->warning.warning("Received out-of-bound request (reqAddr: 0x%llx, reqSize: 0x%llx, memSize: 0x%llx)\n", offset, size, _this->size);
    return vp::IO_REQ_INVALID;
  }

  if (_this->first_pending_reqs)
    _this->last_pending_reqs->set_next(req);
  else
    _this->first_pending_reqs = req;
  _this->last_pending_reqs = req;
  req->set_next(NULL);

  _this->current_reqs++;

  if (_this->current_reqs > _this->max_reqs)
  {
    if (_this->first_stalled_req == NULL)
      _this->first_stalled_req = _this->last_pending_reqs;
    return vp::IO_REQ_DENIED;
  }
  else
  {
    return vp::IO_REQ_PENDING;
  }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new ddr(config);
}

