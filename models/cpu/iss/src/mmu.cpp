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

    this->iss.csr.satp.register_callback(std::bind(&Mmu::satp_update, this, std::placeholders::_1));
}

void Mmu::reset(bool active)
{
    this->satp = 0;
}

bool Mmu::satp_update(iss_reg_t value)
{
#if ISS_REG_WIDTH == 64

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

void Mmu::raise_exception()
{
    if (this->access_is_load)
    {
        this->iss.exception.raise(ISS_EXCEPT_LOAD_PAGE_FAULT);
    }
    else if (this->access_is_store)
    {
        this->iss.exception.raise(ISS_EXCEPT_STORE_PAGE_FAULT);
    }
    else
    {
        this->iss.exception.raise(ISS_EXCEPT_INSN_PAGE_FAULT);
    }
}

bool Mmu::handle_pte()
{
    // printf("HANDLE PTE %llx %d\n", this->pte_value, this->pte_value.v);

    iss_reg_t pte_attr = this->pte_value.raw & MMU_PTE_ATTR;

    if (!this->pte_value.v || (!this->pte_value.r && this->pte_value.w) ||
        (this->pte_value.raw & MMU_PTE_ATTR) != 0)
    {
        printf("Raise\n");
        this->raise_exception();
        return false;
    }

    if (this->pte_value.r || this->pte_value.x)
    {
        printf("Found leaf\n");
        return true;
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
            iss_addr_t pte_addr = (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT << MMU_PGSHIFT;
            // printf("PPN %x\n", (this->pte_value.raw & ~MMU_PTE_ATTR) >> MMU_PTE_PPN_SHIFT);
            return this->read_pte(pte_addr);
        }
    }
}

bool Mmu::read_pte(iss_addr_t pte_addr)
{
    // printf("Loading entry at %lx\n", pte_addr);
    this->iss.lsu.data_req(pte_addr, (uint8_t *)&this->pte_value.raw, this->pte_size, false);
    if (this->iss.exec.stalled.get())
    {
        this->iss.exec.instr_event->meth_set(&this->iss, &Mmu::handle_pte_stub);
        this->iss.exec.insn_hold();
        return false;
    }
    else
    {
        return this->handle_pte();
    }
}


bool Mmu::walk_pgtab(iss_addr_t virt_addr)
{
    printf("Miss at %x\n", virt_addr);

    this->current_virt_addr = virt_addr;
    this->current_level = this->nb_levels - 1;
    this->current_vpn_bit = MMU_PGSHIFT + this->nb_levels * this->vpn_width;

    this->current_vpn_bit -= this->vpn_width;
    int vpn_index = get_field(this->current_virt_addr, this->current_vpn_bit, this->vpn_width);
    iss_addr_t pte_addr = this->pt_base + vpn_index*this->pte_size;

    return this->read_pte(pte_addr);
}


iss_addr_t Mmu::virt_to_phys_miss(iss_addr_t virt_addr)
{
    int mode = this->iss.core.mode_get();
    int tag = virt_addr >> MMU_PGSHIFT;
    int index = tag & MMU_TLB_ENTRIES_MASK;
    iss_addr_t page_virt_addr = tag << MMU_PGSHIFT;
    iss_addr_t page_phys_addr;
    bool load = true, store = true, fetch = true;

    if (mode == PRIV_M)
    {
        page_phys_addr = page_virt_addr;
    }
    else
    {
        if (!this->walk_pgtab(virt_addr))
        {
            return 0;
        }
    }

    this->tlb_phys_addr[index] = page_phys_addr - page_virt_addr;
    if (load)
    {
        this->tlb_insn_tag[index] = tag;
    }
    if (store)
    {
        this->tlb_insn_tag[index] = tag;
    }
    if (fetch)
    {
        this->tlb_insn_tag[index] = tag;
    }

    return virt_addr + this->tlb_phys_addr[index];
}
