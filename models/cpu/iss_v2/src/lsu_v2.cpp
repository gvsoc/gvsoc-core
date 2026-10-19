// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * io_v2 variant of the ISS LSU. Functionally identical to the v1 model;
 * only the IO-side plumbing (status codes, callbacks, back-pressure
 * handshake) differs. See lsu_v2.hpp for a summary of the changes.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/memcheck.hpp>

LsuV2::LsuV2(Iss &iss)
    : iss(iss),
      log_addr(iss, "lsu/addr", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
      log_size(iss, "lsu/size", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
      log_is_write(iss, "lsu/is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      stalled(iss, "lsu/stalled", 1),
      io_req_denied(iss, "lsu/req_denied", 1)
{
    iss.traces.new_trace("lsu", &this->trace, vp::DEBUG);

    this->hold_on_async = iss.cfg.lsu_hold_on_async;
    // data's retry/resp callbacks are passed via the in-class initializer
    // of the IoMaster member; no set_retry_meth/set_resp_meth needed in v2.
    this->iss.new_master_port("data", &this->data, (vp::Block *)this);

    this->req_entry_first = NULL;
    for (int i = 0; i < CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING; i++)
    {
        LsuReqEntry *req = &this->req_entry[i];
        req->next = this->req_entry_first;
        req->req.set_data((uint8_t *)&req->data);
        req->req.set_second_data((uint8_t *)&req->data2);
        req->misaligned_size = 0;
        req->misaligned_byte_offset = 0;
        req->holds_core = false;
        this->req_entry_first = req;
        req->task.callback = &LsuV2::task_handle;
    }
}

void LsuV2::reset(bool active)
{
    if (active)
    {
        this->io_req_denied = false;
        this->denied_entry = NULL;
        this->granted_entry = NULL;
        // The retain count of the core is reset with it. Every entry is free
        // again, including the ones of the accesses in flight at reset, whose
        // responses are dropped with the rest of the reset system.
        this->req_entry_first = NULL;
        for (int i = 0; i < CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING; i++)
        {
            LsuReqEntry *req = &this->req_entry[i];
            req->holds_core = false;
            req->misaligned_size = 0;
            req->misaligned_byte_offset = 0;
            req->next = this->req_entry_first;
            this->req_entry_first = req;
        }
        this->pending_fence = false;
        this->nb_pending_accesses = 0;
        this->next_retire_cycle = 0;
        this->issuing_misaligned = false;
        this->pending_addr_buffer_id = 0;

#ifdef VP_MEMCHECK_ACTIVE
        // Resolve this core's alias windows from the trace engine table (declared
        // by the platform for watchpoints), so buffer checks fold aliased
        // addresses to their canonical form. All declaration components have been
        // built by the time reset is called.
        this->memcheck_aliases.clear();
        std::string path = this->iss.get_path();
        for (auto &alias : this->iss.traces.get_trace_engine()->get_aliases())
        {
            if (path.find(alias.master_pattern) != std::string::npos)
            {
                this->memcheck_aliases.push_back(
                    {alias.local_base, alias.global_base, alias.size});
            }
        }
#endif
    }
}

bool LsuV2::data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size,
                              vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
#ifdef CONFIG_GVSOC_ISS_TRAP_MISALIGNED
    // Like RI5CY without misaligned support: any access not aligned on its size
    // traps, and is not issued.
    if (addr & (size - 1))
    {
        vp_warning_always(&this->trace,
            "Misaligned %s (pc: 0x%" PRIxFULLREG ", addr: 0x%" PRIxFULLREG ", size: %d)\n",
            opcode != 0 ? "store" : "load", this->iss.exec.current_insn, addr, size);
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
        this->iss.exception.raise(this->iss.exec.current_insn,
            opcode != 0 ? ISS_EXCEPT_STORE_MISALIGNED : ISS_EXCEPT_LOAD_MISALIGNED);
#else
        // Without RISC-V exceptions there is a single trap path, whatever the id
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_ILLEGAL);
#endif
        return false;
    }
#endif

    iss_addr_t phys_addr;
    bool use_mem_array;
    bool miss;
    if (opcode != 0)
    {
        miss = this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array);
    }
    else
    {
        miss = this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array);
    }

    if (miss)
    {
        // Either an exception was raised (page fault), in which case the core will
        // jump to the handler, or a page-table walk is pending and the access must
        // be retried until the TLB is filled.
        if (!this->iss.exec.has_exception)
        {
            this->iss.exec.insn_stall();
        }
        return true;
    }

    if (this->io_req_denied || this->data_req(insn, phys_addr, size, opcode, is_signed, reg, reg2))
    {
        this->iss.exec.insn_stall();
        return true;
    }

    return false;
}

bool LsuV2::fence()
{
    if (this->nb_pending_accesses == 0)
    {
        this->pending_fence = false;
        return false;
    }

    if (this->pending_fence) return true;

    this->pending_fence = true;
    this->iss.exec.insn_stall();
    return true;
}

void LsuV2::data_retry(vp::Block *__this, vp::IoRetryChannel)
{
    LsuV2 *_this = (LsuV2 *)__this;
    LsuReqEntry *entry = _this->denied_entry;

#ifdef CONFIG_GVSOC_ISS_MMU_ENABLED
    // A denied page-table walk read is sent again first. The core is stalled on the walk, so
    // no data access can be denied at the same time.
    if (_this->iss.mmu.pte_retry()) return;
#endif

    if (entry == NULL)
    {
        // Retry broadcast with nothing held here (e.g. another master's
        // deny on a shared router input). Nothing to re-send.
        _this->io_req_denied = false;
        return;
    }

    // Re-issue the denied request synchronously, inside the retry()
    // callback: zero-buffer arbiters (log_ico_v2) keep their accept window
    // open only for the duration of this call (io_v2 contract).
    vp::IoReqStatus err = _this->data.req(&entry->req);

    if (err == vp::IO_REQ_DENIED)
    {
        // Lost the election again; keep holding for the next retry.
        return;
    }

    _this->denied_entry = NULL;
    _this->io_req_denied = false;

    if (err == vp::IO_REQ_DONE)
    {
        // Completed inline. The instruction was parked when the deny was
        // taken, so finish it through the deferred task path, honouring
        // the response's annotated latency.
        entry->timestamp = _this->iss.clock.get_cycles() + entry->req.get_latency();
        _this->iss.exec.enqueue_task(&entry->task);
    }
    else
    {
        // GRANTED: data_response will complete the parked instruction.
        _this->hold_core(entry);
    }
}


void LsuV2::hold_core(LsuReqEntry *entry)
{
    if (this->hold_on_async && !entry->holds_core)
    {
        entry->holds_core = true;
        this->iss.exec.retain_inc();
    }
}


void LsuV2::release_core(LsuReqEntry *entry)
{
    if (entry->holds_core)
    {
        entry->holds_core = false;
        this->iss.exec.retain_dec();
    }
}

vp::IoRespAck LsuV2::data_response(vp::Block *__this, vp::IoReq *req)
{
    LsuV2 *_this = (LsuV2 *)__this;
    LsuReqEntry *entry = (LsuReqEntry *)req;

    _this->trace.msg("Received data response (req: %p)\n", req);

#ifdef CONFIG_GVSOC_ISS_MMU_ENABLED
    // Route back to the MMU the responses of the page-table walk reads
    if (req == &_this->iss.mmu.pte_req)
    {
        _this->iss.mmu.handle_pte_response();
        return vp::IO_RESP_ACCEPTED;
    }
#endif

    // First beat of a misaligned access just landed: fire beat 1 and
    // keep the insn held. Do NOT touch next_retire_cycle yet — that is
    // reserved for the *final* retire when beat 1 lands.
    if (entry->misaligned_size != 0)
    {
        _this->fire_misaligned_second(entry);
        return vp::IO_RESP_ACCEPTED;
    }

    int64_t cur = _this->iss.clock.get_cycles();
    if (cur < _this->next_retire_cycle)
    {
        // Another response already retired this cycle. Defer via the
        // entry's task to the next available retire slot so the
        // single-slot scoreboard release in ExecInOrder stays
        // unambiguous (one writeback per cycle).
        entry->timestamp = _this->next_retire_cycle;
        _this->next_retire_cycle++;
        _this->iss.exec.enqueue_task(&entry->task);
        return vp::IO_RESP_ACCEPTED;
    }

    InsnEntry *insn_entry = entry->insn_entry;
    _this->iss.lsu.req_retire_hook(entry);
    _this->handle_req_end(entry);
#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    // Load-use stall: the response's dest reg becomes readable one
    // cycle after retire. Park the release in ExecInOrder's
    // single-slot delay mechanism, then commit with
    // `defer_scoreboard_release=true` so `insn_terminate` doesn't
    // also clear the bits the same cycle. Applies to any core wired
    // with a scoreboard, not just those opting into in-order commit.
    iss_insn_t *insn = _this->iss.exec.get_insn(insn_entry);
    _this->iss.exec.schedule_scoreboard_release(insn->sb_out_reg_mask,
        _this->async_load_use_delay());
    _this->iss.exec.insn_terminate(insn_entry, /*defer_scoreboard_release=*/true);
#else
    _this->iss.exec.insn_terminate(insn_entry);
#endif
    _this->next_retire_cycle = cur + 1;

    return vp::IO_RESP_ACCEPTED;
}

bool LsuV2::handle_req_response(LsuReqEntry *entry)
{
    vp::IoReq *req = &entry->req;
    // The slave's annotated latency is the cycle at which its response
    // arrives. Free the LSU pool slot at that cycle so a subsequent
    // request can re-use the slot immediately without a wasted cycle.
    // The "register file can only be consumed next cycle" semantics
    // (load-use stall) are provided separately by ExecInOrder's
    // single-slot delayed scoreboard release, scheduled in
    // `data_response` / `task_handle` whenever the regfile has a
    // scoreboard.
    int64_t latency = req->get_latency();

    if (latency > 0)
    {
        entry->timestamp = this->iss.clock.get_cycles() + latency;
        this->iss.exec.enqueue_task(&entry->task);
        return true;
    }
    else
    {
        // First beat of a misaligned access completing synchronously at
        // zero latency: fire beat 1 in the same cycle. The held insn
        // stays held. fire_misaligned_second returns true if beat 1 also
        // landed an async path (held), false otherwise.
        if (entry->misaligned_size != 0)
        {
            return this->fire_misaligned_second(entry);
        }
        this->handle_req_end(entry);
        return false;
    }
}

void LsuV2::handle_req_end(LsuReqEntry *entry)
{
    vp::IoReq *req = &entry->req;

    if (req->get_resp_status() == vp::IO_RESP_INVALID)
    {
        this->iss.exception.invalid_access(entry->pc, req->get_addr(),
                                            req->get_size(), req->get_opcode());
    }
    else
    {
        if (req->get_opcode() == vp::IoReqOpcode::READ)
        {
            uint64_t data = entry->data;
            int      size = (int)req->get_size();

            // Final beat of a misaligned READ: merge the beat-0 bytes
            // we stashed in data2 (low size0 bytes) with the beat-1
            // bytes that just arrived in data (low size1 bytes, == the
            // request size we are inspecting now). The merged value is
            // the full N-byte register the original lw/lh asked for.
            if (entry->misaligned_byte_offset != 0)
            {
                int size0 = entry->misaligned_byte_offset;
                uint64_t low_mask  = (1ULL << (size0 * 8)) - 1;
                uint64_t high_mask = (1ULL << (size * 8)) - 1;
                data = (entry->data2 & low_mask) |
                       ((data & high_mask) << (size0 * 8));
                size += size0;
                entry->misaligned_byte_offset = 0;
            }

            if (entry->is_signed)
            {
                data = iss_get_signed_value(data, size * 8);
            }

            // An LSU subclass may take loads into registers of its own
            if (!this->iss.lsu.load_end_hook(entry, data))
            {
                this->iss.regfile.set_reg(entry->reg, data);

#ifdef VP_MEMCHECK_ACTIVE
                this->memcheck_handle_load(entry, size);
#endif
            }
        }
        else if (req->get_opcode() != vp::IoReqOpcode::WRITE)
        {
            // Atomic operations that need to write the result to a register.
            uint64_t data = entry->data2;

            if (entry->is_signed)
            {
                data = iss_get_signed_value(data, req->get_size() * 8);
            }

            this->iss.regfile.set_reg(entry->reg2, data);

#ifdef VP_MEMCHECK_ACTIVE
            // Atomics do not carry a shadow yet, consider the result initialized
            if (this->iss.traces.get_trace_engine()->is_memcheck_enabled())
            {
                this->iss.regfile.memcheck_set_valid(entry->reg2, true);
            }
#endif
        }
    }

    // Clear the misaligned scratch on the way back to the free list so
    // a future aligned access doesn't see stale state.
    entry->misaligned_byte_offset = 0;
    this->release_core(entry);
    this->free_req_entry(entry);
}

bool LsuV2::data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size,
                              vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, opcode: %d)\n",
                     addr, size, opcode);
    this->granted_entry = NULL;
    LsuReqEntry *entry = this->get_req_entry();
    if (entry == NULL)
    {
        // Stalled (LSU full / retry will land us here again). We do NOT
        // account a load/store event for the retry — the bus transaction
        // hasn't actually been issued yet.
        this->trace.msg("Aborting request, no available request\n");
        return true;
    }

    // Per-bus-transaction accounting: matches RI5CY's mem_load_i /
    // mem_store_i which fire once per memory access. For misaligned
    // accesses, this fires for beat 0 here; fire_misaligned_second
    // fires the matching event for beat 1, so a misaligned lw / sw
    // counts twice (matching PCCR_LD / PCCR_ST on RI5CY).
    if (opcode == vp::IoReqOpcode::READ)
    {
        this->iss.timing.event_load_account(1);
        // Tag this load's destination(s) in the scoreboard so a later
        // dependent insn that stalls on them gets attributed to a
        // load-use hazard (PCER_LD_STALL on Ri5ky). The scoreboard
        // stores the byte opaquely.
        this->iss.regfile.sb_set_reason(insn->sb_out_reg_mask,
                                        ISS_STALL_REASON_LOAD);
    }
    else if (opcode == vp::IoReqOpcode::WRITE)
    {
        this->iss.timing.event_store_account(1);
    }

    if (opcode == vp::IoReqOpcode::READ)
    {
        entry->data = 0;
    }
    else
    {
        // Writes and atomic operations have a source operand in a
        // register; reads leave data untouched so zero-extension works.
        entry->data = this->iss.regfile.get_reg(reg);
    }
    entry->reg = reg;
    entry->reg2 = reg2;
    entry->is_signed = is_signed;
    entry->pc = insn->addr;

    vp::IoReq *req = &entry->req;
    req->prepare();
    req->set_addr(addr);
    req->set_size(size);
    req->set_opcode(opcode);
    // Start every send with a fresh "OK" status. A downstream that wants
    // to signal an error will set it to IO_RESP_INVALID before (sync) or
    // while (async) completing the request.
    req->set_resp_status(vp::IO_RESP_OK);

#ifdef VP_MEMCHECK_ACTIVE
    this->memcheck_prepare_req(entry, addr, size, opcode, reg);
#endif

    this->log_addr.set_and_release(addr);
    this->log_size.set_and_release(size);
    this->log_is_write.set_and_release(opcode != 0);

    vp::IoReqStatus err = this->data.req(req);

    if (err == vp::IO_REQ_DONE)
    {
        // Synchronous completion. The resp status already reflects
        // OK/INVALID; handle_req_response will re-check it when it fires
        // ``handle_req_end``.
        if (this->handle_req_response(entry))
        {
            entry->insn_entry = this->iss.exec.insn_hold(insn);
        }

        return false;
    }
    else if (err == vp::IO_REQ_GRANTED)
    {
        // Async: park the instruction; data_response will eventually fire
        // the completion.
        entry->insn_entry = this->iss.exec.insn_hold(insn);
        this->granted_entry = entry;
        this->hold_core(entry);
        return false;
    }
    else
    {
        // DENIED: the downstream did not accept the request. Park the
        // instruction like the GRANTED path and keep the request alive:
        // zero-buffer arbiters (log_ico_v2) only accept a re-issue from
        // INSIDE the synchronous retry() callback, so ``data_retry``
        // re-sends this request itself. A core-driven re-execution one
        // cycle later would miss the accept window and live-lock.
        // ``io_req_denied`` blocks younger accesses meanwhile, preserving
        // ordering.
        entry->insn_entry = this->iss.exec.insn_hold(insn);
        this->denied_entry = entry;
        this->io_req_denied = true;
        return false;
    }
}

bool LsuV2::data_req(iss_insn_t *insn, iss_addr_t addr, int size,
                     vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
#ifdef VP_MEMCHECK_ACTIVE
    // Buffer check at issue time, on the full access before any split, while the
    // address is still in the form the program computed it
    this->memcheck_check_access(addr, size, opcode);
#endif

    // RI5CY-style misalignment check: only data-port-boundary crosses
    // split. A halfword that fits entirely inside one port word goes
    // through the fast path even if `addr & 1 != 0`.
    iss_addr_t addr_first = addr & ~iss_addr_t(LSU_PORT_MASK);
    iss_addr_t addr_last  = (addr + size - 1) & ~iss_addr_t(LSU_PORT_MASK);
    if (addr_first != addr_last)
    {
        return this->data_req_misaligned(insn, addr, size, opcode, is_signed,
                                         reg, reg2);
    }
    return this->data_req_aligned(insn, addr, size, opcode, is_signed, reg, reg2);
}

bool LsuV2::data_req_misaligned(iss_insn_t *insn, iss_addr_t addr, int size,
                                vp::IoReqOpcode opcode, bool is_signed,
                                int reg, int reg2)
{
    // Split into two aligned beats:
    //   beat 0: [addr, next-port-boundary)         — size0 bytes
    //   beat 1: [next-port-boundary, addr + size)  — size1 bytes
    // The first beat reuses ``data_req_aligned`` so it pays exactly one
    // memory-latency unit. The response path (data_response / task_handle
    // / handle_req_response sync) sees ``entry->misaligned_size != 0``
    // and calls ``fire_misaligned_second`` instead of retiring the insn
    // — that re-arms the same entry for beat 1, going through
    // ``data.req()`` again so we pay memory latency twice. This matches
    // RI5CY's serial ``misaligned_st`` FSM.
    iss_addr_t addr1 = (addr + size - 1) & ~iss_addr_t(LSU_PORT_MASK);
    int        size0 = (int)(addr1 - addr);
    int        size1 = size - size0;

    this->trace.msg("Misaligned data request (addr: 0x%lx, size: 0x%x, "
                    "size0: 0x%x, size1: 0x%x, opcode: %d)\n",
                    addr, size, size0, size1, opcode);

    this->iss.timing.event_misaligned_account(1);

    // For writes we need the bytes that go into beat 1 to be visible at
    // offset 0 of entry->data when beat 1 fires (the IoReq always reads
    // from the start of its data buffer). For reads we will save the
    // beat-0 bytes that beat 1 is about to clobber. Both paths are
    // handled in fire_misaligned_second using entry->data2 as scratch:
    //   - On entry to fire_misaligned_second:
    //       reads:  save beat-0 result bytes into data2 low size0 bytes
    //       writes: copy original-register beat-1 bytes (the high part
    //               of data2) down to data low size1 bytes
    //   - On final handle_req_end (after beat 1):
    //       reads: merge low size1 bytes of data with saved size0 low
    //              bytes of data2 (saved part shifted up by size1*8)
    //
    // To make this work, we issue beat 0 with the FULL N-byte register
    // value pre-loaded for writes (data_req_aligned already does this
    // because it reads register `reg`), and we stash the full value in
    // data2 so fire_misaligned_second can recover the high bytes.

    // Peek the entry that data_req_aligned is about to allocate (head of
    // the free list). After the call returns, this same entry is the
    // in-flight one — `data_req_aligned` keeps it in flight on the
    // sync-DONE / GRANTED / DENIED-held paths, all of which return
    // ``false`` (no stall). Only the LSU-full path (``true``) leaves no
    // entry allocated, so the misaligned bookkeeping is committed on
    // ``false``.
    LsuReqEntry *entry = this->req_entry_first;

    // For writes, snapshot the full register value into data2 BEFORE
    // data_req_aligned overwrites entry->data with the truncated beat-0
    // payload. fire_misaligned_second will shift this down for beat 1.
    uint64_t full_write_data = 0;
    if (opcode == vp::IoReqOpcode::WRITE && entry != nullptr)
    {
        full_write_data = this->iss.regfile.get_reg(reg);
    }

    this->issuing_misaligned = true;
    bool stalled = this->data_req_aligned(insn, addr, size0, opcode,
                                          is_signed, reg, reg2);
    this->issuing_misaligned = false;

    if (!stalled && entry != nullptr)
    {
        entry->misaligned_size        = size1;
        entry->misaligned_addr        = addr1;
        entry->misaligned_byte_offset = size0;
        if (opcode == vp::IoReqOpcode::WRITE)
        {
            entry->data2 = full_write_data;
            // The second half occupies the execute stage one more cycle, so
            // the next instruction starts one cycle later. A misaligned load
            // needs nothing here: its user already waits for the second half.
            if (this->iss.cfg.lsu_misaligned_store_stall)
            {
                this->iss.exec.stall_cycles_inc(1);
            }
        }
    }

    return stalled;
}

bool LsuV2::fire_misaligned_second(LsuReqEntry *entry)
{
    int  size0 = entry->misaligned_byte_offset;
    int  size1 = entry->misaligned_size;
    bool is_wr = entry->req.get_opcode() == vp::IoReqOpcode::WRITE;

    this->trace.msg("Misaligned second beat (addr: 0x%lx, size: 0x%x, "
                    "is_write: %d)\n", entry->misaligned_addr, size1, is_wr);

    if (!is_wr)
    {
        // Reads: beat 0 wrote its size0 bytes into entry->data at offset 0.
        // Save them in data2 so the upcoming beat 1 (which writes its
        // result to entry->data at offset 0 too) does not clobber them.
        // handle_req_end on beat 1 will merge size1 (low) | data2 (shifted
        // up by size1*8) so the final regfile write reflects the true
        // memory order: [beat0_byte0 .. beat0_byteN-1 || beat1_byte0 ..].
        entry->data2 = entry->data & ((1ULL << (size0 * 8)) - 1);
    }
    else
    {
        // Writes: beat 1 should ship bytes [size0 .. size-1] of the
        // original register. We saved the full value in data2 during
        // data_req_misaligned; shift it down so beat 1's IoReq picks up
        // those bytes at offset 0.
        entry->data = entry->data2 >> (size0 * 8);
    }

    // Per-bus-transaction accounting for the second beat — see
    // data_req_aligned for beat 0. Matches RI5CY's mem_load_i /
    // mem_store_i pulse per bus access.
    if (is_wr)
    {
        this->iss.timing.event_store_account(1);
    }
    else
    {
        this->iss.timing.event_load_account(1);
    }

    // Re-arm the IoReq for the second beat. data pointer is fixed to
    // &entry->data at construction time, so only addr/size/status need
    // updating; opcode and is_signed are unchanged.
    vp::IoReq *req = &entry->req;
    req->prepare();
    req->set_addr(entry->misaligned_addr);
    req->set_size(size1);
    req->set_resp_status(vp::IO_RESP_OK);

    // Mark this as the final beat — handle_req_end on the response will
    // perform the read-side merge (see below) and retire the insn.
    entry->misaligned_size = 0;
    // Remember the split sizes so handle_req_end can merge correctly.
    // We reuse misaligned_byte_offset (still holds size0) for that.

    vp::IoReqStatus err = this->data.req(req);
    if (err == vp::IO_REQ_DONE)
    {
        // Synchronous DONE: route through the normal latency / sync
        // handlers. handle_req_response will either fire a task (latency
        // > 0) or call handle_req_end immediately (latency 0).
        return this->handle_req_response(entry);
    }
    if (err == vp::IO_REQ_DENIED)
    {
        // Beat 1 denied: register the entry so data_retry re-issues it
        // synchronously (see data_retry). Instruction already held.
        this->denied_entry = entry;
        this->io_req_denied = true;
        return true;
    }
    // GRANTED: data_response will land the completion. Instruction is
    // already held from beat 0.
    return true;
}

void LsuV2::task_handle(Iss *iss, Task *task)
{
    LsuReqEntry *entry = (LsuReqEntry *)((char *)task
        - ((char *)&((LsuReqEntry *)0)->task - (char *)0));

    int64_t cur = iss->clock.get_cycles();
    if (cur < entry->timestamp)
    {
        iss->exec.enqueue_task(task);
        return;
    }

    // First beat of a misaligned access just timed out: fire beat 1.
    // No retire-slot accounting yet — beat 1 will do its own.
    if (entry->misaligned_size != 0)
    {
        iss->lsu.fire_misaligned_second(entry);
        return;
    }

    // We have reached the unique retire slot (entry->timestamp) that was
    // reserved for this completion when it was deferred (in data_response, or
    // an earlier pass through this handler). That slot is ours, so retire now.
    //
    // Do NOT re-apply the `cur < next_retire_cycle` gate here: next_retire_cycle
    // was already advanced past our slot when the slot was reserved, so the
    // check would be permanently true and the task would keep pushing its own
    // slot one cycle further every cycle (next_retire_cycle tracking cur+1),
    // livelocking the core until an unrelated event happens to break the tie.

    InsnEntry *insn_entry = entry->insn_entry;
    // handle_req_end releases the core, remember if this access was holding it
    bool held_core = entry->holds_core;
    iss->lsu.req_retire_hook(entry);
    iss->lsu.handle_req_end(entry);
#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    // Same load-use 1-cycle stall as the async path: schedule the
    // dest reg's scoreboard release for next cycle, and tell
    // `insn_terminate` to leave the scoreboard alone so the parked
    // mask fires one cycle from now.
    iss_insn_t *insn = iss->exec.get_insn(insn_entry);
    iss->exec.schedule_scoreboard_release(insn->sb_out_reg_mask,
        held_core ? iss->lsu.async_load_use_delay() : 1);
    iss->exec.insn_terminate(insn_entry, /*defer_scoreboard_release=*/true);
#else
    iss->exec.insn_terminate(insn_entry);
#endif
    // Advance the next free retire slot, but never rewind it below slots that
    // were already reserved for other in-flight completions.
    if (iss->lsu.next_retire_cycle < cur + 1)
        iss->lsu.next_retire_cycle = cur + 1;
}

bool LsuV2::atomic(iss_insn_t *insn, iss_addr_t addr, int size,
                    int reg_in, int reg_out, vp::IoReqOpcode opcode)
{
    return this->data_req_virtual(insn, addr, size, opcode, true, reg_in, reg_out);
}

#ifdef VP_MEMCHECK_ACTIVE

void LsuV2::memcheck_check_access(iss_addr_t addr, int size, vp::IoReqOpcode opcode)
{
    // The provenance of the base register was latched by the ISA handler through
    // memcheck_access_reg. Consume it here; an ID can only be attached when memory
    // checking is enabled, so no runtime gate is needed.
    uint32_t buffer_id = this->pending_addr_buffer_id;
    this->pending_addr_buffer_id = 0;
    if (buffer_id == 0)
    {
        return;
    }

    // Fold master-local aliases to the canonical global address, so the bounds
    // check compares addresses in the allocator's address space
    uint64_t folded = addr;
    for (auto &alias : this->memcheck_aliases)
    {
        if (folded >= alias.local_base && folded < alias.local_base + alias.size)
        {
            folded = alias.global_base + (folded - alias.local_base);
            break;
        }
    }

    std::string error;
    if (!this->iss.get_memcheck()->check_access(folded, size, buffer_id,
        opcode != vp::IoReqOpcode::READ, error))
    {
        this->memcheck_fault_report(error);
    }
}

void LsuV2::memcheck_fault_report(const std::string &error)
{
    // Complete the structured fault record for front-ends
    vp::MemCheck *mc = this->iss.get_memcheck();
    mc->fault.time = this->iss.time.get_time();
    mc->fault.core = this->iss.get_path();
    mc->fault.pc = this->iss.exec.current_insn;

    if (this->iss.gdbserver.is_enabled())
    {
        // Halt the core and notify GDB with a bus error so the fault can be
        // inspected and execution resumed, like the uninitialized-value faults
        this->trace.force_warning_no_error("%s\n", error.c_str());
        if (!this->iss.exec.halted.get())
        {
            this->iss.exec.retain_inc();
            this->iss.exec.halted.set(true);
        }
        this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver,
            vp::Gdbserver_engine::SIGNAL_BUS);
    }
    else if (mc->fault_stop())
    {
        // A front-end is attached (GUI or console), the simulation pauses on the
        // fault like on a watchpoint hit; execution can be resumed from there
        this->trace.force_warning_no_error("%s\n", error.c_str());
    }
    else
    {
        // Batch mode: report and apply the werror policy
        this->trace.force_warning("%s\n", error.c_str());
    }
}

void LsuV2::memcheck_prepare_req(LsuReqEntry *entry, iss_addr_t addr, int size,
    vp::IoReqOpcode opcode, int reg)
{
    if (!this->iss.traces.get_trace_engine()->is_memcheck_enabled())
    {
        return;
    }

    vp::IoReq *req = &entry->req;

    // Misaligned beats carry no data shadow. Writes without shadow are considered
    // initialized by the target and reads leave the destination register fully
    // valid, so misalignment can not raise false faults.
    if (this->issuing_misaligned)
    {
        return;
    }

    if (opcode == vp::IoReqOpcode::READ)
    {
        // Preset the shadow as valid so that bytes coming from a target without
        // shadow support are considered initialized
        memset(entry->memcheck_data, 0xFF, size);
        req->set_memcheck_data(entry->memcheck_data);
    }
    else if (opcode == vp::IoReqOpcode::WRITE)
    {
        // Ship the per-bit validity of the stored register along with the data
        iss_reg_t mask = this->iss.regfile.memcheck_get(reg);
        int copy_size = size < (int)sizeof(iss_reg_t) ? size : (int)sizeof(iss_reg_t);
        memcpy(entry->memcheck_data, &mask, copy_size);
        if (size > copy_size)
        {
            memset(entry->memcheck_data + copy_size, 0xFF, size - copy_size);
        }
        req->set_memcheck_data(entry->memcheck_data);

        // Pointer-sized aligned stores also ship the provenance of the stored
        // value, so that pointers spilled to memory keep their buffer attached
        if (size == (int)sizeof(iss_reg_t) && (addr & (sizeof(iss_reg_t) - 1)) == 0)
        {
            req->set_memcheck_data_id(this->iss.regfile.memcheck_get_id(reg));
        }
    }
}

void LsuV2::memcheck_handle_load(LsuReqEntry *entry, int size)
{
    if (!this->iss.traces.get_trace_engine()->is_memcheck_enabled())
    {
        return;
    }

    vp::IoReq *req = &entry->req;

    if (entry->reg == 0)
    {
        return;
    }

    if (req->get_memcheck_data() == NULL)
    {
        // No shadow was attached (misaligned access), no information
        this->iss.regfile.memcheck_set_valid(entry->reg, true);
        return;
    }

    // Build the register validity mask from the per-byte shadow, extending the
    // upper part according to the zero/sign extension of the load
    iss_reg_t mask;
    if (size >= (int)sizeof(iss_reg_t))
    {
        memcpy(&mask, entry->memcheck_data, sizeof(iss_reg_t));
    }
    else
    {
        mask = 0;
        memcpy(&mask, entry->memcheck_data, size);
        if (entry->is_signed)
        {
            // The upper bits replicate the sign bit, they are valid only if it is
            if ((entry->memcheck_data[size - 1] >> 7) & 1)
            {
                mask |= ((iss_reg_t)-1) << (size * 8);
            }
        }
        else
        {
            // Zero-extension, the upper bits are architecturally 0 hence valid
            mask |= ((iss_reg_t)-1) << (size * 8);
        }
    }
    this->iss.regfile.memcheck_set(entry->reg, mask);

    // Pointer-sized aligned loads restore the provenance stored in the accessed
    // slot, so that pointers reloaded from memory keep naming their buffer
    if (size == (int)sizeof(iss_reg_t)
        && (req->get_addr() & (sizeof(iss_reg_t) - 1)) == 0)
    {
        this->iss.regfile.memcheck_set_id(entry->reg, req->get_memcheck_data_id());
    }
}

#endif
