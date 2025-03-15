/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include "cpu/iss/include/iss.hpp"


Vector::Vector(Iss &iss)
    : vlsu(iss)
{
}

void Vector::build()
{
    this->vlsu.build();
}

void Vector::reset(bool active)
{
    if (active)
    {
        for (int i = 0; i < ISS_NB_VREGS; i++){
            for (int j = 0; j < NB_VEL; j++){
                this->vregs[i][j] = i == 0 ? 0 : 0x57575757;
            }
        }
    }
}


void Vlsu::data_response(vp::Block *__this, vp::IoReq *req)
{
}


Vlsu::Vlsu(Iss &iss) : iss(iss)
{
}

void Vlsu::build()
{
    for (int i=0; i<4; i++)
    {
        this->io_itf[i].set_resp_meth(&Vlsu::data_response);
        this->iss.top.new_master_port("vlsu_" + std::to_string(i), &this->io_itf[i], (vp::Block *)this);
    }


}
