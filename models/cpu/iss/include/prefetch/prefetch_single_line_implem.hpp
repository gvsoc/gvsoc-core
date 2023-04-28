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

#include "types.hpp"
#include <stdio.h>



inline bool Prefetcher::fetch(iss_reg_t addr)
{
    // Since an instruction can be 2 or 4 bytes, we need to be careful that only part of it can
    // fit the buffer, so we have to check both the low part and the high part.

    // Compute where the instructions address falls into the prefetch buffer
    iss_reg_t phys_addr;

#ifdef CONFIG_GVSOC_ISS_MMU
    if (this->iss.mmu.insn_virt_to_phys(addr, phys_addr))
    {
        return false;
    }
#endif

    iss_insn_t *insn = insn_cache_get(&this->iss, addr);
    if (insn == NULL)
    {
        return false;
    }

    unsigned int index = phys_addr - this->buffer_start_addr;

    // If it is entirely within the buffer, get the opcode and decode it.
    if (likely(index <= ISS_PREFETCHER_SIZE - sizeof(iss_opcode_t)))
    {
        insn->opcode = *(iss_opcode_t *)&this->data[index];
        return true;
    }

    // Otherwise, fake a refill
    this->current_pc = addr;
    return this->fetch_refill(insn, phys_addr, index);
}


inline void Prefetcher::flush()
{
    // Since the address is an unsigned int, the next index will be negative and will force the prefetcher
    // to refill
    this->buffer_start_addr = -1;
}

inline void Prefetcher::handle_stall(void (*callback)(Prefetcher *), iss_insn_t *current_insn)
{
    Iss *iss = &this->iss;

    // Function to be called when teh refill is done
    this->fetch_stall_callback = callback;
    // Remember the current instruction since the core may switch to a new one while the prefetch buffer
    // is being refilled
    this->prefetch_insn = current_insn;
    // Stall the core
    iss->exec.stalled_inc();
}
