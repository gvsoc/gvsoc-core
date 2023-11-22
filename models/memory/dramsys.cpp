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
#include <systemc.h>

class ddr : public vp::Component
{

public:
    ddr(vp::ComponentConf &conf);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

private:
    vp::Trace trace;
    vp::IoSlave in;
};

ddr::ddr(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in.set_req_meth(&ddr::req);
    new_slave_port("input", &in);

}

vp::IoReqStatus ddr::req(vp::Block *__this, vp::IoReq *req)
{
    ddr *_this = (ddr *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    _this->trace.msg("IO access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

    return vp::IO_REQ_OK;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ddr(config);
}
