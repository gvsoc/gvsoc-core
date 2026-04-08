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
 * Authors: Marco Paci, Chips-it (marco.paci@chips.it)
 */

#pragma once

#include <cpu/iss/include/csr.hpp>

class Cv32e40pCsr : public Csr
{
public:
    Cv32e40pCsr(Iss &iss);

    void build();           // Calls Csr::build() then build_cv32e40p()
    void build_cv32e40p();  // CV32E40P-specific CSR customization
    void reset(bool active);

    bool mstatus_access(bool is_write, iss_reg_t &value) override;
    bool minstret_access(bool is_write, iss_reg_t &value) override;
    bool mcycle_access(bool is_write, iss_reg_t &value) override;
    void mstatus_read_fixup(iss_reg_t &value) override;

    // PULP custom CSRs (0xCD0-0xCD2)
    CsrReg uhartid;     // 0xCD0 — duplicate of mhartid (user-mode readable)
    CsrReg privlv;      // 0xCD1 — current privilege level
    CsrReg zfinx_csr;   // 0xCD2 — ZFINX indicator (1 if FPU+ZFINX, else 0)

private:
    bool mcountinhibit_access(bool is_write, iss_reg_t &value);

    int64_t mcycle_offset = 0;
    bool m_fpu_in_isa = false;
    bool m_zfinx = false;
};
