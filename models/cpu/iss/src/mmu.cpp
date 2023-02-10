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

#include "iss.hpp"

#define ISS_MMU_SATP_PPN_WIDTH  22
#define ISS_MMU_SATP_PPN_WIDTH  22
#define ISS_MMU_SATP_ASID_BIT   22
#define ISS_MMU_SATP_ASID_WIDTH 9
#define ISS_MMU_SATP_MODE_BIT   31
#define ISS_MMU_SATP_MODE_WIDTH 1

static inline iss_reg_t get_field(iss_reg_t field, int bit, int width)
{
    return (field >> bit) & ((1 << width) - 1);
}

Mmu::Mmu(Iss &iss)
: iss(iss)
{
}

void Mmu::build()
{
    this->iss.top.traces.new_trace("mmu", &this->trace, vp::DEBUG);

    this->iss.csr.satp.register_callback(std::bind(&Mmu::satp_update, this, std::placeholders::_1, std::placeholders::_2));
}

void Mmu::reset(bool active)
{
    this->satp = 0;

    this->flush(0, 0);

}

bool Mmu::satp_update(bool is_write, iss_reg_t &value)
{
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

        this->trace.msg(vp::trace::LEVEL_DEBUG, "Updated SATP (base: 0x%x, asid: %d, mode: %d)\n",
            pt_base, asid, mode);
    }

    return true;

#else

    this->trace.force_warning("MMU is only supported for 64-bits cores\n");
    return false;

#endif
}


void Mmu::handle_pte_stub(void *__this, vp::clock_event *event)
{
    Iss *iss = (Iss *)__this;
    Mmu *_this = &iss->mmu;
    _this->handle_pte();
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
    this->iss.csr.stval.value = this->current_virt_addr;

    if (this->access_type & ACCESS_LOAD)
    {
        this->iss.exec.current_insn = this->iss.exception.raise(ISS_EXCEPT_LOAD_PAGE_FAULT);
    }
    else if (this->access_type & ACCESS_STORE)
    {
        this->iss.exec.current_insn = this->iss.exception.raise(ISS_EXCEPT_STORE_PAGE_FAULT);
    }
    else
    {
        this->iss.exec.current_insn = this->iss.exception.raise(ISS_EXCEPT_INSN_PAGE_FAULT);
    }

    this->iss.trace.dump_trace_enabled = true;
    this->iss.exec.switch_to_full_mode();
}

bool Mmu::handle_pte()
{
    iss_reg_t pte_attr = this->pte_value.raw & MMU_PTE_ATTR;

    this->trace.msg(vp::trace::LEVEL_TRACE, "Handle pte (value: 0x%lx)\n", this->pte_value.raw);

    if (!this->pte_value.v || (!this->pte_value.r && this->pte_value.w) ||
        (this->pte_value.raw & MMU_PTE_ATTR) != 0)
    {
        this->raise_exception();
        return false;
    }

    if (this->pte_value.r || this->pte_value.x)
    {
        iss_addr_t phys_base = (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT << MMU_PGSHIFT;

        if (this->current_level > 0)
        {
            int vpn_index = get_field(this->current_virt_addr, MMU_PGSHIFT, this->vpn_width);
            phys_base += vpn_index << MMU_PGSHIFT;
        }

        iss_addr_t virt_base = this->current_virt_addr >> MMU_PGSHIFT << MMU_PGSHIFT;
        iss_addr_t tag = this->current_virt_addr >> MMU_PGSHIFT;
        int index = tag & MMU_TLB_ENTRIES_MASK;
        if (this->access_type & ACCESS_INSN)
        {
            if (!this->pte_value.a || !this->pte_value.x)
            {
                this->raise_exception();
                return false;
            }

            this->tlb_insn_tag[index] = tag;
            this->tlb_insn_phys_addr[index] = phys_base - virt_base;
        }
        else
        {
            bool is_store = this->access_type & ACCESS_STORE;
            bool is_load = this->access_type & ACCESS_LOAD;
            if (!this->pte_value.a ||
                is_load && !this->pte_value.r ||
                is_store && (!this->pte_value.w || !this->pte_value.d))
            {
                this->raise_exception();
                return false;
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
            this->tlb_ls_phys_addr[index] = phys_base - virt_base;
        }
        this->iss.trace.dump_trace_enabled = true;
        this->iss.exec.switch_to_full_mode();
        this->iss.exec.current_insn = this->stall_insn;
        return false;
    }
    else
    {
        this->current_level--;
        this->current_vpn_bit -= this->vpn_width;

        if (this->current_level < 0)
        {
            this->raise_exception();
            return false;
        }
        else
        {
            iss_addr_t pte_page = (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT << MMU_PGSHIFT;
            int vpn_index = get_field(this->current_virt_addr, this->current_vpn_bit, this->vpn_width);
            iss_addr_t pte_addr = pte_page + vpn_index*this->pte_size;
            this->read_pte(pte_addr);
            return false;
        }
    }
}

void Mmu::handle_pte_response(Lsu *lsu)
{
    lsu->iss.mmu.handle_pte();
}

void Mmu::read_pte(iss_addr_t pte_addr)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Read pte (addr: 0x%lx)\n", pte_addr);

    int err = this->iss.lsu.data_req(pte_addr, (uint8_t *)&this->pte_value.raw, this->pte_size, false);
    if (err == vp::IO_REQ_OK)
    {
        // The should have already accounted the request latency.
        // Just do nothing so tha the pte load occupy one
        // full cycle. Maybe we should account 2 cycles since we have a load dependency on
        // the pte load.
    }
    else if (err == vp::IO_REQ_INVALID)
    {
        // Nothing to do since the LSU have already raise an exception
    }
    else
    {
        // Just register the stall calbback to be notified by the response
        this->iss.lsu.stall_callback = &Mmu::handle_pte_response;
    }
}


void Mmu::walk_pgtab(iss_addr_t virt_addr)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Page-table walk (virt_addr: 0x%lx)\n", virt_addr);

    // Remember now the current instruction, in case walking the page-table is stalling the core
    // so that we can re-execute the instruction once the translatio is done.
    // This is needed for example, when a load instruction is triggering a miss.
    this->stall_insn = this->iss.exec.current_insn;

    this->current_virt_addr = virt_addr;
    this->current_level = this->nb_levels - 1;
    this->current_vpn_bit = MMU_PGSHIFT + this->nb_levels * this->vpn_width;

    this->current_vpn_bit -= this->vpn_width;
    int vpn_index = get_field(this->current_virt_addr, this->current_vpn_bit, this->vpn_width);
    iss_addr_t pte_addr = this->pt_base + vpn_index*this->pte_size;

    this->iss.exec.instr_event->meth_set(&this->iss, &Mmu::handle_pte_stub);
    this->iss.exec.insn_hold();

    this->read_pte(pte_addr);
}


bool Mmu::virt_to_phys_miss(iss_addr_t virt_addr, iss_addr_t &phys_addr)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Handling miss (virt_addr: 0x%lx)\n", virt_addr);
    int mode = this->iss.core.mode_get();
    int tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;
    iss_addr_t page_virt_addr = tag << MMU_PGSHIFT;
    iss_addr_t page_phys_addr;
    bool load = true, store = true, fetch = true;

    if (mode == PRIV_M || this->mode == MMU_MODE_OFF)
    {
        page_phys_addr = page_virt_addr;

        if (this->access_type & ACCESS_INSN)
        {
            this->tlb_insn_phys_addr[index] = page_phys_addr - page_virt_addr;
            this->tlb_insn_tag[index] = tag;
        }
        else
        {
            this->tlb_ls_phys_addr[index] = page_phys_addr - page_virt_addr;
            this->tlb_load_tag[index] = tag;
            this->tlb_store_tag[index] = tag;
        }
        phys_addr = virt_addr + page_phys_addr - page_virt_addr;
        return false;
    }
    else
    {
        this->walk_pgtab(virt_addr);
        return true;
    }

}
