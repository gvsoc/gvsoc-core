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

#include <cpu/iss/include/irq/irq_riscv.hpp>

class Cv32e40pIrq : public Irq
{
public:
    Cv32e40pIrq(Iss &iss) : Irq(iss) {}

    void reset(bool active) override;
    bool mip_access(bool is_write, iss_reg_t &value) override;
    bool mie_access(bool is_write, iss_reg_t &value) override;
    bool mtvec_access(bool is_write, iss_reg_t &value) override;
    void elw_irq_unstall() override;

protected:
    void register_csr_callbacks() override;
};
