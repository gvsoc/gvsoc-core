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

class Csr;

typedef struct
{
    union
    {
        struct
        {
            unsigned int fflags : 5;
            unsigned int frm : 3;
        };
        iss_reg_t raw;
    };
} __attribute__((packed)) iss_fcsr_t;

class CsrAbtractReg
{
    friend class Csr;

public:
    CsrAbtractReg(iss_reg_t *value=NULL);
    void register_callback(std::function<bool(bool, iss_reg_t &)> callback);
    bool access(bool is_write, iss_reg_t &value);

    std::string name;
    iss_reg_t reset_val;

protected:
    void reset(bool active);

    iss_reg_t write_mask;

private:
    std::vector<std::function<bool(bool, iss_reg_t &)>> callbacks;
    iss_reg_t default_value;
    iss_reg_t *value_p;
};

class CsrReg : public CsrAbtractReg
{
    friend class Csr;

public:
    CsrReg() : CsrAbtractReg(&this->value) {}

    iss_reg_t value;
};

class Mstatus : public CsrAbtractReg
{
    public:
        Mstatus() : CsrAbtractReg(&this->value) {}
        union
        {
            iss_reg_t value;
            struct
            {
#if ISS_REG_WIDTH == 32
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
    void declare_csr(CsrAbtractReg *reg, std::string name, iss_reg_t address, iss_reg_t reset_val=0, iss_reg_t mask=-1);
    CsrAbtractReg *get_csr(iss_reg_t address);

    bool access(bool is_write, iss_reg_t address, iss_reg_t &value);

    Iss &iss;

    vp::trace trace;

    CsrReg cycle;
    CsrReg instret;

    CsrReg sstatus;
    CsrReg sie;
    CsrReg stvec;
    CsrReg scounteren;

    CsrReg  sscratch;
    CsrReg  sepc;
    CsrReg  scause;
    CsrReg  stval;
    CsrReg  sip;

    CsrReg  satp;

    Mstatus mstatus;
    CsrReg misa;
    CsrReg  medeleg;
    CsrReg  mideleg;
    CsrReg  mie;
    CsrReg  mtvec;
    CsrReg  mcounteren;

    CsrReg  mscratch;
    CsrReg  mepc;
    CsrReg  mcause;
    CsrReg  mtval;
    CsrReg  mip;

    CsrReg tselect;
    CsrReg tdata1;
    CsrReg tdata2;
    CsrReg tdata3;

    CsrReg mvendorid;
    CsrReg marchid;

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
    iss_fcsr_t fcsr;
    iss_reg_t mhartid;

    CsrReg vstart;
    CsrReg vxstat;
    CsrReg vxrm;
    CsrReg vcsr;
    CsrReg vl;
    CsrReg vtype;
    CsrReg vlenb;


    bool hwloop = false;
    iss_reg_t hwloop_regs[HWLOOP_NB_REGS];

private:

    bool tselect_access(bool is_write, iss_reg_t &value);

    std::map<iss_reg_t, CsrAbtractReg *> regs;

};
