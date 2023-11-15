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

vp::top::top(std::string config_path, bool is_async)
{
    js::Config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        throw std::invalid_argument("Invalid configuration.");
    }

    this->gv_config = js_config->get("target/gvsoc");

    trace_manager = new vp::TraceEngine(this->gv_config);
    power_engine = new vp::PowerEngine();
    gv_time_engine = new vp::TimeEngine(this->gv_config);

    this->top_instance = vp::Component::load_component(js_config->get("**/target"), this->gv_config, NULL, "");

    power_engine->init(this->top_instance);
    trace_manager->init(this->top_instance);
    gv_time_engine->init(this->top_instance);
}


vp::top::~top()
{
    delete power_engine;
    delete trace_manager;
}