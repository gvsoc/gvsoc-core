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
#include <types.hpp>
#include <core.hpp>
#include "mmu.hpp"

inline iss_addr_t Mmu::insn_virt_to_phys(iss_addr_t virt_addr)
{
#ifdef CONFIG_GVSOC_ISS_MMU
    iss_addr_t tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;

    if (likely(this->tlb_insn_tag[index] == tag))
    {
        // printf("TLB converted %lx to %lx\n", virt_addr, virt_addr + this->tlb_phys_addr[index]);
        return virt_addr + this->tlb_phys_addr[index];
    }

        // printf("MISS converted %lx to %lx\n", virt_to_phys_miss(virt_addr));
    return virt_to_phys_miss(virt_addr);
#else
    return virt_addr;
#endif
}

inline iss_addr_t Mmu::load_virt_to_phys(iss_addr_t virt_addr)
{
    return 0;
}

inline iss_addr_t Mmu::store_virt_to_phys(iss_addr_t virt_addr)
{
    return 0;
}
