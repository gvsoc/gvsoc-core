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
#include <cpu/iss/include/types.hpp>
#include <memory/memory_memcheck.hpp>

class Memcheck
{
public:
    Memcheck(IssWrapper &top, Iss &iss);
    iss_reg_t mem_alloc(iss_reg_t mem_id, iss_reg_t ptr, iss_reg_t size);
    iss_reg_t mem_free(iss_reg_t mem_id, iss_reg_t ptr, iss_reg_t size);

private:
    MemoryMemcheck *get_memory(iss_reg_t mem_id);

    Iss &iss;
    vp::Trace trace;

    int nb_mem_itf;
    int expansion_factor;

    std::map<iss_reg_t, MemoryMemcheck *> memories;
    std::vector<vp::WireMaster<MemoryMemcheckBuffer *>> mem_itfs;
};
