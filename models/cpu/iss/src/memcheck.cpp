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

void Memcheck::mem_open(iss_reg_t mem_id, iss_reg_t base, iss_reg_t size,
    iss_reg_t virtual_base)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Memory open (id: %d, base: 0x%x, size: 0x%x, "
        "virtual_base: 0x%x, expansion factor: %d)\n",
        mem_id, base, size, virtual_base, this->expansion_factor);

    if (this->memories.count(mem_id) > 0)
    {
        this->trace.force_warning("Trying to open already opened memory (id: %d)\n", mem_id);
        return;
    }

    if (mem_id >= this->nb_mem_itf)
    {
        this->trace.force_warning("Trying to open invalid memory (id: %d, nb_memories: %d)\n",
            mem_id, this->nb_mem_itf);
        return;
    }

    this->memories[mem_id] = new MemcheckMemory(this, mem_id, base, size, virtual_base);
}

void Memcheck::mem_close(iss_reg_t mem_id)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Memory close (id: %d)\n", mem_id);

    if (this->memories.count(mem_id) == 0)
    {
        this->trace.force_warning("Trying to close invalid memory (id: %d)\n", mem_id);
        return;
    }

    delete this->memories[mem_id];
    this->memories.erase(mem_id);
}

iss_reg_t Memcheck::mem_alloc(iss_reg_t mem_id, iss_reg_t ptr, iss_reg_t size)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Memory alloc (id: %d, ptr: 0x%x, size: 0x%x)\n",
        mem_id, ptr, size);

    if (this->memories.count(mem_id) == 0)
    {
        this->trace.force_warning("Trying to alloc from invalid memory (id: %d)\n", mem_id);
        return ptr;
    }

    iss_reg_t virtual_ptr = this->memories[mem_id]->alloc(ptr, size);

    this->trace.msg(vp::Trace::LEVEL_INFO, "Translated to virtual address (id: %d, virtual_ptr: 0x%x)\n",
        mem_id, virtual_ptr);

    return virtual_ptr;
}

iss_reg_t Memcheck::mem_free(iss_reg_t mem_id, iss_reg_t ptr, iss_reg_t size)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Memory free (id: %d, ptr: 0x%x, size: 0x%x)\n",
        mem_id, ptr, size);

    if (this->memories.count(mem_id) == 0)
    {
        this->trace.force_warning("Trying to free from invalid memory (id: %d)\n", mem_id);
        return ptr;
    }

    return this->memories[mem_id]->free(ptr, size);
}


MemcheckMemory::MemcheckMemory(Memcheck *top, iss_reg_t mem_id, iss_reg_t base, iss_reg_t size,
    iss_reg_t virtual_base)
{
    this->top = top;
    this->mem_id = mem_id;
    this->base = base;
    this->size = size;
    this->virtual_base = virtual_base;
}


iss_reg_t MemcheckMemory::alloc(iss_reg_t ptr, iss_reg_t size)
{
    iss_reg_t virtual_offset = (ptr - this->base) * this->top->expansion_factor + size * (this->top->expansion_factor  / 2) ;
    iss_reg_t virtual_ptr = virtual_offset + this->virtual_base;

    MemoryMemcheckBuffer info = { .enable=true, .base=virtual_offset,
        .size=size};
    this->top->mem_itfs[this->mem_id].sync(&info);

    return virtual_ptr;
}

iss_reg_t MemcheckMemory::free(iss_reg_t virtual_ptr, iss_reg_t size)
{
    iss_reg_t virtual_offset = virtual_ptr - this->virtual_base;
    iss_reg_t offset = (virtual_offset - size * (this->top->expansion_factor  / 2)) / this->top->expansion_factor + this->base;

    MemoryMemcheckBuffer info = { .enable=false, .base=virtual_offset,
        .size=size};
    this->top->mem_itfs[this->mem_id].sync(&info);

    return offset;
}