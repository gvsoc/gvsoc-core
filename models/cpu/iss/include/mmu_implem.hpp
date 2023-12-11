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
#include <cpu/iss/include/core.hpp>
#include "cpu/iss/include/mmu.hpp"

#define ACCESS_INSN  1
#define ACCESS_LOAD  2
#define ACCESS_STORE 4

inline bool Mmu::insn_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr)
{
#ifdef CONFIG_GVSOC_ISS_MMU
    iss_addr_t tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;

    if (likely(this->tlb_insn_tag[index] == tag))
    {
        phys_addr = virt_addr + this->tlb_insn_phys_addr[index];
        return false;
    }

    this->access_type = ACCESS_INSN;
    bool use_mem_array;
    return virt_to_phys_miss(virt_addr, phys_addr, use_mem_array);
#else
    phys_addr = virt_addr;
    return false;
#endif
}

inline bool Mmu::load_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr, bool &use_mem_array)
{
#ifdef CONFIG_GVSOC_ISS_MMU
    iss_addr_t tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;

    if (likely(this->tlb_load_tag[index] == tag))
    {
        phys_addr = virt_addr + this->tlb_ls_phys_addr[index];
        use_mem_array = this->tlb_load_use_mem_array[index];
        return false;
    }

    this->access_type = ACCESS_LOAD;
    return virt_to_phys_miss(virt_addr, phys_addr, use_mem_array);
#else

    phys_addr = virt_addr;

#ifdef CONFIG_GVSOC_ISS_MEMORY
    use_mem_array = phys_addr >= this->iss.lsu.memory_start && phys_addr < this->iss.lsu.memory_end;
#endif

    return false;
#endif
}

inline bool Mmu::store_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr, bool &use_mem_array)
{
#ifdef CONFIG_GVSOC_ISS_MMU
    iss_addr_t tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;

    if (likely(this->tlb_store_tag[index] == tag))
    {
        phys_addr = virt_addr + this->tlb_ls_phys_addr[index];
        use_mem_array = this->tlb_load_use_mem_array[index];
        return false;
    }

    this->access_type = ACCESS_STORE;
    return virt_to_phys_miss(virt_addr, phys_addr, use_mem_array);
#else
    phys_addr = virt_addr;

#ifdef CONFIG_GVSOC_ISS_MEMORY
    use_mem_array = phys_addr >= this->iss.lsu.memory_start && phys_addr < this->iss.lsu.memory_end;
#endif

    return false;
#endif
}
