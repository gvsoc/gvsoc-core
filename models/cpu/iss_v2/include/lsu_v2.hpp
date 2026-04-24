// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * io_v2 variant of the ISS LSU. Structurally mirrors the v1 class so the
 * surrounding ISS (decode/execute, exceptions, MMU, syscalls, ...) can
 * call the exact same method set regardless of which LSU is in use. The
 * differences vs v1 are in the IO-side plumbing only:
 *
 *   - ``data`` is now a ``vp::IoMaster`` from ``vp/itf/io_v2.hpp`` (chosen
 *     by the ``CONFIG_GVSOC_ISS_LSU_V2`` guard in ``types.hpp``).
 *   - Request statuses are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` /
 *     ``IO_REQ_DENIED``; error reporting travels on the response status
 *     sideband (``IO_RESP_OK`` / ``IO_RESP_INVALID``).
 *   - The back-pressure handshake is AXI-like: on DENIED we flag
 *     ``io_req_denied`` and wait for ``retry()`` from the downstream
 *     before allowing the core to re-submit.
 *
 * Publicly-visible fields (``data``, ``debug_req``) keep the same names
 * as v1 so external users (``syscalls.cpp``, ``ara_vlsu.cpp``, ...) can
 * talk to either variant through the same ``iss.lsu.data`` / ``iss.lsu.debug_req``
 * handles. Those callers still need to guard any v1-specific API use
 * (``req->init()``, ``set_debug()``, ``IO_REQ_OK``, ``get_full_latency()``)
 * with ``#ifdef CONFIG_GVSOC_ISS_LSU_V2``.
 */

#pragma once

#include <vp/signal.hpp>
#include <cpu/iss_v2/include/types.hpp>
#include <cpu/iss_v2/include/task.hpp>

struct LsuReqEntry
{
    // First must be first field of this structure to ease casting between
    // a raw vp::IoReq * (coming back from a muxed resp/retry) and its
    // owning entry.
    vp::IoReq req;
    Task task;
    uint64_t data;
    uint64_t data2;
    bool is_signed;
    int reg;
    int reg2;
    uint64_t pc;
    LsuReqEntry *next;
    InsnEntry *insn_entry;
    int64_t timestamp;
};

class LsuV2
{
public:
    LsuV2(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    // Not implemented yet, just defined to keep compatibility with ISA files
    inline void stack_access_check(int reg, iss_addr_t addr) {}

    bool data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2=0);
    bool data_req(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2);
    bool data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2);

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

    bool fence();

    // TODO used by syscalls, find a better to handle such accesses
    vp::IoReq   debug_req;
    vp::IoMaster data{&LsuV2::data_retry, &LsuV2::data_response};

private:
    // Allocate a request. Returns NULL if no entry is free and the core
    // must stall.
    inline LsuReqEntry *get_req_entry();
    // Free a request so that it can be used for another access.
    inline void free_req_entry(LsuReqEntry *entry);
    bool load_req(iss_insn_t *insn, iss_addr_t addr, int size, int reg, bool is_signed);
    static void task_handle(Iss *iss, Task *task);
    // io_v2 downstream callbacks. ``retry`` fires without a request
    // argument — it is a pure "ready-again" signal per the v2 protocol.
    static void data_retry(vp::Block *__this);
    static void data_response(vp::Block *__this, vp::IoReq *req);
    bool handle_req_response(LsuReqEntry *entry);
    void handle_req_end(LsuReqEntry *entry);

    Iss &iss;

    vp::Trace trace;

    LsuReqEntry *req_entry_first;
    LsuReqEntry req_entry[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];

    // True while a downstream DENY is outstanding. The core must not send
    // another request until the downstream fires ``retry()``; cleared
    // from ``data_retry``.
    vp::Signal<bool> io_req_denied;
    bool pending_fence;

    vp::Signal<bool>       stalled;
    vp::Signal<iss_reg_t>  log_addr;
    vp::Signal<iss_reg_t>  log_size;
    vp::Signal<bool>       log_is_write;

    int nb_pending_accesses;
};
