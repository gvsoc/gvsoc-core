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
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>


class Plic : public vp::component
{
public:
    Plic(js::config *config);

    int build();

private:
    static vp::io_req_status_e req(void *__this, vp::io_req *req);

    vp::io_slave input_itf;
};



int Plic::build()
{
    this->input_itf.set_req_meth(&Plic::req);
    new_slave_port("input", &this->input_itf);

    return 0;
}


vp::io_req_status_e Plic::req(void *__this, vp::io_req *req)
{
    Plic *_this = (Plic *)__this;

    return vp::IO_REQ_OK;
}


Plic::Plic(js::config *config)
    : vp::component(config)
{
}


extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Plic(config);
}
