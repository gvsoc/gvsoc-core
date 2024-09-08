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

#include "vp/memcheck.hpp"

void vp::MemCheck::register_memory(int mem_id, vp::WireSlave<MemCheckRequest *> *itf)
{
    this->memories[mem_id] = new vp::WireMaster<MemCheckRequest *>();
    this->memories[mem_id]->bind_to(itf, NULL);
}

uint64_t vp::MemCheck::alloc(int mem_id, uint64_t offset, uint64_t size)
{
    if (this->memories.count(mem_id) != 0)
    {
        MemCheckRequest req = { .is_alloc=true, .offset=offset, .size=size};
        this->memories[mem_id]->sync(&req);
        return req.offset;
    }
    return 0;
}

uint64_t vp::MemCheck::free(int mem_id, uint64_t offset, uint64_t size)
{
    if (this->memories.count(mem_id) != 0)
    {
        MemCheckRequest req = { .is_alloc=false, .offset=offset, .size=size};
        this->memories[mem_id]->sync(&req);
        return req.offset;
    }
    return 0;
}