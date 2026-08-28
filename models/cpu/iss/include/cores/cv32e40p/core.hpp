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

#pragma once

#include <cpu/iss/include/core.hpp>

class Cv32e40pCore : public Core
{
public:
    Cv32e40pCore(Iss &iss) : Core(iss) {}

protected:
    /* CV32E40P is M-mode only.
     * CSR writes can set MPP to 0, but
     * MRET must ignore that and stay in M-mode. */
    void mret_mode_restore() override;

    /* CV32E40P mstatus_write_mask FPU-aware.
     * only MIE(3) + MPIE(7) writable; MPP forced to M by hardware.
     * With FPU: add FS(14:13). */
    void mstatus_write_mask_fixup() override;
};
