/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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

static inline iss_reg_t lr_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, 0, REG_OUT(0), vp::IoReqOpcode::LR);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t sc_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::SC);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amoswap_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::SWAP);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amoadd_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::ADD);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amoxor_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::XOR);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amoand_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::AND);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amoor_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::OR);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amomin_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::MIN);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amomax_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::MAX);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amominu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::MINU);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t amomaxu_d_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->lsu.atomic(insn, REG_GET(0), 8, REG_IN(1), REG_OUT(0), vp::IoReqOpcode::MAXU);
    return iss_insn_next(iss, insn, pc);
}
