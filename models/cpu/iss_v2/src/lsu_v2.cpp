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

LsuV2::LsuV2(Iss &iss)
    : iss(iss),
      log_addr(iss, "lsu/addr", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
      log_size(iss, "lsu/size", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
      log_is_write(iss, "lsu/is_write", 1, vp::SignalCommon::ResetKind::HighZ),
      stalled(iss, "lsu/stalled", 1),
      io_req_denied(iss, "lsu/req_denied", 1)
{
    iss.traces.new_trace("lsu", &this->trace, vp::DEBUG);
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
        this->req_entry_first = req;
        req->task.callback = &LsuV2::task_handle;
    }
}

void LsuV2::reset(bool active)
{
    if (active)
    {
        this->io_req_denied = false;
        this->pending_fence = false;
        this->nb_pending_accesses = 0;
        this->next_retire_cycle = 0;
    }
}

bool LsuV2::data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size,
                              vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (opcode != 0)
    {
        if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array)) return false;
    }
    else
    {
        if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array)) return false;
    }

    if (this->io_req_denied || this->data_req(insn, addr, size, opcode, is_signed, reg, reg2))
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

void LsuV2::data_retry(vp::Block *__this)
{
    // A previously-denied request may now be re-sent. The core stall
    // driven by io_req_denied clears once this flag goes false; the core
    // will then re-issue its pending instruction, which re-enters
    // ``data_req_aligned`` with a fresh entry.
    LsuV2 *_this = (LsuV2 *)__this;
    _this->io_req_denied = false;
}

void LsuV2::data_response(vp::Block *__this, vp::IoReq *req)
{
    LsuV2 *_this = (LsuV2 *)__this;
    LsuReqEntry *entry = (LsuReqEntry *)req;

    _this->trace.msg("Received data response (req: %p)\n", req);

    // First beat of a misaligned access just landed: fire beat 1 and
    // keep the insn held. Do NOT touch next_retire_cycle yet — that is
    // reserved for the *final* retire when beat 1 lands.
    if (entry->misaligned_size != 0)
    {
        _this->fire_misaligned_second(entry);
        return;
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
        return;
    }

    InsnEntry *insn_entry = entry->insn_entry;
    _this->handle_req_end(entry);
#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    // Load-use stall: the response's dest reg becomes readable one
    // cycle after retire. Park the release in ExecInOrder's
    // single-slot delay mechanism, then commit with
    // `defer_scoreboard_release=true` so `insn_terminate` doesn't
    // also clear the bits the same cycle. Applies to any core wired
    // with a scoreboard, not just those opting into in-order commit.
    iss_insn_t *insn = _this->iss.exec.get_insn(insn_entry);
    _this->iss.exec.schedule_scoreboard_release(insn->sb_out_reg_mask);
    _this->iss.exec.insn_terminate(insn_entry, /*defer_scoreboard_release=*/true);
#else
    _this->iss.exec.insn_terminate(insn_entry);
#endif
    _this->next_retire_cycle = cur + 1;
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

            this->iss.regfile.set_reg(entry->reg, data);
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
        }
    }

    // Clear the misaligned scratch on the way back to the free list so
    // a future aligned access doesn't see stale state.
    entry->misaligned_byte_offset = 0;
    this->free_req_entry(entry);
}

bool LsuV2::data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size,
                              vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, opcode: %d)\n",
                     addr, size, opcode);
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
        return false;
    }
    else
    {
        // DENIED: the downstream did not accept the request. Free the
        // entry (the slave has not taken ownership), and block the core
        // from issuing another access until ``data_retry`` clears the
        // flag. The core's stall mechanism will then re-issue the
        // instruction, which lands back in ``data_req_aligned`` with a
        // fresh entry carrying the current register file state.
        this->free_req_entry(entry);
        this->io_req_denied = true;
        return true;
    }
}

bool LsuV2::data_req(iss_insn_t *insn, iss_addr_t addr, int size,
                     vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    // RI5CY-style misalignment check: only word-boundary crosses split.
    // A halfword that fits entirely inside one 4-byte word goes through
    // the fast path even if `addr & 1 != 0`.
    iss_addr_t addr_first = addr & ~iss_addr_t(0x3);
    iss_addr_t addr_last  = (addr + size - 1) & ~iss_addr_t(0x3);
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
    //   beat 0: [addr, next-word-boundary)         — size0 bytes
    //   beat 1: [next-word-boundary, addr + size)  — size1 bytes
    // The first beat reuses ``data_req_aligned`` so it pays exactly one
    // memory-latency unit. The response path (data_response / task_handle
    // / handle_req_response sync) sees ``entry->misaligned_size != 0``
    // and calls ``fire_misaligned_second`` instead of retiring the insn
    // — that re-arms the same entry for beat 1, going through
    // ``data.req()`` again so we pay memory latency twice. This matches
    // RI5CY's serial ``misaligned_st`` FSM.
    iss_addr_t addr1 = (addr + size - 1) & ~iss_addr_t(0x3);
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
    // in-flight one — `data_req_aligned` doesn't free it on the
    // sync-DONE / GRANTED paths. The DENIED path frees it, so we'll
    // only commit the misaligned bookkeeping if the call returned
    // ``false`` (no stall) — which is exactly the path that leaves the
    // entry in flight.
    LsuReqEntry *entry = this->req_entry_first;

    // For writes, snapshot the full register value into data2 BEFORE
    // data_req_aligned overwrites entry->data with the truncated beat-0
    // payload. fire_misaligned_second will shift this down for beat 1.
    uint64_t full_write_data = 0;
    if (opcode == vp::IoReqOpcode::WRITE && entry != nullptr)
    {
        full_write_data = this->iss.regfile.get_reg(reg);
    }

    bool stalled = this->data_req_aligned(insn, addr, size0, opcode,
                                          is_signed, reg, reg2);

    if (!stalled && entry != nullptr)
    {
        entry->misaligned_size        = size1;
        entry->misaligned_addr        = addr1;
        entry->misaligned_byte_offset = size0;
        if (opcode == vp::IoReqOpcode::WRITE)
        {
            entry->data2 = full_write_data;
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
    // GRANTED/DENIED: response or retry will land us back in
    // data_response. Instruction is already held from beat 0.
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

    if (cur < iss->lsu.next_retire_cycle)
    {
        // Another response already retired this cycle. Push our slot
        // out to the next available retire cycle and re-enqueue.
        entry->timestamp = iss->lsu.next_retire_cycle;
        iss->lsu.next_retire_cycle++;
        iss->exec.enqueue_task(task);
        return;
    }

    InsnEntry *insn_entry = entry->insn_entry;
    iss->lsu.handle_req_end(entry);
#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
    // Same load-use 1-cycle stall as the async path: schedule the
    // dest reg's scoreboard release for next cycle, and tell
    // `insn_terminate` to leave the scoreboard alone so the parked
    // mask fires one cycle from now.
    iss_insn_t *insn = iss->exec.get_insn(insn_entry);
    iss->exec.schedule_scoreboard_release(insn->sb_out_reg_mask);
    iss->exec.insn_terminate(insn_entry, /*defer_scoreboard_release=*/true);
#else
    iss->exec.insn_terminate(insn_entry);
#endif
    iss->lsu.next_retire_cycle = cur + 1;
}

bool LsuV2::atomic(iss_insn_t *insn, iss_addr_t addr, int size,
                    int reg_in, int reg_out, vp::IoReqOpcode opcode)
{
    return this->data_req_virtual(insn, addr, size, opcode, true, reg_in, reg_out);
}
