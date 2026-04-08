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

#pragma once

#include <vp/vp.hpp>
#include <cpu/iss/include/types.hpp>

class Exception
{
public:
    Exception(Iss &iss);
    virtual ~Exception() = default;

    void build();

    void raise(iss_reg_t pc, int id);

    /* Hook for core-specific trap vector PC masking.
     * Default: return vec_value unchanged (generic RISC-V).
     * CV32E40P: return vec_value & ~3 (mtvec[1:0] hardwired to 0). */
    virtual iss_reg_t trap_vector_pc(iss_reg_t vec_value) { return vec_value; }

    iss_addr_t debug_handler_addr;

protected:
    Iss &iss;
    vp::Trace trace;
};
