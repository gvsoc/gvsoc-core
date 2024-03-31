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

#include <string>
#include <vp/vp.hpp>
#include "vp/top.hpp"

vp::Top::Top(std::string config_path, bool is_async)
{
    js::Config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        throw std::invalid_argument("Invalid configuration.");
    }

    this->gv_config = js_config->get("target/gvsoc");

    this->time_engine = new vp::TimeEngine(this->gv_config);
    this->trace_engine = new vp::TraceEngine(this->gv_config);
    this->power_engine = new vp::PowerEngine();

    this->top_instance = vp::Component::load_component(js_config->get("**/target"), this->gv_config,
        NULL, "", this->time_engine, this->trace_engine, this->power_engine);

    power_engine->init(this->top_instance);
    trace_engine->init(this->top_instance);
    time_engine->init(this->top_instance);
}


void vp::Top::flush()
{
    this->trace_engine->flush();
    this->time_engine->flush();
}

void vp::Top::start()
{
    this->trace_engine->start();
}

vp::Top::~Top()
{
    delete this->power_engine;
    delete this->trace_engine;
}