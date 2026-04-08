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

#include <cpu/iss/include/core.hpp>

class Cv32e40pCore : public Core
{
public:
    Cv32e40pCore(Iss &iss) : Core(iss) {}

protected:
    /* CV32E40P RTL does NOT clear mcause/scause on mret/sret (D21 fix).
     * Override hooks with empty bodies to suppress the generic clearing. */
    void post_mret_hook() override {}
    void post_sret_hook() override {}
};
