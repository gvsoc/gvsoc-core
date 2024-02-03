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

#pragma once

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"


static inline iss_reg_t dmsrc_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmdst_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmstr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmrep_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
    };
    iss->exec.offload_insn(&offload_insn);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmcpy_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    REG_SET(0, offload_insn.result);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmcpyi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=UIM_GET(0),
    };
    iss->exec.offload_insn(&offload_insn);
    REG_SET(0, offload_insn.result);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmstat_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    REG_SET(0, offload_insn.result);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t dmstati_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_b=UIM_GET(0),
    };
    iss->exec.offload_insn(&offload_insn);
    REG_SET(0, offload_insn.result);
    return iss_insn_next(iss, insn, pc);
}
