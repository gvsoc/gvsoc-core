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

#include <vp/vp.hpp>
#include <vp/signal.hpp>

Lsu::Lsu(Iss &iss)
    : iss(iss),
    log_addr(iss, "lsu/addr", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
    log_size(iss, "lsu/size", ISS_REG_WIDTH, vp::SignalCommon::ResetKind::HighZ),
    log_is_write(iss, "lsu/is_write", 1, vp::SignalCommon::ResetKind::HighZ),
    stalled(iss, "lsu/stalled", 1),
    io_req_denied(iss, "lsu/req_denied", 1)
{
    iss.traces.new_trace("lsu", &this->trace, vp::DEBUG);
    data.set_resp_meth(&Lsu::data_response);
    data.set_grant_meth(&Lsu::data_grant);
    this->iss.new_master_port("data", &data, (vp::Block *)this);

    this->req_entry_first = NULL;
    for (int i=0; i<CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING; i++)
    {
        LsuReqEntry *req = &this->req_entry[i];
        req->next = this->req_entry_first;
        req->req.set_data((uint8_t *)&req->data);
        req->req.set_second_data((uint8_t *)&req->data2);
        this->req_entry_first = req;
        req->task.callback = &Lsu::task_handle;
    }
}

void Lsu::reset(bool active)
{
    if (active)
    {
        this->io_req_denied = false;
        this->pending_fence = false;
        this->nb_pending_accesses = 0;
    }
}

bool Lsu::data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
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

bool Lsu::fence()
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

void Lsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
    // The denied request is granted, we can now allow the core to do other accesses
    Lsu *_this = (Lsu *)__this;
    _this->io_req_denied = false;
}

void Lsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    Lsu *_this = (Lsu *)__this;
    Iss *iss = &_this->iss;
    LsuReqEntry *entry = (LsuReqEntry *)req;

    _this->trace.msg("Received data response (req: %p)\n", req);

    _this->iss.exec.insn_terminate(entry->insn_entry);

    _this->handle_req_end(entry);
}

bool Lsu::handle_req_response(LsuReqEntry *entry)
{
    vp::IoReq *req = &entry->req;
    int64_t latency = req->get_latency();

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

void Lsu::handle_req_end(LsuReqEntry *entry)
{
    vp::IoReq *req = &entry->req;

    if (req->status == vp::IO_REQ_INVALID)
    {
        this->iss.exception.invalid_access(entry->pc, req->get_addr(), req->get_size(), req->get_opcode());
    }
    else
    {
        if (req->get_opcode() == vp::IoReqOpcode::READ)
        {
            // Handle classic read
            uint64_t data = entry->data;

            if (entry->is_signed)
            {
                data = iss_get_signed_value(data, req->get_size() * 8);
            }

            this->iss.regfile.set_reg(entry->reg, data);
        }
        else if (req->get_opcode() != vp::IoReqOpcode::WRITE)
        {
            // Handle atomic operations which need to write result to register
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

bool Lsu::data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, opcode: %d)\n", addr, size, opcode);
    LsuReqEntry *entry = this->get_req_entry();
    if (entry == NULL) return true;

    if (opcode == vp::IoReqOpcode::READ)
    {
        // Init to zero in case we need zero-extension
        entry->data = 0;
    }
    else
    {
        // Only read operation will not send data, writes and atomic operations will
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

    this->log_addr.set_and_release(addr);
    this->log_size.set_and_release(size);
    this->log_is_write.set_and_release(opcode != 0);

    vp::IoReqStatus err = this->data.req(req);

    if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
    {
        req->status = err;

        if (this->handle_req_response(entry))
        {
            entry->insn_entry = this->iss.exec.insn_hold(insn);
        }

        return false;
    }
    else if (err == vp::IO_REQ_PENDING)
    {
        // In case the request is pending, don't do anything, we will free
        // the request and update the scoreboard when the response is received
        entry->insn_entry = this->iss.exec.insn_hold(insn);
        return false;
    }
    else
    {
        // In case the request is denied, make sure we don't allow any other access
        // until this request is granted
        this->io_req_denied = true;
        return true;
    }
}

bool Lsu::data_req(iss_insn_t *insn, iss_addr_t addr, int size, vp::IoReqOpcode opcode, bool is_signed, int reg, int reg2)
{
    return this->data_req_aligned(insn, addr, size, opcode, is_signed, reg, reg2);
}

void Lsu::task_handle(Iss *iss, Task *task)
{
    LsuReqEntry *entry = (LsuReqEntry *)((char *)task - ((char *)&((LsuReqEntry *)0)->task - (char *)0));

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

bool Lsu::atomic(iss_insn_t *insn, iss_addr_t addr, int size, int reg_in, int reg_out,
    vp::IoReqOpcode opcode)
{
    return this->data_req_virtual(insn, addr, size, opcode, true, reg_in, reg_out);
}
