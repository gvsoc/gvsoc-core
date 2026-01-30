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
#ifdef VP_TRACE_ACTIVE
    TraceEntry *trace;
#endif
    InsnEntry *next;
};

class PendingInsn
{
public:
    iss_insn_t *insn;
    InsnEntry *entry;
    uint64_t timestamp;
    iss_reg_t pc;
    uint64_t reg;
    uint64_t reg_2;
    uint64_t reg_3;
    bool done;
    // True if the instruction is currently being chained.
    bool chained;
    int id;
};
