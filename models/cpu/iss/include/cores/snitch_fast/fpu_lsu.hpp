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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#pragma once

#include "cpu/iss/include/types.hpp"

class FpuLsu : public vp::Block
{
public:

    FpuLsu(IssWrapper &top, Iss &iss);

    int data_req(iss_addr_t addr, uint8_t *data, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency, int &req_id);
    int data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency, int &req_id);
    int data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency);

    template<typename T>
    bool load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    bool store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    bool load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    bool store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    Iss &iss;

    vp::Trace trace;

    vp::IoReq io_req;
#ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
    void (*stall_callback[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING])(FpuLsu *lsu, vp::IoReq *req);
    int stall_reg[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
    int stall_size[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
#else
    void (*stall_callback)(FpuLsu *lsu, vp::IoReq *req);
    int stall_reg;
    int stall_size;
#endif
    int misaligned_size;
    uint8_t *misaligned_data;
    uint8_t *misaligned_memcheck_data;
    iss_addr_t misaligned_addr;
    bool misaligned_is_write;

private:
    static void load_float_resume(FpuLsu *lsu, vp::IoReq *req);
    static void store_resume(FpuLsu *lsu, vp::IoReq *req);

    int64_t pending_latency;
};
