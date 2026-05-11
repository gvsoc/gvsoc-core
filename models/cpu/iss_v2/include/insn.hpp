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

#include <cpu/iss_v2/include/types.hpp>

class TraceEntry;

class InsnEntry
{
public:
    iss_addr_t addr;
    iss_reg_t opcode;
    TraceEntry *trace;
    InsnEntry *next;
#ifdef CONFIG_GVSOC_ISS_EXEC_INORDER_COMMIT
    // In-order commit queue link state: `false` while a held async
    // insn is still waiting for its LSU response (or WFI for its
    // wakeup); flipped to `true` by `insn_terminate`, or set to
    // `true` at enqueue time for sync followers whose result was
    // already written to the regfile at dispatch.
    bool ready;
#endif
};
