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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#ifndef __CPU_ISS_RV32FREP_HPP
#define __CPU_ISS_RV32FREP_HPP

#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"

#if defined(CONFIG_GVSOC_ISS_SNITCH_FAST)

static inline iss_reg_t frep_o_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss->sequencer.frep_handle(insn, pc, false);
}

static inline iss_reg_t frep_i_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss->sequencer.frep_handle(insn, pc, true);
}

#else

static inline iss_reg_t frep_o_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    // Add access check REG_GET(0) here, one of input operand is from register.
    // Latency accumulates after the instruction if there's data dependency.
    insn->max_rpt = REG_GET(0);
    insn->is_outer = true;

    // Send an IO request to check whether the subsystem is ready for offloading.
    bool acc_req_ready = iss->check_state(insn);
    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Integer side receives acceleration request handshaking signal: %d\n", acc_req_ready);

    // If not ready, stay at current PC and fetch the same instruction next cycle.
    if (!acc_req_ready)
    {
        iss->exec.trace.msg("Stall at current instruction\n");
        insn->handler = frep_o_exec;
        return pc;
    }

    // If ready, send offload request.
    if (acc_req_ready)
    {
        int stall = iss->handle_req(insn, pc, false);
    }
#endif
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t frep_i_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH
    insn->max_rpt = REG_GET(0);
    insn->is_outer = false;

    // Send an IO request to check whether the subsystem is ready for offloading.
    bool acc_req_ready = iss->check_state(insn);
    iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "Integer side receives acceleration request handshaking signal: %d\n", acc_req_ready);

    // If not ready, stay at current PC and fetch the same instruction next cycle.
    if (!acc_req_ready)
    {
        iss->exec.trace.msg("Stall at current instruction\n");
        insn->handler = frep_o_exec;
        return pc;
    }

    // If ready, send offload request.
    if (acc_req_ready)
    {
        int stall = iss->handle_req(insn, pc, false);
    }
#endif
    return iss_insn_next(iss, insn, pc);
}

#endif

#endif
