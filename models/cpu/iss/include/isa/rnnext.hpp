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

#ifndef __CPU_ISS_RNNEXT_HPP
#define __CPU_ISS_RNNEXT_HPP

static inline void pl_sdotsp_h_0_load_resume(Lsu *lsu, IO_REQ *req)
{
    iss_insn_t *insn = lsu->iss.rnnext.sdot_insn;
    lsu->iss.csr.trace.msg("Loaded new sdot_prefetch_0 value (value: 0x%x)\n", lsu->iss.rnnext.sdot_prefetch_0);
}

static inline iss_reg_t pl_sdotsp_h_0_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t addr = REG_GET(0);
    IN_REG_SET(0, addr + 4);

    REG_SET(0, LIB_CALL3(lib_VEC_SDOTSP_16, REG_GET(2), iss->rnnext.sdot_prefetch_0, REG_GET(1)));

    int64_t latency;
    int req_id;
    if (!iss->lsu.data_req(addr, (uint8_t *)&iss->rnnext.sdot_prefetch_0, NULL, 4, false, latency, req_id))
    {
        iss->csr.trace.msg("Loaded new sdot_prefetch_0 value (value: 0x%x)\n", iss->rnnext.sdot_prefetch_0);
    }
    else
    {
        iss->lsu.stall_callback = pl_sdotsp_h_0_load_resume;
        iss->rnnext.sdot_insn = insn;
        iss->exec.insn_stall();
    }

    return iss_insn_next(iss, insn, pc);
}

static inline void pl_sdotsp_h_1_load_resume(Lsu *lsu, IO_REQ *req)
{
    iss_insn_t *insn = lsu->iss.rnnext.sdot_insn;
    lsu->iss.csr.trace.msg("Loaded new sdot_prefetch_1 value (value: 0x%x)\n", lsu->iss.rnnext.sdot_prefetch_1);
}

static inline iss_reg_t pl_sdotsp_h_1_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t addr = REG_GET(0);
    IN_REG_SET(0, addr + 4);

    REG_SET(0, LIB_CALL3(lib_VEC_SDOTSP_16, REG_GET(2), iss->rnnext.sdot_prefetch_1, REG_GET(1)));

    int64_t latency;
    int req_id;
    if (!iss->lsu.data_req(addr, (uint8_t *)&iss->rnnext.sdot_prefetch_1, NULL, 4, false, latency, req_id))
    {
        iss->csr.trace.msg("Loaded new sdot_prefetch_1 value (value: 0x%x)\n", iss->rnnext.sdot_prefetch_1);
    }
    else
    {
        iss->lsu.stall_callback = pl_sdotsp_h_1_load_resume;
        iss->rnnext.sdot_insn = insn;
        iss->exec.insn_stall();
    }

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t pl_tanh_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL1(lib_TANH, REG_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t pl_sig_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL1(lib_SIG, REG_GET(0)));
    return iss_insn_next(iss, insn, pc);
}

#endif
