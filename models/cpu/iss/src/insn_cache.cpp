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

#include "iss.hpp"
#include <string.h>

static void insn_block_init(Iss *iss, iss_insn_block_t *b, iss_addr_t pc);
void insn_init(iss_insn_t *insn, iss_addr_t addr);

static void flush_cache(Iss *iss, iss_insn_cache_t *cache)
{
    iss->prefetcher.flush();

    if (cache->first_page)
    {
        cache->last_page->next = cache->first_free_page;
        cache->first_free_page = cache->first_page;
        cache->first_page = NULL;
    }

    cache->pages.clear();

    iss_cache_vflush(iss);
}

int insn_cache_init(Iss *iss)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    cache->current_insn_page = NULL;
    cache->first_page = NULL;
    cache->first_free_page = NULL;
    return 0;
}

bool insn_cache_is_decoded(Iss *iss, iss_insn_t *insn)
{
    return insn->handler != iss_decode_pc_handler;
}

void insn_init(iss_insn_t *insn, iss_addr_t addr)
{
    insn->handler = iss_decode_pc_handler;
    insn->fast_handler = iss_decode_pc_handler;
    insn->addr = addr;
    insn->hwloop_handler = NULL;
    insn->breakpoint_saved_handler = NULL;
    insn->fetched = false;
    insn->input_latency_reg = -1;
}



void iss_cache_flush(Iss *iss)
{
    flush_cache(iss, &iss->decode.insn_cache);

    iss->gdbserver.enable_all_breakpoints();

    iss->irq.cache_flush();
}

void iss_cache_vflush(Iss *iss)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    cache->current_insn_page = NULL;
}

iss_insn_t *insn_cache_get(Iss *iss, iss_addr_t pc)
{
    return insn_cache_get_insn(iss, pc);
}

void Decode::flush_cache_sync(void *__this, bool active)
{
    Decode *_this = (Decode *)__this;
    iss_cache_flush(&_this->iss);
}



iss_insn_page_t *insn_cache_page_get(Iss *iss, iss_reg_t paddr)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    iss_reg_t index = paddr >> INSN_PAGE_BITS;
    iss_insn_page_t *page = cache->pages[index];
    if (page != NULL)
    {
        return page;
    }

    if (cache->first_free_page)
    {
        page = cache->first_free_page;
        cache->first_free_page = page->next;
    }
    else
    {
        page = new iss_insn_page_t;
    }

    cache->pages[index] = page;

    for (int i=0; i<INSN_PAGE_SIZE; i++)
    {
        insn_init(&page->insns[i], (index << INSN_PAGE_BITS) + (i << 1));
    }

    page->next = cache->first_page;
    if (cache->first_page == NULL)
    {
        cache->last_page = page;
    }
    cache->first_page = page;

    return page;
}



iss_insn_t *insn_cache_get_insn_from_cache(Iss *iss, iss_reg_t vaddr)
{
    iss_reg_t paddr;
    iss_insn_cache_t *cache = &iss->decode.insn_cache;

#ifdef CONFIG_GVSOC_ISS_MMU
    if (iss->mmu.insn_virt_to_phys(vaddr, paddr))
    {
        return NULL;
    }
#else
    paddr = vaddr;
#endif

    cache->current_insn_page = insn_cache_page_get(iss, paddr);
    cache->current_insn_page_base = (vaddr >> INSN_PAGE_BITS) << INSN_PAGE_BITS;

    return insn_cache_get_insn(iss, vaddr);
}