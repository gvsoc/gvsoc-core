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

#include "cpu/iss/include/iss.hpp"



Memcheck::Memcheck(IssWrapper &top, Iss &iss)
: iss(iss)
{
    top.traces.new_trace("memcheck", &this->trace, vp::DEBUG);
    this->nb_mem_itf = top.get_js_config()->get_int("memcheck/nb_memories");
    this->mem_itfs.resize(this->nb_mem_itf);
    this->expansion_factor = 5;
    for (int i=0; i<nb_mem_itf; i++)
    {
        top.new_master_port("memcheck_mem_" + std::to_string(i), &this->mem_itfs[i],
            (vp::Block *)this);
    }
}


MemoryMemcheck *Memcheck::get_memory(iss_reg_t mem_id)
{
    if (this->memories.count(mem_id) == 0)
    {
        MemoryMemcheck *memory = (MemoryMemcheck *)this->iss.top.get_service(
            "memcheck_memory" + std::to_string(mem_id));
        if (memory == NULL) return NULL;

        this->memories[mem_id] = memory;
    }

    return this->memories[mem_id];
}

iss_reg_t Memcheck::mem_alloc(iss_reg_t mem_id, iss_reg_t ptr, iss_reg_t size)
{
    if (this->iss.top.traces.get_trace_engine()->is_memcheck_enabled())
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Memory alloc (id: %d, ptr: 0x%x, size: 0x%x)\n",
            mem_id, ptr, size);

        MemoryMemcheck *memory = this->get_memory(mem_id);
        if (memory == NULL)
        {
            this->trace.force_warning("Trying to alloc from invalid memory (id: %d)\n", mem_id);
            return ptr;
        }

        iss_reg_t virtual_ptr = memory->alloc(ptr, size);

        this->trace.msg(vp::Trace::LEVEL_INFO, "Translated to virtual address (id: %d, virtual_ptr: 0x%x)\n",
            mem_id, virtual_ptr);

        return virtual_ptr;
    }

    return ptr;
}

iss_reg_t Memcheck::mem_free(iss_reg_t mem_id, iss_reg_t virtual_ptr, iss_reg_t size)
{
    if (this->iss.top.traces.get_trace_engine()->is_memcheck_enabled())
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Memory free (id: %d, ptr: 0x%x, size: 0x%x)\n",
            mem_id, virtual_ptr, size);

        MemoryMemcheck *memory = this->get_memory(mem_id);
        if (memory == NULL)
        {
            this->trace.force_warning("Trying to free from invalid memory (id: %d)\n", mem_id);
            return virtual_ptr;
        }

        iss_reg_t ptr = memory->free(virtual_ptr, size);

        this->trace.msg(vp::Trace::LEVEL_INFO, "Translated to physical address (id: %d, ptr: 0x%x)\n",
            mem_id, ptr);

        return ptr;
    }

    return virtual_ptr;
}
