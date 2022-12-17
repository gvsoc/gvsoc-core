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

#include <types.hpp>

class Csr
{
public:
    Csr(Iss &iss);

    void build();

    void declare_pcer(int index, std::string name, std::string help);

    Iss &iss;

    vp::trace trace;

    iss_reg_t status;
    iss_reg_t epc;
    iss_reg_t depc;
    iss_reg_t dcsr;
    iss_reg_t mtvec;
    iss_reg_t mcause;
#if defined(ISS_HAS_PERF_COUNTERS)
    iss_reg_t pccr[32];
    iss_reg_t pcer;
    iss_reg_t pcmr;
#endif
    iss_reg_t stack_conf;
    iss_reg_t stack_start;
    iss_reg_t stack_end;
    iss_reg_t scratch0;
    iss_reg_t scratch1;
    iss_reg_t mscratch;
};
