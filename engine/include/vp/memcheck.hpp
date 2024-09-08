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

#include <stdint.h>
#include "vp/vp.hpp"
#include "vp/itf/wire.hpp"

namespace vp
{
    class MemCheckRequest
    {
    public:
        bool is_alloc;
        uint64_t offset;
        uint64_t size;
    };

    class MemCheck
    {
    public:
        void register_memory(int mem_id, vp::WireSlave<MemCheckRequest *> *itf);
        uint64_t alloc(int mem_id, uint64_t offset, uint64_t size);
        uint64_t free(int mem_id, uint64_t offset, uint64_t size);

    private:
        std::map<int, vp::WireMaster<MemCheckRequest *> *> memories;
    };

    vp::MemCheck *get_memcheck();
};