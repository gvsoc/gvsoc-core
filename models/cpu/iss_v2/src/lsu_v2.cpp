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

    _this->iss.exec.insn_terminate(entry->insn_entry);

    _this->handle_req_end(entry);
}

bool LsuV2::handle_req_response(LsuReqEntry *entry)
{
    vp::IoReq *req = &entry->req;
    // Add one cycle to the annotated latency to account for the fact that
    // the response arrives this cycle but the register file can only
    // consume it on the next one.
    int64_t latency = req->get_latency() + 1;

    if (latency > 0)
    {
        entry->timestamp = this->iss.clock.get_cycles() + latency;
        this->iss.exec.enqueue_task(&entry->task);
        return true;
    }
    else
    {
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

            if (entry->is_signed)
            {
                data = iss_get_signed_value(data, req->get_size() * 8);
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
        this->trace.msg("Aborting request, no available request\n");
        return true;
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
    return this->data_req_aligned(insn, addr, size, opcode, is_signed, reg, reg2);
}

void LsuV2::task_handle(Iss *iss, Task *task)
{
    LsuReqEntry *entry = (LsuReqEntry *)((char *)task
        - ((char *)&((LsuReqEntry *)0)->task - (char *)0));

    if (iss->clock.get_cycles() >= entry->timestamp)
    {
        iss->lsu.handle_req_end(entry);
        iss->exec.insn_terminate(entry->insn_entry);
    }
    else
    {
        iss->exec.enqueue_task(task);
    }
}

bool LsuV2::atomic(iss_insn_t *insn, iss_addr_t addr, int size,
                    int reg_in, int reg_out, vp::IoReqOpcode opcode)
{
    return this->data_req_virtual(insn, addr, size, opcode, true, reg_in, reg_out);
}
