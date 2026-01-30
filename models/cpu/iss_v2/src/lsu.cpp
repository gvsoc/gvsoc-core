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

void Lsu::reset(bool active)
{
    if (active)
    {
        this->io_req_denied = false;
    }
}

bool Lsu::data_req_virtual(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array)) return false;

    if (this->data_req(insn, addr, size, is_write, is_signed, reg))
    {
        this->iss.exec.insn_stall();
        return true;
    }

    return false;
}

bool Lsu::lsu_is_empty()
{
    // TO BE IMPLEMENTED
    return false;
}

void Lsu::data_grant(vp::Block *__this, vp::IoReq *req)
{
// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     // The denied request is granted, we can now allow the core to do other accesses
//     Lsu *_this = (Lsu *)__this;
//     _this->io_req_denied = false;
//     _this->iss.exec.stalled_dec();
// #endif
}

void Lsu::data_response(vp::Block *__this, vp::IoReq *req)
{
    Lsu *_this = (Lsu *)__this;
    Iss *iss = &_this->iss;
    LsuReqEntry *entry = (LsuReqEntry *)req;

    _this->trace.msg("Received data response (req: %p)\n", req);

    _this->iss.exec.insn_terminate(entry->insn_entry);

    _this->free_req_entry(entry, _this->iss.clock.get_cycles() + req->get_latency() + 1);
}

bool Lsu::data_req_aligned(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);
    LsuReqEntry *entry = this->get_req_entry();
    if (entry == NULL) return true;

    if (!is_write)
    {
        entry->data = 0;
    }
    else
    {
        entry->data = this->iss.regfile.get_reg(reg);
    }

    vp::IoReq *req = &entry->req;
    req->prepare();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);

    this->log_addr.set_and_release(addr);
    this->log_size.set_and_release(size);
    this->log_is_write.set_and_release(is_write);

    int err = this->data.req(req);

    if (err == vp::IO_REQ_OK)
    {
        if (!is_write)
        {
            uint64_t data = entry->data;

            if (is_signed)
            {
                data = iss_get_signed_value(data, size * 8);
            }

            this->iss.regfile.set_reg(reg, data);
        }

        int64_t latency = req->get_latency();

        if (latency > 0)
        {
            entry->timestamp = this->iss.clock.get_cycles() + latency;
            this->iss.exec.enqueue_task(&entry->task);
            entry->insn_entry = this->iss.exec.insn_hold(insn);
        }
        else
        {
            this->free_req_entry(entry, this->iss.clock.get_cycles() + latency);
        }

        return false;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        // TODO exception
//         latency = req->get_latency() + 1;

// #ifndef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
//         if (this->iss.gdbserver.gdbserver)
//         {
//             this->trace.msg(vp::Trace::LEVEL_WARNING,"Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n",
//                             this->iss.exec.current_insn, addr, size, is_write);
//             this->iss.exec.stalled_inc();
//             this->iss.exec.halted.set(true);
//             this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver, vp::Gdbserver_engine::SIGNAL_BUS);
//         }
//         else
//         {
//             vp_warning_always(&this->trace,
//                             "Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n",
//                             this->iss.exec.current_insn, addr, size, is_write);
//         }
// #else
//         int trap = is_write ? ISS_EXCEPT_STORE_FAULT : ISS_EXCEPT_LOAD_FAULT;
//         this->iss.exception.raise(this->iss.exec.current_insn, trap);
// #endif
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

bool Lsu::data_req(iss_insn_t *insn, iss_addr_t addr, int size, bool is_write, bool is_signed, int reg)
{
// #if !defined(CONFIG_GVSOC_ISS_HANDLE_MISALIGNED)

    return this->data_req_aligned(insn, addr, size, is_write, is_signed, reg);

// #else

//     iss_addr_t addr0 = addr & ADDR_MASK;
//     iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

//     if (likely(addr0 == addr1))
//         return this->data_req_aligned(addr, data_ptr, memcheck_data, size, is_write, latency, req_id);
//     else
//         return this->data_misaligned_req(addr, data_ptr, memcheck_data, size, is_write, latency);

// #endif
//
    return 0;
}

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
    this->iss.new_master_port("meminfo", &this->meminfo, (vp::Block *)this);

    this->iss.new_reg("elw_stalled", &this->elw_stalled, false);

    this->memory_start = -1;
    this->memory_end = -1;

    if (this->iss.get_js_config()->get("memory_start") != NULL)
    {
        this->memory_start = this->iss.get_js_config()->get("memory_start")->get_int();
        this->memory_end = this->memory_start +
            this->iss.get_js_config()->get("memory_size")->get_int();
    }

    this->req_entry_first = NULL;
    for (int i=0; i<CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING; i++)
    {
        LsuReqEntry *req = &this->req_entry[i];
        req->next = this->req_entry_first;
        req->req.set_data((uint8_t *)&req->data);
        this->req_entry_first = req;
        req->task.callback = &Lsu::handle_io_req_end;
    }
}

void Lsu::start()
{
}

void Lsu::store_resume(Lsu *lsu, vp::IoReq *req)
{
//     // For now we don't have to do anything as the register was written directly
//     // by the request but we cold support sign-extended loads here;
// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     int req_id = *((int *)req->arg_get(0));
//     lsu->iss.exec.insn_terminate(false, lsu->stall_insn[req_id]);
// #else
//     lsu->iss.exec.insn_terminate();
// #endif
}

void Lsu::load_resume(Lsu *lsu, vp::IoReq *req)
{
//     // Nothing to do, the zero-extension was done by initializing the register to 0

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     int req_id = *((int *)req->arg_get(0));
//     int reg = lsu->stall_reg[req_id];
// #else
//     int reg = lsu->stall_reg;
// #endif
// #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
//     lsu->iss.regfile.scoreboard_reg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
// #endif
// #if defined(PIPELINE_STALL_THRESHOLD)
//         if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
//         {
//             lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
//         }
// #endif

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     lsu->iss.exec.insn_terminate(false, lsu->stall_insn[req_id]);
// #else
//     lsu->iss.exec.insn_terminate();
// #endif
}

void Lsu::elw_resume(Lsu *lsu, vp::IoReq *req)
{
    // // Clear pending elw to not replay it when the next interrupt occurs
    // lsu->iss.exec.insn_terminate();
    // lsu->iss.exec.elw_insn = 0;
    // lsu->elw_stalled.set(false);
    // lsu->iss.exec.busy_enter();
}

void Lsu::load_signed_resume(Lsu *lsu, vp::IoReq *req)
{
// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     int req_id = *((int *)req->arg_get(0));
//     int reg = lsu->stall_reg[req_id];
//     int stall_size = lsu->stall_size[req_id];
// #else
//     int reg = lsu->stall_reg;
//     int stall_size = lsu->stall_size;
// #endif
//     lsu->iss.regfile.set_reg(reg, iss_get_signed_value(lsu->iss.regfile.get_reg(reg),
//         stall_size * 8));

//     // Due to sign extension, whole register is valid only if sign is valid
//     lsu->iss.regfile.memcheck_set_valid(reg, (lsu->iss.regfile.memcheck_get(reg) >> (stall_size *8)) & 1);

// #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
//     lsu->iss.regfile.scoreboard_reg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
// #endif
// #if defined(PIPELINE_STALL_THRESHOLD)
//         if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
//         {
//             lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
//         }
// #endif

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     lsu->iss.exec.insn_terminate(false, lsu->stall_insn[req_id]);
// #else
//     lsu->iss.exec.insn_terminate();
// #endif
}

void Lsu::load_float_resume(Lsu *lsu, vp::IoReq *req)
{
// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     int req_id = *((int *)req->arg_get(0));
//     int reg = lsu->stall_reg[req_id];
//     int stall_size = lsu->stall_size[req_id];
// #else
//     int reg = lsu->stall_reg;
//     int stall_size = lsu->stall_size;
// #endif
//     lsu->iss.regfile.set_freg(reg, iss_get_float_value(lsu->iss.regfile.get_freg(reg),
//         stall_size * 8));
// #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
//     lsu->iss.regfile.scoreboard_freg_set_timestamp(reg, lsu->pending_latency + 1, CSR_PCER_LD_STALL);
// #endif
// #if defined(PIPELINE_STALL_THRESHOLD)
//         if (lsu->pending_latency > PIPELINE_STALL_THRESHOLD)
//         {
//             lsu->iss.timing.stall_load_account(lsu->pending_latency - PIPELINE_STALL_THRESHOLD);
//         }
// #endif

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     lsu->iss.exec.insn_terminate(false, lsu->stall_insn[req_id]);
// #else
//     lsu->iss.exec.insn_terminate();
// #endif
}

bool Lsu::atomic(iss_insn_t *insn, iss_addr_t addr, int size, int reg_in, int reg_out,
    vp::IoReqOpcode opcode)
{
//     iss_addr_t phys_addr;

//     this->trace.msg("Atomic request (addr: 0x%lx, size: 0x%x, opcode: %d)\n", addr, size, opcode);
//     vp::IoReq *req = this->get_req();
//     if (req == NULL)
//     {
//         // If there is no more request, returns true to stall the core stall the core
//         return true;
//     }

//     if (opcode == vp::IoReqOpcode::LR)
//     {
//         bool use_mem_array;
// #ifdef CONFIG_GVSOC_ISS_MMU
//         if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
//         {
//             return false;
//         }
// #else
//         phys_addr = addr;
// #endif
//     }
//     else
//     {
//         bool use_mem_array;
// #ifdef CONFIG_GVSOC_ISS_MMU
//         if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
//         {
//             return false;
//         }
// #else
//         phys_addr = addr;
// #endif
//     }

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     req->prepare();
// #else
//     // It seems some models do not correctly pop arguments. They should be fixed before switching to prepare
//     req->init();
// #endif
//     req->set_addr(phys_addr);
//     req->set_size(size);
//     req->set_opcode(opcode);
// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     // Since accesses are not blocking and the store may use input data later, we need
//     // to save it to the request
//     uint8_t *req_data = (uint8_t *)&this->req_data[*((int *)req->arg_get(0))];
//     *(iss_reg_t *)req_data = *(iss_reg_t *)this->iss.regfile.reg_store_ref(reg_in);
//     req->set_data(req_data);
// #else
//     req->set_data((uint8_t *)this->iss.regfile.reg_store_ref(reg_in));
// #endif
//     req->set_second_data((uint8_t *)this->iss.regfile.reg_ref(reg_out));

// // #ifdef VP_MEMCHECK_ACTIVE
// //     uint8_t *memcheck_data = (uint8_t *)this->iss.regfile.reg_store_ref(reg_in);
// //     memset(memcheck_data, 0xFF, size);
// //     req->set_memcheck_data(memcheck_data);
// //     uint8_t *check_second_data = (uint8_t *)this->iss.regfile.reg_ref(reg_out);
// //     req->set_second_memcheck_data(check_second_data);
// // #endif
//     req->set_initiator(this->iss.csr.mhartid);

//     this->log_addr.set_and_release(addr);
//     this->log_size.set_and_release(size);
//     this->log_is_write.set_and_release(true);

//     int err = this->data.req(req);
//     if (err == vp::IO_REQ_OK)
//     {
//         if (size != ISS_REG_WIDTH/8)
//         {
//             this->iss.regfile.set_reg(reg_out, iss_get_signed_value(this->iss.regfile.get_reg(reg_out), size * 8));
//         }

// #ifndef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//         if (req->get_latency() > 0)
//         {
// #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
//             this->iss.regfile.scoreboard_reg_set_timestamp(reg_out, req->get_latency() + 1, CSR_PCER_LD_STALL);
// #else
//             this->iss.timing.stall_load_account(req->get_latency());
// #endif
//         }
// #else
//         // For synchronous requests, free the request now with the proper latency, so that
//         // it becomes available only after the latency has ellapsed
//         this->free_req(req, this->iss.clock.get_cycles() + req->get_latency());
// #endif

//         return false;
//     }
//     else if (err == vp::IO_REQ_INVALID)
//     {
//         vp_warning_always(&this->iss.trace,
//                           "Invalid atomic access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, opcode: %d)\n",
//                           this->iss.exec.current_insn, phys_addr, size, opcode);
//         return false;
//     }

//     this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for asynchronous response\n");

// #ifdef CONFIG_GVSOC_ISS_LSU_NB_OUTSTANDING
//     if (err == vp::IO_REQ_DENIED || err == vp::IO_REQ_PENDING)
//     {
// #ifdef CONFIG_GVSOC_ISS_SCOREBOARD
//         this->iss.regfile.scoreboard_reg_invalidate(reg_out);
// #endif
//         if (err == vp::IO_REQ_DENIED)
//         {
//             // In case the request is denied, make sure we don't allow any other access
//             // until this request is granted
//             this->io_req_denied = true;
//             this->iss.exec.insn_stall();
//         }
//         // What was the purpose of this check ?
//         if (1) //size != ISS_REG_WIDTH/8)
//         {
//             int req_id = *((int *)req->arg_get(0));
//             this->stall_callback[req_id] = &Lsu::load_signed_resume;
//             this->stall_reg[req_id] = reg_out;
//             this->stall_size[req_id] = size;
//             this->stall_insn[req_id] = insn->addr;
//         }
//         else
//         {
//             int req_id = *((int *)req->arg_get(0));
//             this->stall_callback[req_id] = &Lsu::store_resume;
//             this->stall_insn[req_id] = insn->addr;
//         }
//     }
// #else
//     this->iss.exec.insn_stall();

//     if (size != ISS_REG_WIDTH/8)
//     {
//         this->stall_callback = &Lsu::load_signed_resume;
//         this->stall_reg = reg_out;
//         this->stall_size = size;
//     }
//     else
//     {
//         this->stall_callback = &Lsu::store_resume;
//     }
// #endif

    return false;
}

void Lsu::handle_io_req_end(Iss *iss, Task *task)
{
    LsuReqEntry *entry = (LsuReqEntry *)((char *)task - ((char *)&((LsuReqEntry *)0)->task - (char *)0));

    if (iss->clock.get_cycles() >= entry->timestamp)
    {
        iss->exec.insn_terminate(entry->insn_entry);
        iss->lsu.free_req_entry(entry, 0);
    }
    else
    {
        iss->exec.enqueue_task(task);
    }
}
