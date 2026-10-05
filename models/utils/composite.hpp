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



namespace vp
{

    class Composite : public vp::Component
    {

    public:
        // Defined inline so that composites subclassed in other models do not import it from
        // this model, which only exports its entry point.
        Composite(vp::ComponentConf &config) : vp::Component(config) {}

        // Forwarding constructor for composite subclasses using a compiled config
        // struct (e.g. to declare power sources from generated power tables)
        template<typename T>
        Composite(vp::ComponentConf &config, T &cfg) : vp::Component(config, cfg) {}

    protected:
        vp::Trace     trace;
    };

};