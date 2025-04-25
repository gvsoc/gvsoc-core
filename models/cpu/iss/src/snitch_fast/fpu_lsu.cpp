/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */

 #include "cpu/iss/include/iss.hpp"
 #include ISS_CORE_INC(class.hpp)

FpuLsu::FpuLsu(IssWrapper &top, Iss &iss)
: vp::Block(&top, "fpu_lsu"), iss(iss)
{
    iss.top.traces.new_trace("lsu", &this->trace, vp::DEBUG);
    // data.set_resp_meth(&Lsu::data_response);
    // data.set_grant_meth(&Lsu::data_grant);
    // this->iss.top.new_master_port("data", &data, (vp::Block *)this);
    // this->iss.top.new_master_port("meminfo", &this->meminfo, (vp::Block *)this);

    // this->iss.top.new_reg("elw_stalled", &this->elw_stalled, false);

    // this->memory_start = -1;
    // this->memory_end = -1;

    // if (this->iss.top.get_js_config()->get("memory_start") != NULL)
    // {
    //     this->memory_start = this->iss.top.get_js_config()->get("memory_start")->get_int();
    //     this->memory_end = this->memory_start +
    //         this->iss.top.get_js_config()->get("memory_size")->get_int();
    // }

}

// void Lsu::start()
// {
// #ifdef CONFIG_GVSOC_ISS_MEMORY
//     this->meminfo.sync_back((void **)&this->mem_array);
// #endif
// }

template<typename T>
bool FpuLsu::load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        this->iss.regfile.set_freg(reg, iss_get_float_value(*(T *)&this->mem_array[phys_addr - this->memory_start], size * 8));

        return false;
    }
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.freg_ref(reg), NULL, size, false, latency)) == 0)
    {
#ifdef CONFIG_GVSOC_ISS_SNITCH_FAST
        this->iss.regfile.set_freg(reg, iss_get_float_value(this->iss.regfile.get_freg_untimed(reg), size * 8));
#else
        this->iss.regfile.set_freg(reg, iss_get_float_value(this->iss.regfile.get_freg(reg), size * 8));
#endif
#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
        this->iss.regfile.scoreboard_freg_set_timestamp(reg, latency + 1, CSR_PCER_LD_STALL);
#endif

#if defined(PIPELINE_STALL_THRESHOLD)
        if (latency > PIPELINE_STALL_THRESHOLD)
        {
            this->iss.timing.stall_load_account(latency - PIPELINE_STALL_THRESHOLD);
        }
#endif

    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &FpuLsu::load_float_resume;
            this->stall_reg = reg;
            this->stall_size = size;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
bool FpuLsu::store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

#ifdef CONFIG_GVSOC_ISS_MEMORY

    if (use_mem_array)
    {
        *(T *)&this->mem_array[phys_addr - this->memory_start] = this->iss.regfile.get_freg(reg);

        return false;
    }
#endif

#ifdef CONFIG_GVSOC_ISS_SCOREBOARD
    // Since the input register is passed through its index and accessed through a pointer,
    // we need to take care of the scoreboard
    this->iss.regfile.scoreboard_freg_check(reg);
#endif

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)this->iss.regfile.freg_store_ref(reg), NULL, size, true, latency)) == 0)
    {
        // For now we don't have to do anything as the register was written directly
        // by the request but we cold support sign-extended loads here;

#if defined(PIPELINE_STALL_THRESHOLD)
        if (latency > PIPELINE_STALL_THRESHOLD)
        {
            this->iss.timing.stall_load_account(latency - PIPELINE_STALL_THRESHOLD);
        }
#endif

    }
    else
    {
        if (err != vp::IO_REQ_INVALID)
        {
            this->stall_callback = &FpuLsu::store_resume;
            this->stall_reg = reg;
        }
        else
        {
            // Return true so that pc is not updated and gdb can see the faulting instruction.
            // This also prevent the core from continuing to the next instruction.
            if (this->iss.gdbserver.gdbserver)
            {
                return true;
            }
        }
    }

    return false;
}

template<typename T>
bool FpuLsu::load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->load_float<T>(insn, addr, size, reg);
}

template<typename T>
bool FpuLsu::store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    {
        return true;
    }
    this->iss.timing.event_store_account(1);
    return this->store_float<T>(insn, addr, size, reg);
}

template bool FpuLsu::load_float<uint8_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float_perf<uint8_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float<uint16_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float_perf<uint16_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float<uint32_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float_perf<uint32_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float<uint64_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::load_float_perf<uint64_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float<uint8_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float_perf<uint8_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float<uint16_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float_perf<uint16_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float<uint32_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float_perf<uint32_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float<uint64_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
template bool FpuLsu::store_float_perf<uint64_t>(iss_insn_t *insn, iss_addr_t addr, int size, int reg);


void FpuLsu::store_resume(FpuLsu *lsu)
{
    // For now we don't have to do anything as the register was written directly
    // by the request but we cold support sign-extended loads here;
    lsu->iss.exec.insn_terminate();
}

void FpuLsu::load_float_resume(FpuLsu *lsu)
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


int FpuLsu::data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
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
    int err = this->iss.lsu.data.req(req);

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
    // TODO stall FPU SS instead of core
    this->trace.fatal("Unimplemented asynchronous response\n");
    // this->iss.exec.insn_stall();
    return err;
}

int FpuLsu::data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
{
    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

    this->trace.msg("Misaligned data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);

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
        // As the transaction is split into 2 parts, we must tell the ISS
        // that the access is pending as the instruction must be executed
        // only when the second access is finished.
        // this->iss.exec.insn_hold(&Lsu::exec_misaligned);
        return vp::IO_REQ_PENDING;
    }
    else
    {
        return vp::IO_REQ_INVALID;
    }
}

int FpuLsu::data_req(iss_addr_t addr, uint8_t *data_ptr, uint8_t *memcheck_data, int size, bool is_write, int64_t &latency)
{
#if !defined(CONFIG_GVSOC_ISS_HANDLE_MISALIGNED)

    return this->data_req_aligned(addr, data_ptr, memcheck_data, size, is_write, latency);

#else

    iss_addr_t addr0 = addr & ADDR_MASK;
    iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

    if (likely(addr0 == addr1))
        return this->data_req_aligned(addr, data_ptr, memcheck_data, size, is_write, latency);
    else
        return this->data_misaligned_req(addr, data_ptr, memcheck_data, size, is_write, latency);

#endif
}
