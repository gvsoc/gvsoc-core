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

#pragma once

#include <vp/vp.hpp>



class composite : public vp::component
{

public:
    composite(js::config *config);

    vp::port *get_slave_port(std::string name) { return this->ports[name]; }
    vp::port *get_master_port(std::string name) { return this->ports[name]; }

    void add_slave_port(std::string name, vp::slave_port *port) { this->add_port(name, port); }
    void add_master_port(std::string name, vp::master_port *port) { this->add_port(name, port); }

    int build();
    void start();
    void power_supply_set(int state);

    void dump_traces(FILE *file);

protected:
    vp::trace     trace;

private:
    void add_port(std::string name, vp::port *port);
    std::map<std::string, vp::port *> ports;
};
