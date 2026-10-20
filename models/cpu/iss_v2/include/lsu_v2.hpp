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

// CONFIG_GVSOC_ISS_LSU_WIDTH is the width in bytes of the LSU's data port,
// always emitted by the LsuV2 generator module (riscv.py, `width` parameter,
// default 4 = the historical 32-bit port). An access that stays inside one
// port word is a single memory request; one that crosses a port boundary is
// split into two serialized beats (`data_req_misaligned`), each paying the
// memory latency, since beat 1 is only issued once beat 0's response is
// back. A core with a wider data port sets the width accordingly, so an
// aligned access of that size stays a single request.
#define LSU_PORT_MASK (CONFIG_GVSOC_ISS_LSU_WIDTH - 1)

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

    // True while this access retains the core (see LsuV2::hold_on_async).
    bool holds_core;

    // True for an access returning data (load or atomic), which counts
    // against CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING_LOADS
    bool is_load;

    // Memcheck shadow of the transferred data (per-byte validity of ``data``)
    uint8_t  memcheck_data[8];
};

class LsuV2
{
public:
    LsuV2(Iss &iss);

    void start();
    void stop() {}
    void reset(bool active);

    // Not implemented yet, just defined to keep compatibility with ISA files
    inline void stack_access_check(int reg, iss_addr_t addr) {}

    bool data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2=0);
    // Data port access with an already translated address
    bool data_req_phys(iss_insn_t *insn, iss_addr_t phys_addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2);
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
    // Called when a load returns its value, before it is written to
    // entry->reg. Returns true if the subclass took the value, which then
    // goes neither to the register file nor to its memcheck shadow.
    inline bool load_end_hook(LsuReqEntry *entry, iss_reg_t data) { return false; }

    // Number of accesses in flight (held instructions or deferred
    // completions). Used by the DBT dispatch to enter translated code
    // only from a clean state.
    inline int get_nb_pending_accesses() { return this->nb_pending_accesses; }

#ifdef CONFIG_GVSOC_ISS_MEMORY
    // True if the size bytes at phys_addr can be accessed directly in the memory
    // array. Used by the MMU to tell it to the loads and stores.
    inline bool mem_array_contains(iss_addr_t phys_addr, iss_addr_t size);
#endif

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

    // Buffer ID of the base register of the access being issued, latched by
    // Regfile::memcheck_access_reg from the ISA handlers and consumed by the next
    // data request, which stamps it on the IoReq
    uint32_t pending_addr_buffer_id;

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
#ifdef VP_MEMCHECK_ACTIVE
    // Check the access against the buffer named by the latched address provenance.
    // The address is folded to its canonical form first, using the alias windows
    // declared to the trace engine for watchpoints.
    void memcheck_check_access(iss_addr_t addr, int size, vp::IoReqOpcode opcode);
    // Report a buffer fault: notify GDB (bus error), pause for an attached
    // front-end, or apply the werror policy in batch mode.
    void memcheck_fault_report(const std::string &error);
    // Attach the memcheck shadow and address provenance to the request being issued
    void memcheck_prepare_req(LsuReqEntry *entry, iss_addr_t addr, int size,
        vp::IoReqOpcode opcode, int reg);
    // Propagate the loaded shadow and provenance into the destination register
    void memcheck_handle_load(LsuReqEntry *entry, int size);

    // This core's alias windows, filtered from the trace engine table at reset so
    // the per-access folding is a couple of compares
    struct MemcheckAlias
    {
        uint64_t local_base;
        uint64_t global_base;
        uint64_t size;
    };
    std::vector<MemcheckAlias> memcheck_aliases;
#endif

    // io_v2 downstream callbacks. ``retry`` fires without a request
    // argument — it is a pure "ready-again" signal per the v2 protocol.
    static void data_retry(vp::Block *__this, vp::IoRetryChannel);
    static vp::IoRespAck data_response(vp::Block *__this, vp::IoReq *req);
    bool handle_req_response(LsuReqEntry *entry);
    void handle_req_end(LsuReqEntry *entry);
    // Value written to the destination register of a load of ``size`` bytes:
    // sign-extended if asked, NaN-boxed if the register is a FP one narrower
    // than the load
    inline uint64_t load_value(int reg, uint64_t data, int size, bool is_signed);
    // Stall of a load or store whose address translation missed in the TLB, unless
    // it raised a page fault. The access is retried until the TLB is filled.
    inline bool translation_stall();
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

    // Optional behaviour, off by default, enabled with the core configuration
    // field ``lsu_hold_on_async``. Some cores are only granted a data access when
    // its response is about to come back (RI5CY behind the PULP cluster core
    // demux, for anything which is not the TCDM), so they cannot execute
    // anything during the access, load or store alike. When set, an access
    // answered asynchronously retains the core until its response is back.
    bool hold_on_async = false;
    // With the core held, the response stands for the grant of the access:
    // the core resumes on the next cycle, but the data of a load only comes
    // one cycle after the grant, so a dependent instruction waits one more.
    int async_load_use_delay() { return this->hold_on_async ? 2 : 1; }
    // Retain the core for this access, and release it.
    void hold_core(LsuReqEntry *entry);
    void release_core(LsuReqEntry *entry);
    // True while data_req_misaligned drives data_req_aligned for beat 0, so the
    // memcheck preparation knows not to attach a data shadow
    bool issuing_misaligned;

#ifdef CONFIG_GVSOC_ISS_MEMORY
    // Direct access to the array of a memory (RiscvConfig memory_start /
    // memory_size), retrieved at start through the meminfo port
    vp::WireMaster<void *> meminfo;
    uint8_t *mem_array = nullptr;
    iss_addr_t memory_start;
    iss_addr_t memory_size;
#endif

    vp::Signal<bool>       stalled;
    vp::Signal<iss_reg_t>  log_addr;
    vp::Signal<iss_reg_t>  log_size;
    vp::Signal<bool>       log_is_write;

    int nb_pending_accesses;
    // Loads and atomics in flight, at most CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING_LOADS
    // of the CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING accesses
    int nb_pending_loads;

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
