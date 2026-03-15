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
#include <dlfcn.h>
#include <vp/vp.hpp>
#include <vp/component_tree.hpp>
#include "vp/top.hpp"

vp::Top::Top(std::string config_path, bool is_async, gv::Controller *launcher)
{
    js::Config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        throw std::invalid_argument("Invalid configuration.");
    }

    this->gv_config = js_config->get("target/gvsoc");

    // Load the per-target compiled tree if available
    const vp::ComponentTreeNode *platform_tree = nullptr;
    js::Config *tree_lib_cfg = this->gv_config->get("platform_tree");
    if (tree_lib_cfg != nullptr)
    {
        std::string tree_lib_path = tree_lib_cfg->get_str();
        void *tree_lib = dlopen(tree_lib_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (tree_lib != nullptr)
        {
            auto get_tree = (const vp::ComponentTreeNode *(*)())dlsym(tree_lib, "vp_get_platform_tree");
            if (get_tree != nullptr)
            {
                platform_tree = get_tree();
            }
        }
    }

    this->time_engine = new vp::TimeEngine(this->gv_config);
    this->trace_engine = new vp::TraceEngine(this->gv_config);
    this->power_engine = new vp::PowerEngine(this->gv_config);
    this->memcheck = new vp::MemCheck();

    this->top_instance = vp::Component::load_component(js_config->get("**/target"), this->gv_config,
        NULL, "", this->time_engine, this->trace_engine, this->power_engine, this->memcheck,
        platform_tree);

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
