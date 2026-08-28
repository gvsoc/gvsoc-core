/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 * Copyright (C) 2026 Fondazione Chips-it
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
 * Authors: Marco Paci, Fondazione Chips-it (marco.paci@chips.it)
 */

#ifdef CONFIG_GVSOC_ISS_CV32E40P

#include <vp/vp.hpp>
#include <cpu/iss/include/iss.hpp>

void Cv32e40pIrq::register_csr_callbacks()
{
    this->iss.csr.mip.register_callback(std::bind(&Cv32e40pIrq::mip_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.mie.register_callback(std::bind(&Cv32e40pIrq::mie_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.mtvec.register_callback(std::bind(&Cv32e40pIrq::mtvec_access, this, std::placeholders::_1, std::placeholders::_2));
}

bool Cv32e40pIrq::mip_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        iss_reg_t mask = this->iss.csr.mip.write_mask;
        this->iss.csr.mip.value = (this->iss.csr.mip.value & ~mask) | (value & mask);
    }
    else
    {
        value = this->iss.csr.mip.value;
    }
    this->check_interrupts();
    return false;
}

bool Cv32e40pIrq::mie_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        iss_reg_t mask = this->iss.csr.mie.write_mask;
        this->iss.csr.mie.value = (this->iss.csr.mie.value & ~mask) | (value & mask);
    }
    else
    {
        value = this->iss.csr.mie.value;
    }
    this->check_interrupts();
    return false;
}

bool Cv32e40pIrq::mtvec_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->mtvec_set(value);
        iss_reg_t mask = this->iss.csr.mtvec.write_mask;
        this->iss.csr.mtvec.value = (this->iss.csr.mtvec.value & ~mask) | (value & mask);
        return false;
    }
    else
    {
        value = this->iss.csr.mtvec.value;
        return true;
    }
}

void Cv32e40pIrq::elw_irq_unstall()
{
    this->trace.msg("Interrupting pending elw\n");
    this->iss.exec.current_insn = this->iss.exec.elw_insn;
    this->iss.exec.elw_interrupted = 1;
    this->iss.exec.busy_enter();
}

/* CV32E40P vectored trap entry.  mtvec.value keeps the MODE bit (write_mask
 * 0xFFFFFF01): MODE=1 (vectored) -> an interrupt enters at (base & ~1) + cause*4;
 * direct mode (and exceptions) enter at the base.  Matches the RTL mtvec.MODE. */
iss_reg_t Cv32e40pIrq::compute_trap_entry(iss_reg_t base, int cause, bool is_interrupt)
{
    iss_reg_t vbase = base & ~(iss_reg_t)1;
    if (is_interrupt && (base & 1))
    {
        return vbase + (iss_reg_t)cause * 4;
    }
    return vbase;
}


#endif /* CONFIG_GVSOC_ISS_CV32E40P */
