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

#include <cpu/iss_v2/include/decode.hpp>
#include <cpu/iss_v2/include/types.hpp>

// The size of a page corresponds to the tlb page size with instructions of at least 2 bytes
#define INSN_PAGE_BITS 9
#define INSN_PAGE_SIZE (1 << (INSN_PAGE_BITS - 1))
#define INSN_PAGE_MASK (INSN_PAGE_SIZE - 1)

struct InsnPage
{
    iss_insn_t insns[INSN_PAGE_SIZE];
    InsnPage *next;
};

class InsnCache
{
public:
    InsnCache(Iss &iss);
    void reset(bool active);
    void stop();
    void flush();
    iss_insn_t *get_insn_from_cache(iss_reg_t vaddr);
    inline iss_insn_t *get_insn(iss_reg_t vaddr);
    void mode_flush();
    inline void insn_init(iss_insn_t *insn, iss_addr_t addr);
    InsnPage *page_get(iss_reg_t paddr);


private:
    InsnPage *current_insn_page;
    iss_reg_t current_insn_page_base;
    std::unordered_map<iss_reg_t, InsnPage *>pages;
    std::vector<iss_insn_t *> insn_tables;

    Iss &iss;
};



inline iss_insn_t *InsnCache::get_insn(iss_reg_t vaddr)
{
    iss_reg_t index;
    index = (vaddr - this->current_insn_page_base) >> 1;
    if (likely(index < INSN_PAGE_SIZE))
    {
        return &this->current_insn_page->insns[index];
    }

    return this->get_insn_from_cache(vaddr);
}

inline void InsnCache::insn_init(iss_insn_t *insn, iss_addr_t addr)
{
    insn->handler = NULL;
    insn->addr = addr;
}
