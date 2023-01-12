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

typedef struct
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    unsigned int NX : 1;
                    unsigned int UF : 1;
                    unsigned int OF : 1;
                    unsigned int DZ : 1;
                    unsigned int NV : 1;
                };
                unsigned int raw : 5;
            } fflags;
            unsigned int frm : 3;
        };
        iss_reg_t raw;
    };
} iss_fcsr_t;

class Mstatus
{
    public:
        union
        {
            iss_reg_t value;
            struct
            {
#if ISS_REG_WIDTH == 64
                unsigned int uie:1;
                unsigned int sie:1;
                unsigned int reserved0:1;
                unsigned int mie:1;
                unsigned int upie:1;
                unsigned int spie:1;
                unsigned int reserved1:1;
                unsigned int mpie:1;
                unsigned int spp:1;
                unsigned int reserved2:2;
                unsigned int mpp:2;
                unsigned int fs:2;
                unsigned int xs:2;
                unsigned int mprv:1;
                unsigned int sum:1;
                unsigned int mxr:1;
                unsigned int tvm:1;
                unsigned int tw:1;
                unsigned int tsr:1;
                unsigned int reserved3:8;
                unsigned int sd:1;
#else
                unsigned int uie:1;
                unsigned int sie:1;
                unsigned int reserved0:1;
                unsigned int mie:1;
                unsigned int upie:1;
                unsigned int spie:1;
                unsigned int reserved1:1;
                unsigned int mpie:1;
                unsigned int spp:1;
                unsigned int reserved2:2;
                unsigned int mpp:2;
                unsigned int fs:2;
                unsigned int xs:2;
                unsigned int mprv:1;
                unsigned int sum:1;
                unsigned int mxr:1;
                unsigned int tvm:1;
                unsigned int tw:1;
                unsigned int tsr:1;
                unsigned int reserved3:9;
                unsigned int uxl:2;
                unsigned int sxl:2;
                unsigned int reserved4:27;
                unsigned int sd:1;
#endif
            };
        };
};


class Csr
{
public:
    Csr(Iss &iss);

    void build();
    void reset(bool active);

    void declare_pcer(int index, std::string name, std::string help);

    Iss &iss;

    vp::trace trace;

    Mstatus mstatus;
    iss_reg_t depc;
    iss_reg_t dcsr;
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
    iss_reg_t sscratch;
    iss_fcsr_t fcsr;
    iss_reg_t misa;
    iss_reg_t mhartid;
    iss_reg_t mepc;
    iss_reg_t mcause;
    iss_reg_t mtvec;
    iss_reg_t sepc;
    iss_reg_t scause;
    iss_reg_t sie;
    iss_reg_t stvec;


    bool hwloop = false;
    iss_reg_t hwloop_regs[HWLOOP_NB_REGS];
};
