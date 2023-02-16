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

#include "iss_core.hpp"
#include "isa_lib/int.h"
#include "isa_lib/macros.h"

static inline iss_insn_t *lr_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, 0, REG_OUT(0), vp::io_req_opcode_e::LR);
    return insn->next;
}

static inline iss_insn_t *sc_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::SC);
    return insn->next;
}

static inline iss_insn_t *amoswap_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::SWAP);
    return insn->next;
}

static inline iss_insn_t *amoadd_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::ADD);
    return insn->next;
}

static inline iss_insn_t *amoxor_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::XOR);
    return insn->next;
}

static inline iss_insn_t *amoand_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::AND);
    return insn->next;
}

static inline iss_insn_t *amoor_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::OR);
    return insn->next;
}

static inline iss_insn_t *amomin_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::MIN);
    return insn->next;
}

static inline iss_insn_t *amomax_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::MAX);
    return insn->next;
}

static inline iss_insn_t *amominu_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::MINU);
    return insn->next;
}

static inline iss_insn_t *amomaxu_w_exec(Iss *iss, iss_insn_t *insn)
{
    iss->lsu.atomic(insn, REG_GET(0), 4, REG_IN(1), REG_OUT(0), vp::io_req_opcode_e::MAXU);
    return insn->next;
}
