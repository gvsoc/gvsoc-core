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

static inline iss_reg_t get_field(iss_reg_t field, int bit, int width)
{
    // The shift must be done in the register-width type: fields such as the satp
    // PPN are up to 44 bits wide, so "1 << width" with a 32-bit int is undefined
    // behaviour and yields a wrong mask, which breaks the page-table walk once
    // paging is enabled.
    return (field >> bit) & (((iss_reg_t)1 << width) - 1);
}

Mmu::Mmu(Iss &iss)
: iss(iss)
{
    this->iss.traces.new_trace("mmu", &this->trace, vp::DEBUG);

    this->iss.csr.satp.register_callback(std::bind(&Mmu::satp_update, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    this->pte_req.set_data((uint8_t *)&this->pte_value.raw);
}

void Mmu::reset(bool active)
{
    if (active)
    {
        this->satp = 0;
        this->mode = MMU_MODE_OFF;
        this->walk_pending = false;

        this->flush(0, 0);
    }
}

bool Mmu::satp_update(iss_insn_t *insn, bool is_write, iss_reg_t &value)
{
    if (this->iss.core.mode_get() == PRIV_S && this->iss.csr.mstatus.tvm)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Modifying satp in supervisor mode while TVM is 1\n");
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_ILLEGAL);
        return false;
    }

#if ISS_REG_WIDTH == 64

    if (is_write)
    {
        iss_reg_t pt_base = get_field(value, 0, 44) << 12;
        iss_reg_t asid = get_field(value, 44, 16);
        iss_reg_t mode = get_field(value, 60, 4);

        if (mode != 0 && mode != MMU_MODE_SV39)
        {
            this->trace.force_warning("Only 39-bit virtual addressing is supported\n");
            return false;
        }

        this->satp = value;
        this->asid = asid;
        this->mode = mode;
        this->pt_base = pt_base;

        if (this->mode == MMU_MODE_SV39)
        {
            this->nb_levels = 3;
            this->pte_size = 8;
            this->vpn_width = 9;
        }

        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Updated SATP (base: 0x%x, asid: %d, mode: %d)\n",
            pt_base, asid, mode);
    }

    this->flush(0, 0);
    this->iss.insn_cache.mode_flush();

    return true;

#else

    this->trace.force_warning("MMU is only supported for 64-bits cores\n");
    return false;

#endif
}

void Mmu::flush(iss_addr_t address, iss_reg_t address_space)
{
    // For now just flush everything
    for (int i=0; i<MMU_TLB_NB_ENTRIES; i++)
    {
        this->tlb_insn_tag[i] = -1;
        this->tlb_load_tag[i] = -1;
        this->tlb_store_tag[i] = -1;
    }
}

void Mmu::raise_exception()
{
    this->walk_pending = false;

    // Report the faulting virtual address in both tval registers, the exception
    // will be taken in the mode selected by medeleg.
    this->iss.csr.stval.value = this->current_virt_addr;
    this->iss.csr.mtval.value = this->current_virt_addr;

    if (this->access_type & ACCESS_LOAD)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Illegal load access (pc: 0x%lx, 0x%lx)\n",
            this->iss.exec.current_insn, this->current_virt_addr);
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_LOAD_PAGE_FAULT);
    }
    else if (this->access_type & ACCESS_STORE)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Illegal store access (pc: 0x%lx, 0x%lx)\n",
            this->iss.exec.current_insn, this->current_virt_addr);
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_STORE_PAGE_FAULT);
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Illegal fetch access (pc: 0x%lx, 0x%lx)\n",
            this->iss.exec.current_insn, this->current_virt_addr);
        this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_INSN_PAGE_FAULT);
    }
}

// Process the PTE which was just read and keep walking the page table until a leaf
// or a fault is found. Returns false when the walk completed successfully and the
// TLB has been filled, and true when either an exception was raised or the walk is
// still pending on an asynchronous PTE read.
bool Mmu::walk_continue()
{
    while (1)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Handle pte (value: 0x%lx)\n", this->pte_value.raw);

        if (!this->pte_value.v || (!this->pte_value.r && this->pte_value.w) ||
            (this->pte_value.raw & MMU_PTE_ATTR) != 0)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Illegal pte entry\n");
            this->raise_exception();
            return true;
        }

        if (this->pte_value.r || this->pte_value.x)
        {
            // A leaf has been found

            iss_addr_t phys_base = (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT << MMU_PGSHIFT;

            // In case we are not at the last level, check if we have a misaligned superpage
            if (this->current_level > 0)
            {
                int lower_ppn = get_field(phys_base, MMU_PGSHIFT, this->vpn_width * this->current_level);
                if (lower_ppn != 0)
                {
                    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Found misaligned superpage\n");
                    this->raise_exception();
                    return true;
                }

                int vpn_index = get_field(this->current_virt_addr, MMU_PGSHIFT,
                    this->vpn_width * this->current_level);
                phys_base += (iss_addr_t)vpn_index << MMU_PGSHIFT;
            }

            iss_addr_t virt_base = this->current_virt_addr >> MMU_PGSHIFT << MMU_PGSHIFT;
            iss_addr_t tag = this->current_virt_addr >> MMU_PGSHIFT;
            int index = tag & MMU_TLB_ENTRIES_MASK;
            if (this->access_type & ACCESS_INSN)
            {
                if (!this->pte_value.a || !this->pte_value.x)
                {
                    this->raise_exception();
                    return true;
                }

                this->tlb_insn_tag[index] = tag;
                this->tlb_insn_phys_addr[index] = phys_base - virt_base;
            }
            else
            {
                bool is_store = this->access_type & ACCESS_STORE;
                bool is_load = this->access_type & ACCESS_LOAD;
                if (!this->pte_value.a ||
                    (is_load && !this->pte_value.r) ||
                    (is_store && (!this->pte_value.w || !this->pte_value.d)))
                {
                    this->raise_exception();
                    return true;
                }

                this->tlb_load_tag[index] = -1;
                this->tlb_store_tag[index] = -1;

                if (this->pte_value.r)
                {
                    this->tlb_load_tag[index] = tag;
                }
                if (this->pte_value.w && this->pte_value.d)
                {
                    this->tlb_store_tag[index] = tag;
                }
                this->tlb_load_use_mem_array[index] = false;
                this->tlb_ls_phys_addr[index] = phys_base - virt_base;
            }

            this->walk_pending = false;
            return false;
        }
        else
        {
            this->current_level--;
            this->current_vpn_bit -= this->vpn_width;

            if (this->current_level < 0)
            {
                this->raise_exception();
                return true;
            }

            iss_addr_t pte_page = (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT << MMU_PGSHIFT;
            int vpn_index = get_field(this->current_virt_addr, this->current_vpn_bit, this->vpn_width);
            iss_addr_t pte_addr = pte_page + vpn_index*this->pte_size;
            if (this->read_pte(pte_addr))
            {
                // Either the read faulted (exception raised) or it is pending, in
                // both cases the walk is over for now.
                return true;
            }

            // The PTE was read synchronously, loop to process it
        }
    }
}

void Mmu::handle_pte_response()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received pte response (value: 0x%lx)\n",
        this->pte_value.raw);

    if (this->pte_req.status == vp::IO_REQ_INVALID)
    {
        this->walk_pending = false;
        this->iss.csr.stval.value = this->current_virt_addr;
        this->iss.csr.mtval.value = this->current_virt_addr;
        this->iss.exception.raise(this->iss.exec.current_insn,
            this->access_type & ACCESS_LOAD ? ISS_EXCEPT_LOAD_FAULT :
            this->access_type & ACCESS_STORE ? ISS_EXCEPT_STORE_FAULT : ISS_EXCEPT_INSN_FAULT);
        return;
    }

    // Resume the walk. The core keeps retrying the stalled access and will find the
    // translation in the TLB once the walk is over.
    this->walk_continue();
}

// Read one PTE through the LSU data port. Returns false if the PTE is available
// (synchronous response), and true if an exception was raised or the response is
// pending.
bool Mmu::read_pte(iss_addr_t pte_addr)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Read pte (addr: 0x%lx)\n", pte_addr);

    vp::IoReq *req = &this->pte_req;
    req->prepare();
    req->set_data((uint8_t *)&this->pte_value.raw);
    req->set_addr(pte_addr);
    req->set_size(this->pte_size);
    req->set_opcode(vp::IoReqOpcode::READ);

    vp::IoReqStatus err = this->iss.lsu.data.req(req);
    if (err == vp::IO_REQ_OK)
    {
        return false;
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        this->walk_pending = false;
        this->iss.csr.stval.value = this->current_virt_addr;
        this->iss.csr.mtval.value = this->current_virt_addr;
        this->iss.exception.raise(this->iss.exec.current_insn,
            this->access_type & ACCESS_LOAD ? ISS_EXCEPT_LOAD_FAULT :
            this->access_type & ACCESS_STORE ? ISS_EXCEPT_STORE_FAULT : ISS_EXCEPT_INSN_FAULT);
        return true;
    }
    else
    {
        // The response is asynchronous, the walk will be resumed when it is
        // received, through the LSU response callback.
        return true;
    }
}

// Start a page-table walk for the given virtual address. Returns false when the
// walk completed synchronously and the TLB has been filled, and true when an
// exception was raised or the walk is pending.
bool Mmu::walk_pgtab(iss_addr_t virt_addr)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Page-table walk (virt_addr: 0x%lx)\n", virt_addr);

    this->walk_pending = true;
    this->current_virt_addr = virt_addr;
    this->current_level = this->nb_levels - 1;
    this->current_vpn_bit = MMU_PGSHIFT + this->current_level * this->vpn_width;

    int vpn_index = get_field(virt_addr, this->current_vpn_bit, this->vpn_width);
    iss_addr_t pte_addr = this->pt_base + vpn_index*this->pte_size;

    if (this->read_pte(pte_addr))
    {
        return true;
    }

    return this->walk_continue();
}


bool Mmu::virt_to_phys_miss(iss_addr_t virt_addr, iss_addr_t &phys_addr, bool &use_mem_array)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling miss (virt_addr: 0x%lx)\n", virt_addr);

    int mode = this->iss.core.mode_get();
    if (this->iss.csr.mstatus.mprv && !(this->access_type & ACCESS_INSN))
    {
        mode = this->iss.csr.mstatus.mpp;
    }

    iss_addr_t tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;
    iss_addr_t page_virt_addr = tag << MMU_PGSHIFT;

    if (mode == PRIV_M || this->mode == MMU_MODE_OFF)
    {
        phys_addr = virt_addr;

        if (this->access_type & ACCESS_INSN)
        {
            this->tlb_insn_phys_addr[index] = 0;
            this->tlb_insn_tag[index] = tag;
        }
        else
        {
            this->tlb_ls_phys_addr[index] = 0;
            this->tlb_load_tag[index] = tag;
            this->tlb_load_use_mem_array[index] = false;
            this->tlb_store_tag[index] = tag;
            use_mem_array = false;
        }
        return false;
    }

    if (this->walk_pending)
    {
        // A walk is already in progress on an asynchronous PTE read. The core keeps
        // retrying the access until the walk fills the TLB or raises an exception.
        return true;
    }

    if (this->walk_pgtab(virt_addr))
    {
        return true;
    }

    // The walk completed synchronously, the TLB now contains the translation unless
    // the access rights do not match the access, in which case an exception was
    // raised.
    if (this->access_type & ACCESS_INSN)
    {
        phys_addr = virt_addr + this->tlb_insn_phys_addr[index];
    }
    else
    {
        phys_addr = virt_addr + this->tlb_ls_phys_addr[index];
        use_mem_array = false;
    }
    return false;
}
