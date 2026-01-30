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

#include <vp/signal.hpp>
#include <cpu/iss_v2/include/types.hpp>
#include <cpu/iss_v2/include/task.hpp>

struct LsuReqEntry
{
    // First must be first field of this structure to ease casting
    vp::IoReq req;
    Task task;
    uint64_t data;
    LsuReqEntry *next;
    InsnEntry *insn_entry;
    int64_t timestamp;
};

class Lsu
{
public:
    Lsu(Iss &iss);

    void start();
    void stop() {}
    void reset(bool active);

    // Not implemented yet, just defined to keep compatibility with ISA files
    inline void stack_access_check(int reg, iss_addr_t addr) {}

    bool data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg);
    bool data_req(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg);
    bool data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg);

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

    bool lsu_is_empty();

    // inline void stack_access_check(int reg, iss_addr_t addr);

    // Allocate a request. This can returns NULL if the core can not send more requests (last one
    // was denied or all requests are already allocated). In this case the core should stall.
    inline LsuReqEntry *get_req_entry();
    // Free a request so that it can be used for another access.
    // The cyclestamp indicates when the request becomes free. This allows easily freeing requests
    // during synchronous responses.
    inline void free_req_entry(LsuReqEntry *entry, int64_t cyclestamp);

    Iss &iss;

    vp::Trace trace;


    // lsu
    vp::IoMaster data;
    vp::WireMaster<void *> meminfo;
    LsuReqEntry *req_entry_first;
    LsuReqEntry req_entry[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];
    vp::IoReq debug_req;

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
    bool load_req(iss_insn_t *insn, iss_addr_t addr, int size, int reg, bool is_signed);

    static void store_resume(void *_this, vp::IoReq *req);
    static void store_resume(Lsu *lsu, vp::IoReq *req);
    static void load_resume(Lsu *lsu, vp::IoReq *req);
    static void elw_resume(Lsu *lsu, vp::IoReq *req);
    static void load_signed_resume(Lsu *lsu, vp::IoReq *req);
    static void load_float_resume(Lsu *lsu, vp::IoReq *req);
    static void handle_io_req_end(Iss *iss, Task *task);

    int64_t pending_latency;
    // True if the last request has been denied. The core must not send another request until
    // the last request has been granted
    vp::Signal<bool> io_req_denied;

    vp::Signal<bool> stalled;
    vp::Signal<iss_reg_t> log_addr;
    vp::Signal<iss_reg_t> log_size;
    vp::Signal<bool> log_is_write;
};
