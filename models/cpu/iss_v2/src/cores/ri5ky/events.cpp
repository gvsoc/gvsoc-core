// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <cpu/iss_v2/include/cores/ri5ky/csr.hpp>

void Ri5kyEvents::reset(bool active)
{
    Events::reset(active);
    if (active) {
        this->cycles_start_cyclestamp = -1;
    }
}

void Ri5kyEvents::flush_cycles()
{
    if (this->cycles_start_cyclestamp != -1) {
        this->iss.csr.pccr_account(CSR_PCER_CYCLES,
            this->iss.clock.get_cycles() - this->cycles_start_cyclestamp);
        this->cycles_start_cyclestamp = this->iss.clock.get_cycles();
    }
}
