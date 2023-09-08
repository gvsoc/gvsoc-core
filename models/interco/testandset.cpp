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

class testandset : public vp::component
{

public:

    testandset(js::config *config);

    int build();

    static vp::io_req_status_e req(void *__this, vp::io_req *req);

private:
    vp::trace     trace;

    vp::io_master out;
    vp::io_slave in;
    vp::io_req ts_req;
};

testandset::testandset(js::config *config)
: vp::component(config)
{

}

vp::io_req_status_e testandset::req(void *__this, vp::io_req *req)
{
    testandset *_this = (testandset *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();

    _this->trace.msg("Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, is_write);

    vp::io_req_status_e err = _this->out.req_forward(req);
    if (err != vp::IO_REQ_OK) return err;

    if (!is_write)
    {
        _this->trace.msg("Sending test-and-set IO req (offset: 0x%llx, size: 0x%llx)\n", offset & ~(1<<20), size);
        uint64_t ts_data = -1;
        _this->ts_req.set_addr(offset);
        _this->ts_req.set_size(size);
        _this->ts_req.set_is_write(true);
        _this->ts_req.set_data((uint8_t *)&ts_data);
        return _this->out.req(&_this->ts_req);
    }

    return vp::IO_REQ_OK;
}



int testandset::build()
{
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_req_meth(&testandset::req);
    this->new_slave_port("input", &this->in);

    this->new_master_port("output", &this->out);

  return 0;
}



extern "C" vp::component *vp_constructor(js::config *config)
{
    return new testandset(config);
}


