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

class Reset : public vp::Component
{

public:
    Reset(vp::ComponentConf &conf);

    void reset(bool active);

private:
    vp::WireMaster<int> reset_itf;
};

Reset::Reset(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->new_master_port("reset_gen", &this->reset_itf);
}

void Reset::reset(bool active)
{
    this->reset_itf.sync(active);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Reset(config);
}
