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

 #include <string.h>
#include <cpu/iss_v2/include/iss.hpp>

InsnCache::InsnCache(Iss &iss)
    : iss(iss)
{
    this->current_insn_page_base = -INSN_PAGE_SIZE*2;
    this->current_phys_page = NULL;
}

void InsnCache::reset(bool active)
{
    if (active)
    {
        this->flush();
    }
}

void InsnCache::stop()
{
    this->flush();
}


void InsnCache::flush()
{
    for (auto page: this->pages)
    {
        delete page.second;
    }

    this->pages.clear();
    this->current_phys_page = NULL;

    this->mode_flush();

    for (auto insn_table: this->insn_tables)
    {
        delete[] insn_table;
    }

    this->insn_tables.clear();

    this->iss.gdbserver.enable_all_breakpoints();
}

void InsnCache::mode_flush()
{
    // Covers full flushes as well: flush() calls mode_flush().
    this->generation++;
    this->current_insn_page_base = -INSN_PAGE_SIZE*2;
}

InsnPage *InsnCache::page_get(iss_reg_t paddr)
{
    iss_reg_t index = paddr >> INSN_PAGE_BITS;
    InsnPage *page = this->pages[index];
    if (page != NULL)
    {
        return page;
    }

    page = new InsnPage;

    this->pages[index] = page;

    iss_reg_t addr = index << INSN_PAGE_BITS;
    for (int i=0; i<INSN_PAGE_SIZE; i++)
    {
        insn_init(&page->insns[i], addr);
        addr += 2;
    }

    return page;
}



iss_insn_t *InsnCache::get_insn_from_cache(iss_reg_t vaddr)
{
    iss_reg_t paddr;

#ifdef CONFIG_GVSOC_ISS_MMU_ENABLED
    // Pages are indexed by physical address, so that the decoded instructions are
    // shared between all the virtual mappings of the same physical page. The insn
    // addr field then contains the physical address, while the virtual pc is passed
    // to the handlers by the exec loop. Note that the insn page must not be larger
    // than a TLB page so that one insn page has a single translation.
    if (this->iss.mmu.insn_virt_to_phys(vaddr, paddr))
    {
        return NULL;
    }
#else
    paddr = vaddr;
#endif

    this->current_insn_page = this->page_get(paddr);
    this->current_insn_page_base = (vaddr >> INSN_PAGE_BITS) << INSN_PAGE_BITS;

    return this->get_insn(vaddr);
}

iss_insn_t *InsnCache::get_insn_phys(iss_reg_t paddr)
{
    iss_reg_t page_base = (paddr >> INSN_PAGE_BITS) << INSN_PAGE_BITS;
    if (unlikely(this->current_phys_page == NULL || page_base != this->current_phys_page_base))
    {
        this->current_phys_page = this->page_get(paddr);
        this->current_phys_page_base = page_base;
    }
    return &this->current_phys_page->insns[(paddr - page_base) >> 1];
}
