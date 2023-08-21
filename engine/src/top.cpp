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
    js::config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        throw std::invalid_argument("Invalid configuration.");
    }

    js::config *gv_config = js_config->get("target/gvsoc");
    vp::component *instance = vp::component::load_component(js_config->get("**/target"), gv_config);


    this->top_instance = instance;
    this->power_engine = new vp::power::engine(instance);
    this->trace_engine = new vp::trace_domain(instance, gv_config);
    this->time_engine = new vp::time_engine(instance, gv_config);
    this->trace_engine->time_engine = (vp::time_engine *)this->time_engine;

    instance->set_vp_config(gv_config);

    instance->build_instance("", NULL);
}


vp::top::~top()
{
    delete this->power_engine;
    delete this->trace_engine;
}