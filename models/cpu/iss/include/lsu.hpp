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
#include <vp/signal.hpp>

#ifndef CONFIG_GVSOC_ISS_SNITCH
#define ADDR_MASK (~(ISS_REG_WIDTH / 8 - 1))
#endif

// Snitch ISS_REG_WIDTH=32, and memory transfer BW is 64 bits.
#if defined(CONFIG_GVSOC_ISS_SNITCH) || defined(CONFIG_GVSOC_ISS_SNITCH_FAST)
#define ADDR_MASK (~(ISS_REG_WIDTH / 4 - 1))
#endif

class IssWrapper;

class Lsu
{
public:
    Lsu(IssWrapper &top, Iss &iss);

    void build();
    void start();
    void reset(bool active);

#ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
    bool lsu_is_empty();
#endif
    int data_req(iss_addr_t addr, uint8_t *data, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency, int &req_id);
    int data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency, int &req_id);
    int data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency);

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

    bool atomic(iss_insn_t *insn, iss_addr_t addr, int size, int reg_in, int reg_out, vp::IoReqOpcode opcode);

    inline void elw(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void elw_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    inline void stack_access_check(int reg, iss_addr_t addr);

    // Allocate a request. This can returns NULL if the core can not send more requests (last one
    // was denied or all requests are already allocated). In this case the core should stall.
    inline vp::IoReq *get_req();
    // Free a request so that it can be used for another access.
    // The cyclestamp indicates when the request becomes free. This allows easily freeing requests
    // during synchronous responses.
    inline void free_req(vp::IoReq *req, int64_t cyclestamp);

    Iss &iss;

    vp::Trace trace;


    // lsu
    vp::IoMaster data;
    vp::WireMaster<void *> meminfo;
#ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
    // First availabel request. They are sorted out by increasing cyclestamp. The cyclestamp
    // indicates when the request becomes available. First request is the next one to be free,
    // its cyclestamp must be compared against engine cycle to know if it is free in the current
    // cycle.
    vp::IoReq *io_req_first;
    // List of requests which can be sent at the same time.
    vp::IoReq io_req[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
    iss_reg_t stall_insn[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
    iss_reg_t req_data[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
#else
    vp::IoReq io_req;
#endif
    vp::IoReq debug_req;
    int misaligned_size;
    uint8_t *misaligned_data;
    uint8_t *misaligned_memcheck_data;
    iss_addr_t misaligned_addr;
    bool misaligned_is_write;
    vp::reg_1 elw_stalled;

    // A callback can be set here, so that it is called when the response of a pending
    // request is received.
#ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
    // When using multiple outstanding request, each on-going request can have its callback
    // used when the response arrives.
    void (*stall_callback[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING])(Lsu *lsu, vp::IoReq *req);
    int stall_reg[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
    int stall_size[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
#else
    void (*stall_callback)(Lsu *lsu, vp::IoReq *req);
    int stall_reg;
    int stall_size;
#endif
    uint8_t *mem_array;
    iss_addr_t memory_start;
    iss_addr_t memory_end;

private:
    static void store_resume(void *_this, vp::IoReq *req);
    static void store_resume(Lsu *lsu, vp::IoReq *req);
    static void load_resume(Lsu *lsu, vp::IoReq *req);
    static void elw_resume(Lsu *lsu, vp::IoReq *req);
    static void load_signed_resume(Lsu *lsu, vp::IoReq *req);
    static void load_float_resume(Lsu *lsu, vp::IoReq *req);

    int64_t pending_latency;
    // True if the last request has been denied. The core must not send another request until
    // the last request has been granted
    vp::Signal<bool> io_req_denied;

    vp::Signal<bool> stalled;
    vp::Signal<iss_reg_t> log_addr;
    vp::Signal<iss_reg_t> log_size;
    vp::Signal<bool> log_is_write;
};
