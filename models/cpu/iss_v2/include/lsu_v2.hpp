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
 * The publicly-visible ``data`` field keeps the same name as v1 so
 * external users (``ara_vlsu.cpp``, ...) can talk to either variant
 * through the same ``iss.lsu.data`` handle. Those callers still need to
 * guard any v1-specific API use (``req->init()``, ``IO_REQ_OK``,
 * ``get_full_latency()``) with ``#ifdef CONFIG_GVSOC_ISS_LSU_V2``.
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

    // Misaligned-access state. RI5CY-style: a word access that straddles
    // a 4-byte boundary is split into two sequential aligned beats. The
    // first beat carries the low part starting at the original address;
    // when its response lands, the same entry is re-armed with the
    // parameters below to issue the high part. ``misaligned_size``
    // is 0 for an aligned access (no second beat to fire).
    int        misaligned_size;
    iss_addr_t misaligned_addr;
    int        misaligned_byte_offset;
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
    bool data_req_misaligned(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2);

    // Generic extension hooks, called through the configured LSU type
    // (CONFIG_GVSOC_ISS_LSU) so an LSU subclass can intercept them
    // statically (see the ri5ky LSU and its p.elw event load). No-ops here.
    // Called when a request retires (both the async response and the timed
    // task paths), before handle_req_end.
    inline void req_retire_hook(LsuReqEntry *entry) {}
    // Called when the external IRQ unit receives a new interrupt state.
    inline void irq_req_hook(int irq, bool irq_enabled) {}

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

    vp::IoMaster data{&LsuV2::data_retry, &LsuV2::data_response};

protected:
    // Allocate a request. Returns NULL if no entry is free and the core
    // must stall.
    inline LsuReqEntry *get_req_entry();
    // Free a request so that it can be used for another access.
    inline void free_req_entry(LsuReqEntry *entry);

private:
    bool load_req(iss_insn_t *insn, iss_addr_t addr, int size, int reg, bool is_signed);
    static void task_handle(Iss *iss, Task *task);

    // io_v2 downstream callbacks. ``retry`` fires without a request
    // argument — it is a pure "ready-again" signal per the v2 protocol.
    static void data_retry(vp::Block *__this, vp::IoRetryChannel);
    static vp::IoRespAck data_response(vp::Block *__this, vp::IoReq *req);
    bool handle_req_response(LsuReqEntry *entry);
    void handle_req_end(LsuReqEntry *entry);
    // Re-arm and re-issue ``entry`` for the second beat of a misaligned
    // access. Returns true if a second beat was fired (entry still in
    // flight); false if the entry was aligned (no second beat needed).
    bool fire_misaligned_second(LsuReqEntry *entry);

protected:
    Iss &iss;

    vp::Trace trace;

    LsuReqEntry *req_entry_first;
    // Entry parked by the last data_req_aligned that returned GRANTED
    // (scratch used by LSU subclasses, e.g. the ri5ky p.elw, to detect
    // the park). Cleared at issue.
    LsuReqEntry *granted_entry;
    LsuReqEntry req_entry[CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING];

    // True while a downstream DENY is outstanding. The core must not send
    // another request until the downstream fires ``retry()``; cleared
    // from ``data_retry``.
    vp::Signal<bool> io_req_denied;
    // The denied in-flight entry, kept alive so ``data_retry`` can re-issue
    // it synchronously inside the retry() callback. Zero-buffer arbiters
    // (log_ico_v2) keep their accept window open only for the duration of
    // that call — a deferred re-execution by the core misses it and
    // live-locks.
    LsuReqEntry *denied_entry;
    bool pending_fence;

    vp::Signal<bool>       stalled;
    vp::Signal<iss_reg_t>  log_addr;
    vp::Signal<iss_reg_t>  log_size;
    vp::Signal<bool>       log_is_write;

    int nb_pending_accesses;

    // Earliest cycle at which the next response is allowed to retire.
    // A response landing at `cycle >= next_retire_cycle` retires now
    // and advances `next_retire_cycle = cycle + 1`. A response landing
    // earlier (multiple responses on the same cycle, with
    // `nb_outstanding > 1` and mixed latencies) self-defers via its
    // entry's task to the value in `next_retire_cycle`. Serializes
    // writebacks one per cycle so the single-slot scoreboard release
    // in `ExecInOrder` never sees overlapping pushes.
    int64_t next_retire_cycle;
};
