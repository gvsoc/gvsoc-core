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

#ifndef __CPU_ISS_ISS_CORE_HPP
#define __CPU_ISS_ISS_CORE_HPP

#include "types.hpp"
#include <string>

void iss_trace_dump(Iss *iss, iss_insn_t *insn);
void iss_trace_init(Iss *iss);

iss_insn_t *iss_exec_insn_with_trace(Iss *iss, iss_insn_t *insn);

void iss_reset(Iss *iss, int active);
void iss_start(Iss *iss);

iss_decoder_item_t *iss_isa_get(Iss *iss, const char *name);

void iss_register_debug_info(Iss *iss, const char *binary);

iss_insn_t *iss_decode_pc_handler(Iss *cpu, iss_insn_t *pc);
void iss_decode_activate_isa(Iss *cpu, char *isa);





void iss_csr_init(Iss *iss, int active);
bool iss_csr_read(Iss *iss, iss_reg_t reg, iss_reg_t *value);
const char *iss_csr_name(Iss *iss, iss_reg_t reg);
bool iss_csr_write(Iss *iss, iss_reg_t reg, iss_reg_t value);

int iss_trace_pc_info(iss_addr_t addr, const char **func, const char **inline_func, const char **file, int *line);

extern iss_isa_set_t __iss_isa_set;

static inline iss_isa_set_t *iss_get_isa_set()
{
    return &__iss_isa_set;
}

#include "utils.hpp"
#include "iss_api.hpp"
#include "regs.hpp"
#include "perf.hpp"
#include "lsu.hpp"
#include "insn_cache.hpp"
#include "exceptions.hpp"
#include "resource.hpp"


#endif
