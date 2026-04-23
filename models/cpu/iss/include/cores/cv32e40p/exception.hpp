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

#include <cpu/iss/include/exception.hpp>

class Cv32e40pException : public Exception
{
public:
    Cv32e40pException(Iss &iss) : Exception(iss) {}

    /* CV32E40P always aligns the trap vector to 4 bytes.
     * This matches RTL behavior where mtvec[1:0] are hardwired to 0. */
    iss_reg_t trap_vector_pc(iss_reg_t vec_value) override
    {
        return vec_value & ~(iss_reg_t)0x3;
    }
};
