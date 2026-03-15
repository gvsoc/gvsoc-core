/*
 * Copyright (C) 2026 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace vp {

/**
 * @brief A port-to-port binding between two components.
 */
struct TreeBinding
{
    const char *master_comp;    // "self" or child name
    const char *master_port;    // Port name on master
    const char *slave_comp;     // "self" or child name
    const char *slave_port;     // Port name on slave
};

/**
 * @brief Node in the compiled component tree.
 *
 * Generated per-target by Python. Describes the hierarchy of components
 * with their typed configurations, module names, and bindings.
 * The engine walks this tree when instantiating the system.
 */
struct ComponentTreeNode
{
    const char *name;                    // Component name (e.g. "rom", "bank_0")
    const void *config;                  // Pointer to constexpr config struct, or nullptr
    const ComponentTreeNode *children;   // Array of child nodes (nullptr if leaf)
    int num_children;                    // Number of children
    const char *vp_component;            // Module name (.so), or nullptr for default
    const TreeBinding *bindings;         // Array of bindings, or nullptr
    int num_bindings;                    // Number of bindings

    /**
     * @brief Find a child node by name.
     * @return Pointer to the child node, or nullptr if not found.
     */
    const ComponentTreeNode *find_child(const char *child_name) const
    {
        if (children == nullptr) return nullptr;
        for (int i = 0; i < num_children; i++)
        {
            if (strcmp(children[i].name, child_name) == 0)
                return &children[i];
        }
        return nullptr;
    }
};

} // namespace vp

/**
 * @brief Entry point for the per-target compiled tree.
 *
 * Each target .so exports this function. Returns the root of the tree.
 * Returns nullptr if the target has no compiled tree.
 */
extern "C" const vp::ComponentTreeNode *vp_get_platform_tree();
