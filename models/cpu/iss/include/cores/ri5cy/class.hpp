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
#include <vp/register.hpp>
#include <cpu/iss/include/lsu.hpp>
#include <cpu/iss/include/decode.hpp>
#include <cpu/iss/include/trace.hpp>
#include <cpu/iss/include/csr.hpp>
#include <cpu/iss/include/dbgunit.hpp>
#include <cpu/iss/include/exception.hpp>
#include <cpu/iss/include/syscalls.hpp>
#include <cpu/iss/include/timing.hpp>
#include <cpu/iss/include/regfile.hpp>
#include <cpu/iss/include/irq/irq_external.hpp>
#include <cpu/iss/include/core.hpp>
#include <cpu/iss/include/memcheck.hpp>
#include <cpu/iss/include/insn_cache.hpp>
#include <cpu/iss/include/exec/exec_inorder.hpp>
#include <cpu/iss/include/prefetch/prefetch_single_line.hpp>
#include <cpu/iss/include/gdbserver.hpp>


class IssWrapper;


class Iss
{
public:
    Iss(IssWrapper &top);

    Regfile regfile;
    Exec exec;
    InsnCache insn_cache;
    Timing timing;
    Core core;
    Prefetcher prefetcher;
    Decode decode;
    Irq irq;
    Gdbserver gdbserver;
    Lsu lsu;
    DbgUnit dbgunit;
    Syscalls syscalls;
    Trace trace;
    Csr csr;
    Exception exception;
    Memcheck memcheck;

    iss_pulp_nn_t pulp_nn;
    iss_rnnext_t rnnext;




    vp::Component &top;
};


class IssWrapper : public vp::Component
{

public:
    IssWrapper(vp::ComponentConf &config);

    void start();
    void stop();
    void reset(bool active);

    Iss iss;

private:
    vp::Trace trace;
};

inline Iss::Iss(IssWrapper &top)
    : prefetcher(*this), exec(top, *this), insn_cache(*this), decode(*this), timing(*this), core(*this), irq(*this),
      gdbserver(*this), lsu(top, *this), dbgunit(*this), syscalls(top, *this), trace(*this), csr(*this),
      regfile(top, *this), exception(*this), memcheck(top, *this), top(top)
{
}


#include "cpu/iss/include/isa/rv64i.hpp"
#include "cpu/iss/include/isa/rv32i.hpp"
#include "cpu/iss/include/isa/rv32c.hpp"
#include "cpu/iss/include/isa/zcmp.hpp"
#include "cpu/iss/include/isa/rv32a.hpp"
#include "cpu/iss/include/isa/rv64c.hpp"
#include "cpu/iss/include/isa/rv32m.hpp"
#include "cpu/iss/include/isa/rv64m.hpp"
#include "cpu/iss/include/isa/rv64a.hpp"
#include "cpu/iss/include/isa/rvf.hpp"
#include "cpu/iss/include/isa/rvd.hpp"
#include "cpu/iss/include/isa/rvXf16.hpp"
#include "cpu/iss/include/isa/rvXf16alt.hpp"
#include "cpu/iss/include/isa/rvXf8.hpp"
#include "cpu/iss/include/isa/rv32Xfvec.hpp"
#include "cpu/iss/include/isa/rv32Xfaux.hpp"
#include "cpu/iss/include/isa/priv.hpp"
#include "cpu/iss/include/isa/pulp_v2.hpp"
#include "cpu/iss/include/isa/rvXgap9.hpp"
#include "cpu/iss/include/isa/rvXint64.hpp"
#include "cpu/iss/include/isa/rnnext.hpp"
#include "cpu/iss/include/isa/pulp_nn.hpp"

#include <cpu/iss/include/exec/exec_inorder_implem.hpp>
