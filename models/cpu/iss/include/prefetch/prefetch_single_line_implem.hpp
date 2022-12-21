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


inline void Prefetcher::fetch(iss_insn_t *insn)
{
    void (*fetch_callback)(void *, iss_insn_t *) = insn->fetch_callback;

    if (unlikely(fetch_callback != NULL))
    {
        fetch_callback(this, insn);
        insn->fetch_callback = insn->fetch_force_callback;
    }
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
