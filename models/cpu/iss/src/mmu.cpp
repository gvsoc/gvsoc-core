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
 *          David Schaffenrath, ETH Zurich
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

    if (mode != 0 && mode != 8)
    {
        this->trace.force_warning("Only 39-bit virtual addressing is supported\n");
        return false;
    }

    this->satp = value;
    this->asid = asid;
    this->mode = mode;
    this->pt_base = pt_base;

    this->trace.msg(vp::trace::LEVEL_DEBUG, "Updated SATP (base: 0x%x, asid: %d, mode: %d)\n",
        pt_base, asid, mode);

    return true;

#else

    this->trace.force_warning("MMU is only supported for 64-bits cores\n");
    return false;

#endif
}
