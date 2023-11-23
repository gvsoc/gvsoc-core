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
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <string.h>

class Switch : public vp::Component
{

public:
    Switch(vp::ComponentConf &conf);

    void reset(bool active);

private:
    static void sync(vp::Block *__this, int value);

    vp::WireSlave<int> in_itf;
    int value;
};

Switch::Switch(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->in_itf.set_sync_meth(&Switch::sync);
    this->new_slave_port("input", &this->in_itf);

    this->value = this->get_js_config()->get_child_int("value");
}

void Switch::sync(vp::Block *__this, int value)
{

}

void Switch::reset(bool active)
{
    if (!active)
    {
        if (this->in_itf.is_bound())
        {
            this->in_itf.sync(this->value);
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Switch(config);
}
