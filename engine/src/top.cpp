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

#include <filesystem>
#include <string>
#include <stdexcept>
#include <dlfcn.h>
#include <vp/vp.hpp>
#include <vp/component_tree.hpp>
#include "vp/top.hpp"

vp::Top::Top(std::string config_path, std::string runtime_config_path, bool is_async,
    gv::Controller *launcher)
{
    js::Config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        throw std::invalid_argument("Invalid configuration.");
    }

    this->gv_config = js_config->get("target/gvsoc");

    // Load the per-run runtime config values, overlaid onto the compiled
    // tree configs at component construction. Launchers which do not forward
    // the --runtime-config option (e.g. the GUI) get the path from the
    // gvsoc config instead.
    if (runtime_config_path == "")
    {
        js::Config *runtime_config_cfg = this->gv_config->get("runtime_config");
        if (runtime_config_cfg != nullptr)
        {
            runtime_config_path = runtime_config_cfg->get_str();

            // The runner dumps the file next to the gvsoc config and stores
            // a work-dir-relative name, counting on the engine running from
            // the work dir. Launchers like the GUI run from anywhere, so
            // resolve a relative name against the config's own directory
            // (falling back to the plain cwd-relative name when not there,
            // since a missing runtime config silently applies the baked
            // defaults).
            if (!runtime_config_path.empty() && runtime_config_path[0] != '/')
            {
                std::filesystem::path resolved =
                    std::filesystem::path(config_path).parent_path()
                    / runtime_config_path;
                if (std::filesystem::exists(resolved))
                {
                    runtime_config_path = resolved.string();
                }
            }
        }
    }
    this->runtime_config = new vp::RuntimeConfig();
    if (runtime_config_path != "")
    {
        this->runtime_config->load(runtime_config_path);
    }

    // Load the per-target compiled tree if available
    const vp::ComponentTreeNode *platform_tree = nullptr;
    js::Config *tree_lib_cfg = this->gv_config->get("platform_tree");
    if (tree_lib_cfg != nullptr)
    {
        std::string tree_lib_path = tree_lib_cfg->get_str();
        void *tree_lib = dlopen(tree_lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (tree_lib == nullptr)
        {
            throw std::runtime_error("Failed to load platform tree " + tree_lib_path + ": " +
                dlerror());
        }

        // A tree built against another layout of the tree structures would be
        // misread, the target must be rebuilt
        auto get_version = (int (*)())dlsym(tree_lib, "vp_platform_tree_version");
        int version = get_version != nullptr ? get_version() : 1;
        if (version != VP_COMPONENT_TREE_VERSION)
        {
            throw std::runtime_error("Platform tree " + tree_lib_path + " has layout version " +
                std::to_string(version) + " while the engine expects " +
                std::to_string(VP_COMPONENT_TREE_VERSION) + ", rebuild the target");
        }

        auto get_tree = (const vp::ComponentTreeNode *(*)())dlsym(tree_lib, "vp_get_platform_tree");
        if (get_tree == nullptr)
        {
            throw std::runtime_error("Platform tree " + tree_lib_path +
                " has no vp_get_platform_tree entry point");
        }
        platform_tree = get_tree();
    }

    this->time_engine = new vp::TimeEngine(this->gv_config);
    this->trace_engine = new vp::TraceEngine(this->gv_config);
    this->power_engine = new vp::PowerEngine(this->gv_config);
    this->stats_engine = new vp::StatsEngine(this->gv_config);
    this->memcheck = new vp::MemCheck(launcher);

    this->top_instance = vp::Component::load_component(js_config->get("**/target"), this->gv_config,
        NULL, "", this->time_engine, this->trace_engine, this->power_engine, this->memcheck,
        platform_tree, this->stats_engine, this->runtime_config);

    // The whole tree is built, any leftover runtime config key is a typo or
    // a stale file. Only checked when the compiled tree was used: the JSON
    // instantiation fallback has no runtime descriptors and consumes nothing.
    if (platform_tree != nullptr)
    {
        this->runtime_config->check_all_consumed();
    }

    power_engine->init(this->top_instance);
    trace_engine->init(this->top_instance);
    time_engine->init(this->top_instance);
    stats_engine->init(this->top_instance);
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
    delete this->runtime_config;
    delete this->stats_engine;
    delete this->power_engine;
    delete this->trace_engine;
    delete this->time_engine;
    delete this->memcheck;
}
