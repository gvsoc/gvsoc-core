/*
 * Copyright (C) 2020  GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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

#include <string.h>

namespace vp {

    class MappingTree;

    class MappingTreeEntry
    {
        friend class MappingTree;

    public:
        MappingTreeEntry(int id, std::string name, js::Config *config);
        MappingTreeEntry(uint64_t base, MappingTreeEntry *left, MappingTreeEntry *right);

        std::string name;
        int id;
        uint64_t base = 0;
        uint64_t size = 0;

    private:
        MappingTreeEntry *next = NULL;
        uint64_t lowest_base = 0;
        MappingTreeEntry *left = NULL;
        MappingTreeEntry *right = NULL;
    };

    class MappingTree
    {
    public:
        MappingTree(vp::Trace *trace);
        void insert(int id, std::string name, js::Config *config);
        void build();
        MappingTreeEntry *get(uint64_t base, uint64_t size, bool is_write);

    private:
        vp::Trace *trace;
        MappingTreeEntry *first_map_entry = NULL;
        MappingTreeEntry *top_map_entry = NULL;
        MappingTreeEntry *default_entry = NULL;
        MappingTreeEntry *error_entry = NULL;
    };
};
