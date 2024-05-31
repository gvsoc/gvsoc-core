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
#include "cpu/iss/include/iss.hpp"

void Lsu::reset(bool active)
{
    if (active)
    {
        this->elw_stalled.set(false);
        this->misaligned_size = 0;
    }
}

void Lsu::exec_misaligned(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *iss = (Iss *)__this;
    Lsu *_this = &iss->lsu;

    if (iss->exec.handle_stall_cycles()) return;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling second half of misaligned access\n");

    // As the 2 load accesses for misaligned access are generated by the
    // wrapper, we need to account the extra access here.
    // iss->timing.stall_misaligned_account();

    iss->timing.event_load_account(1);
    iss->timing.cycle_account();

    int64_t latency;
    if (_this->data_req_aligned(_this->misaligned_addr, _this->misaligned_data, _this->misaligned_memcheck_data,
                                _this->misaligned_size, _this->misaligned_is_write, latency) == vp::IO_REQ_OK)
    {
        iss->trace.dump_trace_enabled = true;
        _this->pending_latency = latency;
        _this->stall_callback(_this);
        iss->exec.insn_resume();
    }
    else
    {
        _this->trace.warning("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
    }

    _this->misaligned_size = 0;
}


int Lsu::data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
{

    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

    this->trace.msg("Misaligned data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

    this->iss.timing.event_misaligned_account(1);

    // The access is a misaligned access
    // Change the event so that we can do the first access now and the next access
    // during the next cycle
    int size0 = addr1 - addr;
    int size1 = size - size0;

    // Remember the access properties for the second access
    this->misaligned_size = size1;
    this->misaligned_data = data_ptr + size0;
    this->misaligned_memcheck_data = memcheck_data + size0;
    this->misaligned_addr = addr1;
    this->misaligned_is_write = is_write;

    // And do the first one now
    int err = data_req_aligned(addr, data_ptr, memcheck_data, size0, is_write, latency);
    if (err == vp::IO_REQ_OK)
    {
#if defined(PIPELINE_STALL_THRESHOLD)
        if (latency > PIPELINE_STALL_THRESHOLD)
        {
            this->iss.timing.stall_load_account(latency - PIPELINE_STALL_THRESHOLD);
        }
#endif
        // As the transaction is split into 2 parts, we must tell the ISS
        // that the access is pending as the instruction must be executed
        // only when the second access is finished.
        this->iss.exec.insn_hold(&Lsu::exec_misaligned);
        return vp::IO_REQ_PENDING;
    }
    else
    {
        this->trace.force_warning("UNIMPLEMENTED AT %s %d, error during misaligned access\n", __FILE__, __LINE__);
        return vp::IO_REQ_INVALID;
    }
}

void Lsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
}

void Lsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    Lsu *_this = (Lsu *)__this;
    Iss *iss = &_this->iss;

    iss->exec.stalled_dec();

    _this->trace.msg("Received data response (stalled: %d)\n", iss->exec.stalled.get());

    // First call the ISS to finish the instruction
    _this->pending_latency = req->get_latency() + 1;

    // Call the access termination callback only we the access is not misaligned since
    // in this case, the second access with handle it.
    if (_this->misaligned_size == 0)
    {
        _this->stall_callback(_this);
    }
}

int Lsu::data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);
    vp::IoReq *req = &this->io_req;
    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data_ptr);
#ifdef VP_MEMCHECK_ACTIVE
    if (!is_write && memcheck_data)
    {
        memset(memcheck_data, 0xFF, size);
    }
    req->set_memcheck_data(memcheck_data);
#endif
    int err = this->data.req(req);
    if (err == vp::IO_REQ_OK)
    {
        #ifndef CONFIG_GVSOC_ISS_SNITCH
        latency = req->get_latency() + 1;
        #else
        // In case of a write, don't signal a valid transaction. Stores are always
        // without ans answer to the core.
        if (is_write)
        {
            // Suppress stores
            latency = req->get_latency(); 
        }
        else
        {
            // Load needs one more cycle to write result back from tcdm/mem response.
            latency = req->get_latency() + 1;
        }
        #endif
        return 0;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        latency = this->io_req.get_latency() + 1;

#ifndef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
        if (this->iss.gdbserver.gdbserver)
        {
            this->trace.msg(vp::Trace::LEVEL_WARNING,"Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n",
                            this->iss.exec.current_insn, addr, size, is_write);
            this->iss.exec.stalled_inc();
            this->iss.exec.halted.set(true);
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, vp::Gdbserver_engine::SIGNAL_BUS);
        }
        else
        {
            vp_warning_always(&this->trace,
                            "Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n",
                            this->iss.exec.current_insn, addr, size, is_write);
        }
#else
        int trap = is_write ? ISS_EXCEPT_STORE_FAULT : ISS_EXCEPT_LOAD_FAULT;
        this->iss.exception.raise(this->iss.exec.current_insn, trap);
#endif
        return err;
    }

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
    this->iss.exec.insn_stall();
    return err;
}

int Lsu::data_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
{
#if !defined(CONFIG_GVSOC_ISS_HANDLE_MISALIGNED)

    return this->data_req_aligned(addr, data_ptr, memcheck_data, size, is_write, latency);

#else

    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

#ifdef CONFIG_GVSOC_ISS_SNITCH
    // Todo: solve misaligned data request issue of fp subsystem by enqueue acceleration request
    if (likely(addr0 == addr1) || this->iss.fp_ss)
        return this->data_req_aligned(addr, data_ptr, size, is_write, latency);
    else
        return this->data_misaligned_req(addr, data_ptr, size, is_write, latency);
#endif
#ifndef CONFIG_GVSOC_ISS_SNITCH
    if (likely(addr0 == addr1))
        return this->data_req_aligned(addr, data_ptr, memcheck_data, size, is_write, latency);
    else
        return this->data_misaligned_req(addr, data_ptr, memcheck_data, size, is_write, latency);

#endif
#endif
}

Lsu::Lsu(Iss &iss)
    : iss(iss)
{
}

void Lsu::build()
{
    iss.top.traces.new_trace("lsu", &this->trace, vp::DEBUG);
    data.set_resp_meth(&Lsu::data_response);
    data.set_grant_meth(&Lsu::data_grant);
    this->iss.top.new_master_port("data", &data, (vp::Block *)this);
    this->iss.top.new_master_port("meminfo", &this->meminfo, (vp::Block *)this);

    this->iss.top.new_reg("elw_stalled", &this->elw_stalled, false);

    this->memory_start = -1;
    this->memory_end = -1;

    if (this->iss.top.get_js_config()->get("memory_start") != NULL)
    {
        this->memory_start = this->iss.top.get_js_config()->get("memory_start")->get_int();
        this->memory_end = this->memory_start +
            this->iss.top.get_js_config()->get("memory_size")->get_int();
    }
}

void Lsu::start()
{
#ifdef CONFIG_GVSOC_ISS_MEMORY
    this->meminfo.sync_back((void **)&this->mem_array);
#endif
}

void Lsu::store_resume(Lsu *lsu)
{
    // For now we don't have to do anything as the register was written directly
    // by the request but we cold support sign-extended loads here;
    lsu->iss.exec.insn_terminate();
}

void Lsu::load_resume(Lsu *lsu)
{
    // Nothing to do, the zero-extension was done by initializing the register to 0

    int reg = lsu->stall_reg;
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    lsu->iss.regfile.scoreboard_reg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
#endif
#if defined(PIPELINE_STALL_THRESHOLD)
        if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
        {
            lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
        }
#endif

    lsu->iss.exec.insn_terminate();
}

void Lsu::elw_resume(Lsu *lsu)
{
    // Clear pending elw to not replay it when the next interrupt occurs
    lsu->iss.exec.insn_terminate();
    lsu->iss.exec.elw_insn = 0;
    lsu->elw_stalled.set(false);
    lsu->iss.exec.busy_enter();
}

void Lsu::load_signed_resume(Lsu *lsu)
{
    int reg = lsu->stall_reg;
    lsu->iss.regfile.set_reg(reg, iss_get_signed_value(lsu->iss.regfile.get_reg(reg),
        lsu->stall_size * 8));

    // Due to sign extension, whole register is valid only if sign is valid
    lsu->iss.regfile.memcheck_set_valid(reg, (lsu->iss.regfile.memcheck_get(reg) >> (lsu->stall_size *8)) & 1);

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    lsu->iss.regfile.scoreboard_reg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
#endif
#if defined(PIPELINE_STALL_THRESHOLD)
        if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
        {
            lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
        }
#endif
    lsu->iss.exec.insn_terminate();
}

void Lsu::load_float_resume(Lsu *lsu)
{
    int reg = lsu->stall_reg;
    lsu->iss.regfile.set_freg(reg, iss_get_float_value(lsu->iss.regfile.get_freg(reg),
        lsu->stall_size * 8));
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    lsu->iss.regfile.scoreboard_freg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
#endif
#if defined(PIPELINE_STALL_THRESHOLD)
        if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
        {
            lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
        }
#endif
    lsu->iss.exec.insn_terminate();
}

void Lsu::atomic(iss_insn_t *insn, iss_addr_t addr, int size, int reg_in, int reg_out,
    vp::IoReqOpcode opcode)
{
        iss_addr_t phys_addr;

    this->trace.msg("Atomic request (addr: 0x%lx, size: 0x%x, opcode: %d)\n", addr, size, opcode);
    vp::IoReq *req = &this->io_req;

    if (opcode == vp::IoReqOpcode::LR)
    {
        bool use_mem_array;
        if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
        {
            return;
        }
    }
    else
    {
        bool use_mem_array;
        if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
        {
            return;
        }
    }

    req->init();
    req->set_addr(phys_addr);
    req->set_size(size);
    req->set_opcode(opcode);
    req->set_data((uint8_t *)this->iss.regfile.reg_store_ref(reg_in));
    req->set_second_data((uint8_t *)this->iss.regfile.reg_ref(reg_out));
// #ifdef VP_MEMCHECK_ACTIVE
//     uint8_t *memcheck_data = (uint8_t *)this->iss.regfile.reg_store_ref(reg_in);
//     memset(memcheck_data, 0xFF, size);
//     req->set_memcheck_data(memcheck_data);
//     uint8_t *check_second_data = (uint8_t *)this->iss.regfile.reg_ref(reg_out);
//     req->set_second_memcheck_data(check_second_data);
// #endif
    req->set_initiator(this->iss.csr.mhartid);
    int err = this->data.req(req);
    if (err == vp::IO_REQ_OK)
    {
        if (size != ISS_REG_WIDTH/8)
        {
            this->iss.regfile.set_reg(reg_out, iss_get_signed_value(this->iss.regfile.get_reg(reg_out), size * 8));
        }

        if (this->io_req.get_latency() > 0)
        {
            this->iss.timing.stall_load_account(req->get_latency());
        }
        return;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        vp_warning_always(&this->iss.trace,
                          "Invalid atomic access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, opcode: %d)\n",
                          this->iss.exec.current_insn, phys_addr, size, opcode);
        return;
    }

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
    this->iss.exec.insn_stall();

    if (size != ISS_REG_WIDTH/8)
    {
        this->stall_callback = &Lsu::load_signed_resume;
        this->stall_reg = reg_out;
        this->stall_size = size;
    }
    else
    {
        this->stall_callback = &Lsu::store_resume;
    }
}