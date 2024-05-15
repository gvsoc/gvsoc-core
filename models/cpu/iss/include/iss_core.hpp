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

#include "cpu/iss/include/types.hpp"
#include <string>

void update_external_pccr(Iss *iss, int id, unsigned int pcer, unsigned int pcmr);

void iss_trace_save_args(Iss *iss, iss_insn_t *insn, iss_insn_arg_t saved_args[], bool save_out);
void iss_trace_dump(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
void iss_trace_init(Iss *iss);

iss_reg_t iss_exec_insn_with_trace(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

void iss_reset(Iss *iss, int active);

iss_decoder_item_t *iss_isa_get(Iss *iss);

void iss_register_debug_info(Iss *iss, const char *binary);

iss_reg_t iss_decode_pc_handler(Iss *cpu, iss_insn_t *insn, iss_reg_t pc);

bool iss_csr_read(Iss *iss, iss_reg_t reg, iss_reg_t *value);
std::string iss_csr_name(Iss *iss, iss_reg_t reg);
bool iss_csr_write(Iss *iss, iss_reg_t reg, iss_reg_t value);

int iss_trace_pc_info(iss_addr_t addr, const char **func, const char **inline_func, const char **file, int *line);

extern iss_isa_set_t __iss_isa_set;

static inline iss_isa_set_t *iss_get_isa_set()
{
    return &__iss_isa_set;
}

static inline iss_reg_t iss_insn_next(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return pc + insn->size;
}

#include "cpu/iss/include/utils.hpp"
#include "cpu/iss/include/regfile_implem.hpp"
#include "cpu/iss/include/perf.hpp"
#include "cpu/iss/include/lsu.hpp"
#include <cpu/iss/include/decode.hpp>
#include "cpu/iss/include/insn_cache.hpp"
#include "cpu/iss/include/resource.hpp"

#endif
