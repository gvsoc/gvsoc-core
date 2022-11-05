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

class Switch : public vp::component
{

public:
    Switch(js::config *config);

    int build();
    void start();

private:
    static void sync(void *__this, int value);

    vp::wire_slave<int> in_itf;
    int value;
};

Switch::Switch(js::config *config)
    : vp::component(config)
{
}

void Switch::sync(void *__this, int value)
{

}

int Switch::build()
{
    this->in_itf.set_sync_meth(&Switch::sync);
    this->new_slave_port("input", &this->in_itf);

    this->value = this->get_config_int("value");

    return 0;
}

void Switch::start()
{
    if (this->in_itf.is_bound())
    {
        this->in_itf.sync(this->value);
    }
}

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Switch(config);
}
