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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */


#include "cpu/iss/include/ssr.hpp"
#include "cpu/iss/include/iss.hpp"
#include ISS_CORE_INC(class.hpp)


// Get called when we need to update credit counter after a preloading dm_event.
void CreditCnt::update_cnt()
{
    // bool increment;
    // bool decrement;

    if (this->credit_give & !this->credit_take)
    {
        this->credit_cnt++;
    }
    else if (!this->credit_give & this->credit_take)
    {
        this->credit_cnt--;
    }

    // Determine wether there's remaining data can be and need to be preloaded.
    if (this->credit_cnt > 0)
    {
        this->has_credit = 1;
    }
    else
    {
        this->has_credit = 0;
    }
}


// Get called when we calculate the memory access address for the next time.
iss_reg_t DataMover::addr_gen_unit(bool is_write)
{
    int ssr_dim = this->config.DIM;
    iss_reg_t mem_addr = 0x0;
    iss_reg_t mem_inc = 0x0;
    int i = 0;

    // Address update is based on previous address and the value of increment.
    // temp_addr: memory address of previous access
    // inc_addr: the increase of address in the next access, obtained by the configurations and iterations.
    if (is_write)
    {
        // mem_addr = this->config.REG_WPTR[ssr_dim] + mem_inc;
        mem_addr = this->temp_addr + this->inc_addr;
    }
    else
    {
        // mem_addr = this->config.REG_RPTR[ssr_dim] + mem_inc;
        mem_addr = this->temp_addr + this->inc_addr;
    }

    this->temp_addr = mem_addr;

    return mem_addr;

}


// Get called when the loop counter is updated after a memory access finishes.
void DataMover::update_cnt()
{
    int ssr_dim = this->config.DIM;

    // Update loop counters
    // First increment repetition number.
    this->counter.rep++;
    if (this->counter.rep > this->config.REG_REPEAT)
    {
        this->counter.bound[0]++;
        this->counter.rep = 0;
        this->rep_done = true;
        this->inc_addr = this->config.REG_STRIDES[0];
    }
    else
    {
        this->rep_done = false;
        this->inc_addr = 0;
        return;
    }

    // Then increment the bound number if one round of repetition finishes.
    for (int i=0; i<ssr_dim; i++)
    {
        if (this->counter.bound[i] > this->config.REG_BOUNDS[i])
        {
            this->counter.bound[i] = 0;
            this->counter.bound[i+1]++;
            this->inc_addr = this->config.REG_STRIDES[i+1];
        }
    }
    // Finally update the bound count of the highest dimension.
    // When all iterations finish, set ssr_done to 1, which means this SSR computation ends completely.
    if (this->counter.bound[ssr_dim] > this->config.REG_BOUNDS[ssr_dim])
    {
        this->counter.bound[ssr_dim] = 0;
        this->ssr_done = 1;
        // this->counter.rep++;
    }
}


Ssr::Ssr(Iss &iss)
: iss(iss)
{
}


void Ssr::reset(bool active)
{
    if (active)
    {
        this->ssr_enable = false;
        // Todo: Clear all data mover fifos and configurations
    }
}


void Ssr::build()
{
    this->ssr_enable = false;

    // Todo: Initialize all data mover configurations

    iss.top.traces.new_trace("ssr", &this->trace, vp::DEBUG);
    this->event = this->iss.top.event_new((vp::Block *)this, Ssr::dm_event);

    this->iss.top.new_master_port("ssr_dm0", &ssr_dm0, (vp::Block *)this);
    this->iss.top.new_master_port("ssr_dm1", &ssr_dm1, (vp::Block *)this);
    this->iss.top.new_master_port("ssr_dm2", &ssr_dm2, (vp::Block *)this);
}


// Enable the SSR
void Ssr::enable()
{
    if (!this->ssr_enable)
    {
        this->trace.msg("Enable SSR\n");
        this->ssr_enable = true; 
        this->dm0.ssr_done = 0;
        this->dm1.ssr_done = 0;
        this->dm2.ssr_done = 0;
        // (Work under dm_event)
        // this->event->enable();
    }
}


// Disable the SSR
void Ssr::disable()
{
    if (this->ssr_enable)
    {
        this->trace.msg("Disable SSR\n");
        this->ssr_enable = false;
        // this->dm0.temp_addr = 0;
        // this->dm0.inc_addr = 0;
        // this->dm1.temp_addr = 0;
        // this->dm1.inc_addr = 0;
        // this->dm2.temp_addr = 0;
        // this->dm2.inc_addr = 0;
        // (Work under dm_event)
        // this->event->disable();
    }
}


// Get called when set the configuration registers.
void Ssr::cfg_write(iss_insn_t *insn, int reg, int ssr, iss_reg_t value)
{
    // Set data mover 0
    if (ssr == 0)
    {
        this->dm0.is_config = true;

        if (unlikely(reg == 0))
        {
            this->dm0.config.set_REG_STATUS(value);
        }
        else if (unlikely(reg == 1))
        {
            this->dm0.config.set_REG_REPEAT(value);
            this->trace.msg("Set REG_REPEAT with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >=2 & reg < 6)
        {
            this->dm0.config.set_REG_BOUNDS(reg-2, value);
            this->trace.msg("Set REG_BOUNDS with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >=6 & reg < 10)
        {
            this->dm0.config.set_REG_STRIDES(reg-6, value);
            this->trace.msg("Set REG_STRIDES with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 24 & reg < 28)
        {
            this->dm0.is_write = false;
            this->dm0.config.DIM = reg-24;
            this->dm0.config.set_REG_RPTR(reg-24, value);
            this->dm0.temp_addr = value;
            this->dm0.inc_addr = 0;
            this->trace.msg("Set REG_RPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            this->trace.msg("Loop dimension in data mover %d to %d\n", ssr, this->dm0.config.DIM+1);
            // Todo: delete this
            this->dm0.config.set_REG_WPTR(reg-24, value);
        }
        else if (reg >= 28 & reg < 32)
        {
            this->dm0.is_write = true;
            this->dm0.config.DIM = reg-28;
            this->dm0.config.set_REG_WPTR(reg-28, value);
            this->dm0.temp_addr = value;
            this->dm0.inc_addr = 0;
            this->trace.msg("Set REG_WPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            // Todo: delete this
            this->dm0.config.set_REG_RPTR(reg-28, value);
        }
        else
        {
            // Invalid configuration
            this->dm0.is_config = false;
            this->trace.msg("Invalid configuration register\n");
        }
    }
    // Set data mover 1
    else if (ssr == 1)
    {
        this->dm1.is_config = true;

        if (unlikely(reg == 0))
        {
            this->dm1.config.set_REG_STATUS(value);
        }
        else if (unlikely(reg == 1))
        {
            this->dm1.config.set_REG_REPEAT(value);
            this->trace.msg("Set REG_REPEAT with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 2 & reg < 6)
        {
            this->dm1.config.set_REG_BOUNDS(reg-2, value);
            this->trace.msg("Set REG_BOUNDS with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 6 & reg < 10)
        {
            this->dm1.config.set_REG_STRIDES(reg-6, value);
            this->trace.msg("Set REG_STRIDES with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 24 & reg < 28)
        {
            this->dm1.is_write = false;
            this->dm1.config.DIM = reg-24;
            this->dm1.config.set_REG_RPTR(reg-24, value);
            this->dm1.temp_addr = value;
            this->dm1.inc_addr = 0;
            this->trace.msg("Set REG_RPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            this->trace.msg("Loop dimension in data mover %d to %d\n", ssr, this->dm1.config.DIM+1);
            // Todo: delete this
            this->dm1.config.set_REG_WPTR(reg-24, value);
        }
        else if (reg >=28 & reg < 32)
        {
            this->dm1.is_write = true;
            this->dm1.config.DIM = reg-28;
            this->dm1.config.set_REG_WPTR(reg-28, value);
            this->dm1.temp_addr = value;
            this->dm1.inc_addr = 0;
            this->trace.msg("Set REG_WPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            // Todo: delete this
            this->dm1.config.set_REG_RPTR(reg-28, value);
        }
        else
        {
            // Invalid configuration
            this->dm1.is_config = false;
            this->trace.msg("Invalid configuration register\n");
        }
    }
    // Set data mover 2
    else if (ssr == 2)
    {
        this->dm2.is_config = true;

        if (unlikely(reg == 0))
        {
            this->dm2.config.set_REG_STATUS(value);
        }
        else if (unlikely(reg == 1))
        {
            this->dm2.config.set_REG_REPEAT(value);
            this->trace.msg("Set REG_REPEAT with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 2 & reg < 6)
        {
            this->dm2.config.set_REG_BOUNDS(reg-2, value);
            this->trace.msg("Set REG_BOUNDS with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 6 & reg < 10)
        {
            this->dm2.config.set_REG_STRIDES(reg-6, value);
            this->trace.msg("Set REG_STRIDES with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
        }
        else if (reg >= 24 & reg < 28)
        {
            this->dm2.is_write = false;
            this->dm2.config.DIM = reg-24;
            this->dm2.config.set_REG_RPTR(reg-24, value);
            this->dm2.temp_addr = value;
            this->dm2.inc_addr = 0;
            this->trace.msg("Set REG_RPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            // Todo: delete this
            this->dm2.config.set_REG_WPTR(reg-24, value);
        }
        else if (reg >= 28 & reg < 32)
        {
            this->dm2.is_write = true;
            this->dm2.config.DIM = reg-28;
            this->dm2.config.set_REG_WPTR(reg-28, value);
            this->dm2.temp_addr = value;
            this->dm2.inc_addr = 0;
            this->trace.msg("Set REG_WPTR with index %d in data mover %d to 0x%llx\n", reg, ssr, value);
            // Todo: delete this
            this->dm2.config.set_REG_RPTR(reg-28, value);
        }
        else
        {
            // Invalid configuration
            this->dm2.is_config = false;
            this->trace.msg("Invalid configuration register\n");
        }
    }
    else
    {
        // Invalid data mover index
        this->trace.msg("Invalid configuration data mover\n");
    }

}


// Get called when get the value of configuration registers.
iss_reg_t Ssr::cfg_read(iss_insn_t *insn, int reg, int ssr)
{
    iss_reg_t value;

    // Get from data mover 0
    if (ssr == 0)
    {
        if (unlikely(reg == 0))
        {
            value = this->dm0.config.get_REG_STATUS();
        }
        else if (unlikely(reg == 1))
        {
            value = this->dm0.config.get_REG_REPEAT();
        }
        else if (reg >= 2 & reg < 6)
        {
            value = this->dm0.config.get_REG_BOUNDS(reg-2);
        }
        else if (reg >= 6 & reg < 10)
        {
            value = this->dm0.config.get_REG_STRIDES(reg-6);
        }
        else if (reg >= 24 & reg < 28)
        {
            value = this->dm0.config.get_REG_RPTR(reg-24);
        }
        else if (reg >= 28 & reg < 32)
        {
            value = this->dm0.config.get_REG_WPTR(reg-28);
        }
        else
        {
            // Invalid configuration
            this->trace.msg("Invalid configuration register\n");
            value = 0;
        }
    }
    // Get from data mover 1
    else if (ssr == 1)
    {
        if (unlikely(reg == 0))
        {
            value = this->dm1.config.get_REG_STATUS();
        }
        else if (unlikely(reg == 1))
        {
            value = this->dm1.config.get_REG_REPEAT();
        }
        else if (reg >= 2 & reg < 6)
        {
            value = this->dm1.config.get_REG_BOUNDS(reg-2);
        }
        else if (reg >= 6 & reg < 10)
        {
            value = this->dm1.config.get_REG_STRIDES(reg-6);
        }
        else if (reg >= 24 & reg < 28)
        {
            value = this->dm1.config.get_REG_RPTR(reg-24);
        }
        else if (reg >= 28 & reg < 32)
        {
            value = this->dm1.config.get_REG_WPTR(reg-28);
        }
        else
        {
            // Invalid configuration
            this->trace.msg("Invalid configuration register\n");
            value = 0;
        }
    }
    // Get from data mover 2
    else if (ssr == 2)
    {
        if (unlikely(reg == 0))
        {
            value = this->dm2.config.get_REG_STATUS();
        }
        else if (unlikely(reg == 1))
        {
            value = this->dm2.config.get_REG_REPEAT();
        }
        else if (reg >= 2 & reg < 6)
        {
            value = this->dm2.config.get_REG_BOUNDS(reg-2);
        }
        else if (reg >= 6 & reg < 10)
        {
            value = this->dm2.config.get_REG_STRIDES(reg-6);
        }
        else if (reg >= 24 & reg < 28)
        {
            value = this->dm2.config.get_REG_RPTR(reg-24);
        }
        else if (reg >= 28 & reg < 32)
        {
            value = this->dm2.config.get_REG_WPTR(reg-28);
        }
        else
        {
            // Invalid configuration
            this->trace.msg("Invalid configuration register\n");
            value = 0;
        }
    }
    else
    {
        // Invalid data mover index
        this->trace.msg("Invalid configuration data mover\n");
    }
    return value;
}


// Get called when the streamer read from / write to TCDM.
// Working principal is similar to LSU data_req().
int Ssr::data_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write, int64_t &latency, int dm)
{
    this->trace.msg("Data request (addr: 0x%lx, size: 0x%x, is_write: %d, data_mover: %d)\n", addr, size, is_write, dm);
    vp::IoReq *req = &this->io_req;
    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data_ptr);

    int err;
    if (dm == 0)
    {
        err = this->ssr_dm0.req(req);
    }
    else if (dm == 1)
    {
        err = this->ssr_dm1.req(req);
    }
    else if (dm == 2)
    {
        err = this->ssr_dm2.req(req);
    }
    else
    {
        this->trace.msg("Invalid data request in data mover\n");
    }

    if (err == vp::IO_REQ_OK)
    {
        #ifdef CONFIG_GVSOC_ISS_SNITCH
        // In case of a write, don't signal a valid transaction. Stores are always
        // without ans answer to the core.
        // latency = req->get_latency();
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

        // Total number of latency in memory access instruction is the sum of latency waiting for operands to be ready
        // and latency resulting from ports contention/conflicts.

#if defined(PIPELINE_STALL_THRESHOLD)
        if (latency > PIPELINE_STALL_THRESHOLD)
        {
            // this->trace.msg(vp::Trace::LEVEL_TRACE, "latency: %d, PIPELINE_STALL_THRESHOLD: %d\n", latency, PIPELINE_STALL_THRESHOLD);
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Number of SSR stall cycles: %d\n", latency - PIPELINE_STALL_THRESHOLD);
            // this->iss.timing.stall_load_account(latency - PIPELINE_STALL_THRESHOLD);
            // this->event->stall_cycle_inc(latency - PIPELINE_STALL_THRESHOLD);

            // (latency model work under dm_event)
            // Use stall_cycle_set instead of stall_cycle_inc, because three data movers have their private memory ports.
            // The latency of memory access in three data movers has the relationship of MAX, not SUM.
            // if ((latency - PIPELINE_STALL_THRESHOLD) > this->event->stall_cycle_get())
            // {
            //     this->event->stall_cycle_set(latency - PIPELINE_STALL_THRESHOLD);
            // }
        }
#endif

        return 0;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        latency = this->io_req.get_latency() + 1;

#if defined(PIPELINE_STALL_THRESHOLD)
        if (latency > PIPELINE_STALL_THRESHOLD)
        {
            // this->iss.timing.stall_load_account(latency - PIPELINE_STALL_THRESHOLD);
            // this->event->stall_cycle_inc(latency - PIPELINE_STALL_THRESHOLD);
        }
#endif

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


// Get called when the fifo reads data from memory.
template<typename T>
inline bool Ssr::load_float(iss_addr_t addr, uint8_t *data_ptr, int size, int dm)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.load_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)data_ptr, size, false, latency, dm)) == 0)
    {

    }
    else
    {
        if (err == vp::IO_REQ_INVALID)
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



// Get called when fifo writes data to memory.
template<typename T>
inline bool Ssr::store_float(iss_addr_t addr, uint8_t *data_ptr, int size, int dm)
{
    iss_addr_t phys_addr;
    bool use_mem_array;
    if (this->iss.mmu.store_virt_to_phys(addr, phys_addr, use_mem_array))
    {
        return false;
    }

    int err;
    int64_t latency;
    if ((err = this->data_req(phys_addr, (uint8_t *)data_ptr, size, true, latency, dm)) == 0)
    {

    }
    else
    {
        if (err == vp::IO_REQ_INVALID)
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


// (Work under dm_event)
// iss_freg_t Ssr::dm_read(int dm)
// {
//     this->trace.msg("Read register value from SSR data mover %d\n", dm);
//     if (dm == 0)
//     {
//         this->dm0.dm_read = true;
//         if (this->dm0.config.REG_STRIDES[0] == 0x4)
//         {
//             if (this->dm0.fifo.isEmpty())
//             {
//                 this->load_float<uint32_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 4, dm);
//                 this->dm0.temp = iss_get_float_value(this->dm0.temp, 4 * 8);
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
//                 this->dm0.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint32_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 4, dm);
//             // this->dm0.temp = iss_get_float_value(this->dm0.temp, 4 * 8);
//             // this->dm0.fifo.push(this->dm0.temp);
//             this->ssr_fregs[0] = this->dm0.fifo.pop();
//             // this->dm0.update_cnt();
//         }
//         else if (this->dm0.config.REG_STRIDES[0] == 0x8)
//         {
//             if (this->dm0.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
//                 this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
//                 this->dm0.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
//             // this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
//             // this->dm0.fifo.push(this->dm0.temp);
//             this->ssr_fregs[0] = this->dm0.fifo.pop();
//             // this->dm0.update_cnt();
//             this->trace.msg("%d %d %d %d %d %d %d %d %d %d %d %d\n", this->dm0.counter.bound[0], this->dm0.counter.bound[1], this->dm0.counter.bound[2], this->dm0.counter.bound[3],
//                                                         this->dm0.config.REG_STRIDES[0], this->dm0.config.REG_STRIDES[1], this->dm0.config.REG_STRIDES[2], this->dm0.config.REG_STRIDES[3],
//                                                         this->dm0.config.REG_BOUNDS[0], this->dm0.config.REG_BOUNDS[1], this->dm0.config.REG_BOUNDS[2], this->dm0.config.REG_BOUNDS[3]);
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             if (this->dm0.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
//                 this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
//                 this->dm0.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
//             // this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
//             // this->dm0.fifo.push(this->dm0.temp);
//             this->ssr_fregs[0] = this->dm0.fifo.pop();
//         }

//         return ssr_fregs[0];
//     }
//     else if (dm == 1)
//     {
//         this->dm1.dm_read = true;
//         if (this->dm1.config.REG_STRIDES[0] == 0x4)
//         {
//             if (this->dm1.fifo.isEmpty())
//             {
//                 this->load_float<uint32_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 4, dm);
//                 this->dm1.temp = iss_get_float_value(this->dm1.temp, 4 * 8);
//                 this->dm1.fifo.push(this->dm1.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
//                 this->dm1.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint32_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 4, dm);
//             // this->dm1.temp = iss_get_float_value(this->dm1.temp, 4 * 8);
//             // this->dm1.fifo.push(this->dm1.temp);
//             this->ssr_fregs[1] = this->dm1.fifo.pop();
//             // this->dm1.update_cnt();
//         }
//         else if (this->dm1.config.REG_STRIDES[0] == 0x8)
//         {
//             if (this->dm1.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
//                 this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
//                 this->dm1.fifo.push(this->dm1.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
//                 this->dm1.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
//             // this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
//             // this->dm1.fifo.push(this->dm1.temp);
//             this->ssr_fregs[1] = this->dm1.fifo.pop();
//             // this->dm1.update_cnt();
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             if (this->dm1.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
//                 this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
//                 this->dm1.fifo.push(this->dm1.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
//                 this->dm1.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
//             // this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
//             // this->dm1.fifo.push(this->dm1.temp);
//             this->ssr_fregs[1] = this->dm1.fifo.pop();
//             // this->dm1.update_cnt();
//             this->trace.msg("%d %d %d %d %d %d %d %d\n", this->dm1.counter.bound[0], this->dm1.counter.bound[1], this->dm1.counter.bound[2], this->dm1.counter.bound[3],
//                                                         this->dm1.config.REG_STRIDES[0], this->dm1.config.REG_STRIDES[1], this->dm1.config.REG_STRIDES[2], this->dm1.config.REG_STRIDES[3]);
//         }

//         return ssr_fregs[1];
//     }
//     else if (dm == 2)
//     {
//         this->dm2.dm_read = true;
//         if (this->dm2.config.REG_STRIDES[0] == 0x4)
//         {
//             if (this->dm2.fifo.isEmpty())
//             {
//                 this->load_float<uint32_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 4, dm);
//                 this->dm2.temp = iss_get_float_value(this->dm2.temp, 4 * 8);
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
//                 this->dm2.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint32_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 4, dm);
//             // this->dm2.temp = iss_get_float_value(this->dm2.temp, 4 * 8);
//             // this->dm2.fifo.push(this->dm2.temp);
//             this->ssr_fregs[2] = this->dm2.fifo.pop();
//             // this->dm2.update_cnt();
//         }
//         else if (this->dm2.config.REG_STRIDES[0] == 0x8)
//         {
//             if (this->dm2.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
//                 this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
//                 this->dm2.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
//             // this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
//             // this->dm2.fifo.push(this->dm2.temp);
//             this->ssr_fregs[2] = this->dm2.fifo.pop();
//             // this->dm2.update_cnt();
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             if (this->dm2.fifo.isEmpty())
//             {
//                 this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
//                 this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
//                 this->dm2.update_cnt();
//                 this->trace.msg("No data preloaded, SSR stalled for %d cycles\n", this->event->stall_cycle_get());
//                 this->iss.timing.stall_load_account(this->event->stall_cycle_get());
//             }
//             // this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
//             // this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
//             // this->dm2.fifo.push(this->dm2.temp);
//             this->ssr_fregs[2] = this->dm2.fifo.pop();
//         }

//         return ssr_fregs[2];
//     }
//     else
//     {
//         this->trace.msg("Invalid data request in data mover\n");
//     }

// }


// (Work under dm_event)
// bool Ssr::dm_write(iss_freg_t value, int dm)
// {
//     this->trace.msg("Write register value 0x%llx to SSR data mover %d\n", value, dm);
//     if (dm == 0)
//     {
//         this->dm0.dm_write = true;
//         // Write data into FIFO
//         this->dm0.temp = value;

//         // Write data to memory
//         if (this->dm0.config.REG_STRIDES[0] == 0x4)
//         {
//             // this->store_float<uint32_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 4, dm);
//             if (!this->dm0.fifo.isFull())
//             {
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->ssr_fregs[0] = this->dm0.temp; 
//             }
//             // this->ssr_fregs[0] = this->dm0.fifo.pop();
//             // this->dm0.update_cnt();
//             return true;
//         }
//         else if (this->dm0.config.REG_STRIDES[0] == 0x8)
//         {
//             // this->store_float<uint64_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 8, dm);
//             if (!this->dm0.fifo.isFull())
//             {
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->ssr_fregs[0] = this->dm0.temp;
//             }
//             // this->ssr_fregs[0] = this->dm0.fifo.pop();
//             // this->dm0.update_cnt();
//             return true;
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             // this->store_float<uint64_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 8, dm);
//             if (!this->dm0.fifo.isFull())
//             {
//                 this->dm0.fifo.push(this->dm0.temp);
//                 this->ssr_fregs[0] = this->dm0.temp;
//             }
//             // this->ssr_fregs[0] = this->dm0.fifo.pop();
//             return false;
//         }
//     }
//     else if (dm == 1)
//     {
//         this->dm1.dm_write = true;
//         // Write data into FIFO
//         this->dm1.temp = value;

//         if (this->dm1.config.REG_STRIDES[0] == 0x4)
//         {
//             // this->store_float<uint32_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 4, dm);
//             if (!this->dm1.fifo.isFull())
//             {
//                 this->dm1.fifo.push(this->dm1.temp);
//                 this->ssr_fregs[1] = this->dm1.temp;
//             }
//             // this->ssr_fregs[1] = this->dm1.fifo.pop();
//             // this->dm1.update_cnt();
//             return true;
//         }
//         else if (this->dm1.config.REG_STRIDES[0] == 0x8)
//         {
//             // this->store_float<uint64_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 8, dm);
//             if (!this->dm1.fifo.isFull())
//             {
//                 // this->dm1.fifo.push(this->dm1.temp);
//                 // this->ssr_fregs[1] = this->dm1.temp;
//                 this->dm1.fifo.push(value);
//                 this->ssr_fregs[1] = value;
//             }
//             // this->ssr_fregs[1] = this->dm1.fifo.pop();
//             // this->dm1.update_cnt();
//             return true;
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             // this->store_float<uint64_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 8, dm);
//             if (!this->dm1.fifo.isFull())
//             {
//                 // this->dm1.fifo.push(this->dm1.temp);
//                 // this->ssr_fregs[1] = this->dm1.temp;
//                 this->dm1.fifo.push(value);
//                 this->ssr_fregs[1] = value;
//             }
//             // this->ssr_fregs[1] = this->dm1.fifo.pop();
//             return false;
//         }
//     }
//     else if (dm == 2)
//     {
//         this->dm2.dm_write = true;
//         // Write data into FIFO
//         this->dm2.temp = value;

//         if (this->dm2.config.REG_STRIDES[0] == 0x4)
//         {
//             // this->store_float<uint32_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 4, dm);
//             if (!this->dm2.fifo.isFull())
//             {
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->ssr_fregs[2] = this->dm2.temp;
//             }
//             // this->ssr_fregs[2] = this->dm2.fifo.pop();
//             // this->dm2.update_cnt();
//             return true;
//         }
//         else if (this->dm2.config.REG_STRIDES[0] == 0x8)
//         {
//             // this->store_float<uint64_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 8, dm);
//             if (!this->dm2.fifo.isFull())
//             {
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->ssr_fregs[2] = this->dm2.temp;
//             }
//             // this->ssr_fregs[2] = this->dm2.fifo.pop();
//             // this->dm2.update_cnt();
//             return true;
//         }
//         else
//         {
//             this->trace.msg("Misaligned data request in data mover\n");
//             // this->store_float<uint64_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 8, dm);
//             if (!this->dm2.fifo.isFull())
//             {
//                 this->dm2.fifo.push(this->dm2.temp);
//                 this->ssr_fregs[2] = this->dm2.temp;
//             }
//             // this->ssr_fregs[2] = this->dm2.fifo.pop();
//             return false;
//         }
//     }
//     else
//     {
//         this->trace.msg("Invalid data request in data mover\n");
//         return false;
//     }

// }


// (work under no dm_event)
iss_freg_t Ssr::dm_read(int dm)
{
    this->trace.msg("Read register value from SSR data mover %d\n", dm);
    if (dm == 0)
    {
        this->dm0.dm_read = true;
        if (this->dm0.config.REG_STRIDES[0] == 0x4)
        {
            this->load_float<uint32_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 4, dm);
            this->dm0.temp = iss_get_float_value(this->dm0.temp, 4 * 8);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
            // this->dm0.update_cnt();
        }
        else if (this->dm0.config.REG_STRIDES[0] == 0x8)
        {
            this->trace.msg("temp_addr: 0x%llx, inc_addr: 0x%llx\n", this->dm0.temp_addr, this->dm0.inc_addr);
            this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
            this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
            // this->dm0.update_cnt();
            this->trace.msg("%d %d %d %d %d %d %d %d %d %d %d %d\n", this->dm0.counter.bound[0], this->dm0.counter.bound[1], this->dm0.counter.bound[2], this->dm0.counter.bound[3],
                                                        this->dm0.config.REG_STRIDES[0], this->dm0.config.REG_STRIDES[1], this->dm0.config.REG_STRIDES[2], this->dm0.config.REG_STRIDES[3],
                                                        this->dm0.config.REG_BOUNDS[0], this->dm0.config.REG_BOUNDS[1], this->dm0.config.REG_BOUNDS[2], this->dm0.config.REG_BOUNDS[3]);
        }
        else
        {
            this->trace.msg("temp_addr: 0x%llx, inc_addr: 0x%llx\n", this->dm0.temp_addr, this->dm0.inc_addr);
            this->trace.msg("Misaligned data request in data mover\n");
            this->load_float<uint64_t>(this->dm0.addr_gen_unit(false), (uint8_t *)(&this->dm0.temp), 8, dm);
            this->dm0.temp = iss_get_float_value(this->dm0.temp, 8 * 8);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
        }

        return ssr_fregs[0];
    }
    else if (dm == 1)
    {
        this->dm1.dm_read = true;
        if (this->dm1.config.REG_STRIDES[0] == 0x4)
        {
            this->load_float<uint32_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 4, dm);
            this->dm1.temp = iss_get_float_value(this->dm1.temp, 4 * 8);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            // this->dm1.update_cnt();
        }
        else if (this->dm1.config.REG_STRIDES[0] == 0x8)
        {
            this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
            this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            // this->dm1.update_cnt();
        }
        else
        {
            this->trace.msg("Misaligned data request in data mover\n");
            this->load_float<uint64_t>(this->dm1.addr_gen_unit(false), (uint8_t *)(&this->dm1.temp), 8, dm);
            this->dm1.temp = iss_get_float_value(this->dm1.temp, 8 * 8);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            // this->dm1.update_cnt();
            this->trace.msg("%d %d %d %d %d %d %d %d\n", this->dm1.counter.bound[0], this->dm1.counter.bound[1], this->dm1.counter.bound[2], this->dm1.counter.bound[3],
                                                        this->dm1.config.REG_STRIDES[0], this->dm1.config.REG_STRIDES[1], this->dm1.config.REG_STRIDES[2], this->dm1.config.REG_STRIDES[3]);
        }

        return ssr_fregs[1];
    }
    else if (dm == 2)
    {
        this->dm2.dm_read = true;
        if (this->dm2.config.REG_STRIDES[0] == 0x4)
        {
            this->load_float<uint32_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 4, dm);
            this->dm2.temp = iss_get_float_value(this->dm2.temp, 4 * 8);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
            // this->dm2.update_cnt();
        }
        else if (this->dm2.config.REG_STRIDES[0] == 0x8)
        {
            this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
            this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
            // this->dm2.update_cnt();
        }
        else
        {
            this->trace.msg("Misaligned data request in data mover\n");
            this->load_float<uint64_t>(this->dm2.addr_gen_unit(false), (uint8_t *)(&this->dm2.temp), 8, dm);
            this->dm2.temp = iss_get_float_value(this->dm2.temp, 8 * 8);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
        }

        return ssr_fregs[2];
    }
    else
    {
        this->trace.msg("Invalid data request in data mover\n");
    }

}


// (work under no dm_event)
bool Ssr::dm_write(iss_freg_t value, int dm)
{
    this->trace.msg("Write register value to SSR data mover %d\n", dm);
    if (dm == 0)
    {
        this->dm0.dm_write = true;
        this->dm0.temp = value;

        // Write data to memory
        if (this->dm0.config.REG_STRIDES[0] == 0x4)
        {
            this->store_float<uint32_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 4, dm);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
            // this->dm0.update_cnt();
            return true;
        }
        else if (this->dm0.config.REG_STRIDES[0] == 0x8)
        {
            this->store_float<uint64_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 8, dm);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
            // this->dm0.update_cnt();
            return true;
        }
        else
        {
            this->trace.msg("Misaligned data request in data mover\n");
            this->store_float<uint64_t>(this->dm0.addr_gen_unit(true), (uint8_t *)(&this->dm0.temp), 8, dm);
            this->dm0.fifo.push(this->dm0.temp);
            this->ssr_fregs[0] = this->dm0.fifo.pop();
            return false;
        }
    }
    else if (dm == 1)
    {
        this->dm1.dm_write = true;
        this->dm1.temp = value;

        if (this->dm1.config.REG_STRIDES[0] == 0x4)
        {
            this->store_float<uint32_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 4, dm);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            // this->dm1.update_cnt();
            return true;
        }
        else if (this->dm1.config.REG_STRIDES[0] == 0x8)
        {
            this->store_float<uint64_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 8, dm);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            // this->dm1.update_cnt();
            return true;
        }
        else
        {
            this->trace.msg("Misaligned data request in data mover\n");
            this->store_float<uint64_t>(this->dm1.addr_gen_unit(true), (uint8_t *)(&this->dm1.temp), 8, dm);
            this->dm1.fifo.push(this->dm1.temp);
            this->ssr_fregs[1] = this->dm1.fifo.pop();
            return false;
        }
    }
    else if (dm == 2)
    {
        this->dm2.dm_write = true;
        this->dm2.temp = value;

        if (this->dm2.config.REG_STRIDES[0] == 0x4)
        {
            this->store_float<uint32_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 4, dm);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
            // this->dm2.update_cnt();
            return true;
        }
        else if (this->dm2.config.REG_STRIDES[0] == 0x8)
        {
            this->store_float<uint64_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 8, dm);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
            // this->dm2.update_cnt();
            return true;
        }
        else
        {
            this->trace.msg("Misaligned data request in data mover\n");
            this->store_float<uint64_t>(this->dm2.addr_gen_unit(true), (uint8_t *)(&this->dm2.temp), 8, dm);
            this->dm2.fifo.push(this->dm2.temp);
            this->ssr_fregs[2] = this->dm2.fifo.pop();
            return false;
        }
    }
    else
    {
        this->trace.msg("Invalid data request in data mover\n");
        return false;
    }

}


// Clear read/write flags after one computation instruction.
// This function is used to handle vector instructions, some of operands would be read/write for more than once,
// in order to avoid redundant memory access to the same memory address and repetitive address update.
void Ssr::clear_flags()
{
    this->dm0.dm_read = false;
    this->dm0.dm_write = false;
    this->dm1.dm_read = false;
    this->dm1.dm_write = false;
    this->dm2.dm_read = false;
    this->dm2.dm_write = false;
}


// Get called when update the address generation unit after one computation instruction,
// ready for memory address in the next instruction.
// (Work under no dm_event)
void Ssr::update_ssr()
{
    if (this->dm0.dm_read | this->dm0.dm_write)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
        this->dm0.update_cnt();
    }
    if (this->dm1.dm_read | this->dm1.dm_write)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
        this->dm1.update_cnt();
    }
    if (this->dm2.dm_read | this->dm2.dm_write)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
        this->dm2.update_cnt();
    }
}


// Get called for data fetching before the corresponding computation instruction.
// This executes by default every cycle if we don't consider any stall.
// This is critical for data preloading.
// Todo: write logic hard to deal with when the fifo is full.
// In real hardware, if fifo is full, stall the subsystem and rewrite to new computation result.
void Ssr::dm_event(vp::Block *__this, vp::ClockEvent *event)
{
    Ssr *_this = (Ssr *)__this;

    _this->trace.msg("dm_event\n");

    // Data mover 0
    // First check whether the target data mover is active
    if (_this->dm0.is_config)
    // if (_this->dm0.is_config)
    {
        // Then check whether it's a read or write lane
        // Read lane: read data from memory to lane
        if (!_this->dm0.is_write & !_this->dm0.ssr_done)
        {
            // Send data request from ssr streamer to memory,
            // when fifo is not fill and there's remaining memory access.
            _this->dm0.credit.credit_take = false;
            if (_this->dm0.rep_done | (_this->dm0.temp_addr==_this->dm0.config.REG_RPTR[_this->dm0.config.DIM]))
            {
                _this->dm0.credit.credit_give = true;
            }
            _this->dm0.credit.update_cnt();
            _this->trace.msg("dm0.credit.has_credit %d\n", _this->dm0.credit.has_credit);
            // _this->dm0.credit.has_credit = 1;

            // Push memory response to fifo
            if (!_this->dm0.fifo.isFull() & _this->dm0.credit.has_credit)
            {
                _this->trace.msg("temp_addr: 0x%llx, inc_addr: 0x%llx\n", _this->dm0.temp_addr, _this->dm0.inc_addr);
                _this->load_float<uint64_t>(_this->dm0.addr_gen_unit(false), (uint8_t *)(&_this->dm0.temp), 8, 0);
                _this->dm0.temp = iss_get_float_value(_this->dm0.temp, 8 * 8);
                _this->dm0.fifo.push(_this->dm0.temp);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
                _this->dm0.update_cnt();
                _this->trace.msg("dm0.temp: 0x%llx\n", _this->dm0.temp);
            }
        }
        // Write lane: write data from lane to memory
        else if (_this->dm0.is_write)
        {
            // Pop computation result from fifo to memory
            if (!_this->dm0.fifo.isEmpty())
            {
                iss_freg_t value;
                value = _this->dm0.fifo.pop();
                _this->store_float<uint64_t>(_this->dm0.addr_gen_unit(true), (uint8_t *)(&value), 8, 0);
                _this->trace.msg("ssr_fregs[0]: 0x%llx\n",value);
                // _this->ssr_fregs[0] = _this->dm0.fifo.pop();
                // _this->store_float<uint64_t>(_this->dm0.addr_gen_unit(true), (uint8_t *)(&_this->ssr_fregs[0]), 8, 0);
                // _this->trace.msg("ssr_fregs[0]: 0x%llx\n",_this->ssr_fregs[0]);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 0\n");
                _this->dm0.update_cnt();
            }
        }
    }

    // Data mover 1
    // First check whether the target data mover is active
    if (_this->dm1.is_config)
    // if (_this->dm1.is_config)
    {
        // Then check whether it's a read or write lane
        // Read lane: read data from memory to lane
        if (!_this->dm1.is_write & !_this->dm1.ssr_done)
        {
            // Send data request from ssr streamer to memory,
            // when fifo is not fill and there's remaining memory access.
            _this->dm1.credit.credit_take = false;
            if (_this->dm1.rep_done | (_this->dm1.temp_addr==_this->dm1.config.REG_RPTR[_this->dm1.config.DIM]))
            {
                _this->dm1.credit.credit_give = true;
            }
            _this->dm1.credit.update_cnt();
            _this->trace.msg("dm1.credit.has_credit %d\n", _this->dm1.credit.has_credit);
            // _this->dm1.credit.has_credit = 1;

            // Push memory response to fifo
            if (!_this->dm1.fifo.isFull() & _this->dm1.credit.has_credit)
            {
                _this->trace.msg("temp_addr: 0x%llx, inc_addr: 0x%llx\n", _this->dm1.temp_addr, _this->dm1.inc_addr);
                _this->load_float<uint64_t>(_this->dm1.addr_gen_unit(false), (uint8_t *)(&_this->dm1.temp), 8, 1);
                _this->dm1.temp = iss_get_float_value(_this->dm1.temp, 8 * 8);
                _this->dm1.fifo.push(_this->dm1.temp);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
                _this->dm1.update_cnt();
                _this->trace.msg("dm1.temp: 0x%llx\n", _this->dm1.temp);
            }
        }
        // Write lane: write data from lane to memory
        else if (_this->dm1.is_write)
        {
            // Pop computation result from fifo to memory
            if (!_this->dm1.fifo.isEmpty())
            {
                iss_freg_t value;
                value = _this->dm1.fifo.pop();
                _this->store_float<uint64_t>(_this->dm1.addr_gen_unit(true), (uint8_t *)(&value), 8, 1);
                _this->trace.msg("ssr_fregs[1]: 0x%llx\n",value);
                // _this->ssr_fregs[1] = _this->dm1.fifo.pop();
                // _this->store_float<uint64_t>(_this->dm1.addr_gen_unit(true), (uint8_t *)(&_this->ssr_fregs[1]), 8, 1);
                // _this->trace.msg("ssr_fregs[1]: 0x%llx\n",_this->ssr_fregs[1]);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 1\n");
                _this->dm1.update_cnt();
            }
        }
    }


    // Data mover 2
    // First check whether the target data mover is active
    if (_this->dm2.is_config)
    // if (_this->dm2.is_config)
    {
        // Then check whether it's a read or write lane
        // Read lane: read data from memory to lane
        if (!_this->dm2.is_write & !_this->dm2.ssr_done)
        {
            // Send data request from ssr streamer to memory,
            // when fifo is not fill and there's remaining memory access.
            _this->dm2.credit.credit_take = false;
            if (_this->dm2.rep_done | (_this->dm2.temp_addr==_this->dm2.config.REG_RPTR[_this->dm2.config.DIM]))
            {
                _this->dm2.credit.credit_give = true;
            }
            _this->dm2.credit.update_cnt();
            _this->trace.msg("dm2.credit.has_credit %d\n", _this->dm2.credit.has_credit);
            // _this->dm2.credit.has_credit = 1;

            // Push memory response to fifo
            if (!_this->dm2.fifo.isFull() & _this->dm2.credit.has_credit)
            {
                _this->trace.msg("temp_addr: 0x%llx, inc_addr: 0x%llx\n", _this->dm2.temp_addr, _this->dm2.inc_addr);
                _this->load_float<uint64_t>(_this->dm2.addr_gen_unit(false), (uint8_t *)(&_this->dm2.temp), 8, 2);
                _this->dm2.temp = iss_get_float_value(_this->dm2.temp, 8 * 8);
                _this->dm2.fifo.push(_this->dm2.temp);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
                _this->dm2.update_cnt();
                _this->trace.msg("dm2.temp: 0x%llx\n", _this->dm2.temp);
            }
        }
        // Write lane: write data from lane to memory
        else if (_this->dm2.is_write)
        {
            // Pop computation result from fifo to memory
            if (!_this->dm2.fifo.isEmpty())
            {
                iss_freg_t value;
                value = _this->dm2.fifo.pop();
                _this->store_float<uint64_t>(_this->dm2.addr_gen_unit(true), (uint8_t *)(&value), 8, 2);
                _this->trace.msg("ssr_fregs[2]: 0x%llx\n",value);
                // _this->ssr_fregs[2] = _this->dm2.fifo.pop();
                // _this->store_float<uint64_t>(_this->dm2.addr_gen_unit(true), (uint8_t *)(&_this->ssr_fregs[2]), 8, 2);
                // _this->trace.msg("ssr_fregs[2]: 0x%llx\n",_this->ssr_fregs[2]);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Update loop counter in data mover 2\n");
                _this->dm2.update_cnt();
            }
        }
    }

}