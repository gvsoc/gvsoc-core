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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#pragma once

#include <cpu/iss/include/types.hpp>

class OffloadRsp
{
public:
    // Destination register index if the output is integer operand
    int rd;
    // Whether the operation has error
    bool error;
    // Value of output result if the output is integer operand
    iss_reg_t data;
    // Computation result fflags
    unsigned int fflags;
    // PC of finished instruction
    iss_reg_t pc;
    // Offloaded instruction info
    iss_insn_t insn;
};