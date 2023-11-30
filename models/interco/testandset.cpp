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

class testandset : public vp::Component
{

public:

    testandset(vp::ComponentConf &conf);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

private:
    vp::Trace     trace;

    vp::IoMaster out;
    vp::IoSlave in;
    vp::IoReq ts_req;
};

testandset::testandset(vp::ComponentConf &config)
: vp::Component(config)
{
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_req_meth(&testandset::req);
    this->new_slave_port("input", &this->in);

    this->new_master_port("output", &this->out);


}

vp::IoReqStatus testandset::req(vp::Block *__this, vp::IoReq *req)
{
    testandset *_this = (testandset *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();

    _this->trace.msg("Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d)\n", offset, size, is_write);

    vp::IoReqStatus err = _this->out.req_forward(req);
    if (err != vp::IO_REQ_OK) return err;

    if (!is_write)
    {
        _this->trace.msg("Sending test-and-set IO req (offset: 0x%llx, size: 0x%llx)\n", offset & ~(1<<20), size);
        uint64_t ts_data = -1;
        _this->ts_req.init();
        _this->ts_req.set_addr(offset);
        _this->ts_req.set_size(size);
        _this->ts_req.set_is_write(true);
        _this->ts_req.set_data((uint8_t *)&ts_data);
        return _this->out.req(&_this->ts_req);
    }

    return vp::IO_REQ_OK;
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new testandset(config);
}


