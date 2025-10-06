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

static inline iss_reg_t mcnfig_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // uint16_t m_size = REG_GET(0);
    // uint16_t n_size = REG_GET(1);
    // uint16_t k_size = (REG_GET(0) >> 16);

    // iss->redmule_mnk_reg[0] = m_size;
    // iss->redmule_mnk_reg[1] = n_size;
    // iss->redmule_mnk_reg[2] = k_size;

    // iss->redmule_req->init();
    // iss->redmule_req->set_addr(0);
    // iss->redmule_req->set_data((uint8_t*)iss->redmule_mnk_reg);
    // iss->redmule_req->set_size(8);
    // iss->redmule_itf.req(iss->redmule_req);
    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
    };
    iss->exec.offload_insn(&offload_insn);
    if (!offload_insn.granted) {
        iss->exec.stall_reg = REG_OUT(0); //here any value is good... I don't see any usage of stall_reg in the exec class... so OK... let's keep the REG_OUT(0) val for now...
        iss->exec.insn_stall();
    }
    return iss_insn_next(iss, insn, pc);
}


static inline iss_reg_t marith_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // uint32_t x_addr = REG_GET(0);
    // uint32_t w_addr = REG_GET(1);
    // uint32_t y_addr = REG_GET(2);
    // uint32_t config = UIM_GET(0);

    // iss->redmule_xwy_reg[0] = x_addr;
    // iss->redmule_xwy_reg[1] = w_addr;
    // iss->redmule_xwy_reg[2] = y_addr;
    // iss->redmule_xwy_reg[3] = config;

    // iss->redmule_req->init();
    // iss->redmule_req->set_addr(4);
    // iss->redmule_req->set_data((uint8_t*)iss->redmule_xwy_reg);
    // iss->redmule_req->set_size(16);
    // iss->redmule_itf.req(iss->redmule_req);

    IssOffloadInsn<iss_reg_t> offload_insn = {
        .opcode=insn->opcode,
        .arg_a=REG_GET(0),
        .arg_b=REG_GET(1),
        .arg_c=REG_GET(2),
        .arg_d=UIM_GET(0),
    };
    iss->exec.offload_insn(&offload_insn);
    
    if (!offload_insn.granted) {
        iss->exec.stall_reg = REG_OUT(0); //here any value is good... I don't see any usage of stall_reg in the exec class... so OK... let's keep the REG_OUT(0) val for now...
        iss->exec.insn_stall();
    }

    return iss_insn_next(iss, insn, pc);
}