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
#include "iss.hpp"

int Lsu::data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{

    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

    this->iss.decode_trace.msg("Misaligned data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

    this->iss.timing.event_misaligned_account(1);

    // The access is a misaligned access
    // Change the event so that we can do the first access now and the next access
    // during the next cycle
    int size0 = addr1 - addr;
    int size1 = size - size0;

    this->iss.misaligned_access.set(true);

    // Remember the access properties for the second access
    this->iss.misaligned_size = size1;
    this->iss.misaligned_data = data_ptr + size0;
    this->iss.misaligned_addr = addr1;
    this->iss.misaligned_is_write = is_write;

    // And do the first one now
    int err = data_req_aligned(addr, data_ptr, size0, is_write);
    if (err == vp::IO_REQ_OK)
    {
        // As the transaction is split into 2 parts, we must tell the ISS
        // that the access is pending as the instruction must be executed
        // only when the second access is finished.
        this->iss.exec.instr_event->meth_set(this, &Lsu::exec_misaligned);
        this->iss.timing.stall_load_account(this->iss.io_req.get_latency() + 1);
        this->iss.exec.insn_hold();
        return 1;
    }
    else
    {
        this->iss.trace.force_warning("UNIMPLEMENTED AT %s %d, error during misaligned access\n", __FILE__, __LINE__);
        return 0;
    }
}

void Lsu::data_grant(void *__this, vp::io_req *req)
{
}

void Lsu::data_response(void *__this, vp::io_req *req)
{
    Lsu *_this = (Lsu *)__this;
    Iss *iss = &_this->iss;

    iss->exec.stalled_dec();

    iss->trace.msg("Received data response (stalled: %d)\n", iss->exec.stalled.get());

    iss->wakeup_latency = req->get_latency();
    if (iss->misaligned_access.get())
    {
        iss->misaligned_access.set(false);
    }
    else
    {
        // First call the ISS to finish the instruction
        iss->state.stall_callback(&iss->lsu);
        iss->exec.insn_terminate();
    }
}

void Lsu::exec_misaligned(void *__this, vp::clock_event *event)
{
    Lsu *_this = (Lsu *)__this;
    Iss *iss = &_this->iss;

    iss->trace.msg(vp::trace::LEVEL_TRACE, "Handling second half of misaligned access\n");

    // As the 2 load accesses for misaligned access are generated by the
    // wrapper, we need to account the extra access here.
    iss->timing.stall_misaligned_account();

    if (_this->data_req_aligned(iss->misaligned_addr, iss->misaligned_data,
                                iss->misaligned_size, iss->misaligned_is_write) == vp::IO_REQ_OK)
    {
        iss->dump_trace_enabled = true;
        iss->exec.insn_terminate();
        iss->timing.stall_load_account(iss->io_req.get_latency() + 1);
        iss->exec.switch_to_full_mode();
    }
    else
    {
        iss->trace.warning("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
    }
}

int Lsu::data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{
    this->iss.decode_trace.msg("Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);
    vp::io_req *req = &this->iss.io_req;
    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data_ptr);
    int err = this->iss.data.req(req);
    if (err == vp::IO_REQ_OK)
    {
        this->iss.timing.stall_load_account(req->get_latency());
        return 0;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        vp_warning_always(&this->iss.warning, 
            "Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n",
            this->iss.current_insn->addr, addr, size, is_write);
    }

    this->iss.trace.msg(vp::trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
    this->iss.exec.insn_stall();
    return err;
}

int Lsu::data_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{

    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

    if (likely(addr0 == addr1))
        return this->data_req_aligned(addr, data_ptr, size, is_write);
    else
        return this->data_misaligned_req(addr, data_ptr, size, is_write);
}

Lsu::Lsu(Iss &iss)
    : iss(iss)
{
}

void Lsu::store_resume(Lsu *lsu)
{
    // For now we don't have to do anything as the register was written directly
    // by the request but we cold support sign-extended loads here;
}


void Lsu::load_resume(Lsu *lsu)
{
    // Nothing to do, the zero-extension was done by initializing the register to 0
}

void Lsu::elw_resume(Lsu *lsu)
{
    // Clear pending elw to not replay it when the next interrupt occurs
    lsu->iss.state.elw_insn = NULL;
    lsu->iss.elw_stalled.set(false);
}

void Lsu::load_signed_resume(Lsu *lsu)
{
    int reg = lsu->iss.state.stall_reg;
    iss_set_reg(&lsu->iss, reg, iss_get_signed_value(iss_get_reg_untimed(&lsu->iss, reg), lsu->iss.state.stall_size * 8));
}
