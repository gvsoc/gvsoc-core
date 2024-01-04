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

#include <cpu/iss/include/types.hpp>

#ifndef CONFIG_GVSOC_ISS_SNITCH
#define ADDR_MASK (~(ISS_REG_WIDTH / 8 - 1))
#endif

// Snitch ISS_REG_WIDTH=32, and memory transfer BW is 64 bits.
#ifdef CONFIG_GVSOC_ISS_SNITCH
#define ADDR_MASK (~(ISS_REG_WIDTH / 4 - 1))
#endif

class Lsu
{
public:
    Lsu(Iss &iss);

    void build();
    void start();
    void reset(bool active);

    int data_req(iss_addr_t addr, uint8_t *data, int size, bool is_write, int64_t &latency);
    int data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write, int64_t &latency);
    int data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write, int64_t &latency);

    static void exec_misaligned(vp::Block *__this, vp::ClockEvent *event);
    static void data_grant(vp::Block *__this, vp::IoReq *req);
    static void data_response(vp::Block *__this, vp::IoReq *req);

    template<typename T>
    inline bool store(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    template<typename T>
    inline bool store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    void atomic(iss_insn_t *insn, iss_addr_t addr, int size, int reg_in, int reg_out, vp::IoReqOpcode opcode);

    inline void elw(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void elw_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    inline void stack_access_check(int reg, iss_addr_t addr);

    Iss &iss;

    vp::Trace trace;


    // lsu
    vp::IoMaster data;
    vp::WireMaster<void *> meminfo;
    vp::IoReq io_req;
    int misaligned_size;
    uint8_t *misaligned_data;
    iss_addr_t misaligned_addr;
    bool misaligned_is_write;
    vp::reg_1 elw_stalled;

    // A callback can be set here, so that it is called when the response of a pending
    // request is received.
    void (*stall_callback)(Lsu *lsu);
    int stall_reg;
    int stall_size;
    uint8_t *mem_array;
    iss_addr_t memory_start;
    iss_addr_t memory_end;

private:
    static void store_resume(void *_this);
    static void store_resume(Lsu *lsu);
    static void load_resume(Lsu *lsu);
    static void elw_resume(Lsu *lsu);
    static void load_signed_resume(Lsu *lsu);
    static void load_float_resume(Lsu *lsu);

    int64_t pending_latency;
};
