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

#ifndef __CPU_ISS_ISS_INSN_CACHE_HPP
#define __CPU_ISS_ISS_INSN_CACHE_HPP





int insn_cache_init(Iss *iss);
void iss_cache_flush(Iss *iss);
bool insn_cache_is_decoded(Iss *iss, iss_insn_t *insn);

iss_insn_t *insn_cache_get_insn_from_cache(Iss *iss, iss_reg_t vaddr, iss_reg_t &index);

bool iss_decode_insn(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

inline iss_insn_t *insn_cache_get_insn(Iss *iss, iss_reg_t vaddr, iss_reg_t &index)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    iss_insn_page_t *page = cache->current_insn_page;

    if (likely(page != NULL))
    {
        index = (vaddr - cache->current_insn_page_base) >> 1;
        if (likely(index < INSN_PAGE_SIZE))
        {
            return &page->insns[index];
        }
    }

    return insn_cache_get_insn_from_cache(iss, vaddr, index);
}

void iss_cache_vflush(Iss *iss);

void insn_init(iss_insn_t *insn, iss_addr_t addr);

#endif
