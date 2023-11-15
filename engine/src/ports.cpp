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


vp::MasterPort::MasterPort(vp::Component *owner)
    : vp::Port(owner)
{
}


void vp::MasterPort::final_bind()
{
    if (this->is_bound)
    {
        this->finalize();
    }
}

void vp::SlavePort::final_bind()
{
    if (this->is_bound)
        this->finalize();
}


std::vector<vp::SlavePort *> vp::SlavePort::get_final_ports()
{
    return { this };
}



std::vector<vp::SlavePort *> vp::MasterPort::get_final_ports()
{
    std::vector<vp::SlavePort *> result;

    for (auto x : this->SlavePorts)
    {
        std::vector<vp::SlavePort *> SlavePorts = x->get_final_ports();
        result.insert(result.end(), SlavePorts.begin(), SlavePorts.end());
    }

    return result;
}



void vp::MasterPort::bind_to_slaves()
{
    for (auto x : this->SlavePorts)
    {
        for (auto y : x->get_final_ports())
        {
            this->get_owner()->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating final binding (%s:%s -> %s:%s)\n",
                this->get_owner()->get_path().c_str(), this->get_name().c_str(),
                y->get_owner()->get_path().c_str(), y->get_name().c_str()
            );

            this->bind_to(y, NULL);
            y->bind_to(this, NULL);
        }
    }
}


void vp::MasterPort::bind_to_virtual(vp::Port *port)
{
    vp_assert_always(port != NULL, this->get_comp()->get_trace(), "Trying to bind master port to NULL\n");
    this->SlavePorts.push_back(port);
}
