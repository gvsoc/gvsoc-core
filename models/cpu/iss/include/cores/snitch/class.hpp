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
#include <cpu/iss/include/lsu.hpp>
#include <cpu/iss/include/decode.hpp>
#include <cpu/iss/include/trace.hpp>
#include <cpu/iss/include/csr.hpp>
#include <cpu/iss/include/dbgunit.hpp>
#include <cpu/iss/include/exception.hpp>
#include <cpu/iss/include/syscalls.hpp>
#include <cpu/iss/include/timing.hpp>
#include <cpu/iss/include/regfile.hpp>
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
#include <cpu/iss/include/irq/irq_riscv.hpp>
#else
#include <cpu/iss/include/irq/irq_external.hpp>
#endif
#include <cpu/iss/include/core.hpp>
#include <cpu/iss/include/mmu.hpp>
#include <cpu/iss/include/pmp.hpp>
#include <cpu/iss/include/exec/exec_inorder.hpp>
#include <cpu/iss/include/prefetch/prefetch_single_line.hpp>
#include <cpu/iss/include/gdbserver.hpp>

#include <cpu/iss/include/spatz.hpp>

class IssWrapper;


class Iss
{
public:
    Iss(vp::Component &top);

    Exec exec;
    Timing timing;
    Core core;
    Regfile regfile;
    Prefetcher prefetcher;
    Decode decode;
    Irq irq;
    Gdbserver gdbserver;
    Lsu lsu;
    DbgUnit dbgunit;
    Syscalls syscalls;
    Trace trace;
    Csr csr;
    Mmu mmu;
    Pmp pmp;
    Exception exception;

    Spatz spatz;



    vp::Component &top;

private:
    bool barrier_update(bool is_write, iss_reg_t &value);
    static void barrier_sync(void *__this, bool value);

    vp::WireMaster<bool> barrier_req_itf;
    vp::WireSlave<bool> barrier_ack_itf;
    CsrReg barrier;
    bool waiting_barrier;
};


class IssWrapper : public vp::Component
{

public:
    IssWrapper(vp::ComponentConf &config);

    void start();
    void reset(bool active);
    virtual void target_open();

    Iss iss;

private:
    vp::Trace trace;
};


#include "cpu/iss/include/rv64i.hpp"
#include "cpu/iss/include/rv32i.hpp"
#include "cpu/iss/include/rv32v.hpp"
#include "cpu/iss/include/rv32c.hpp"
#include "cpu/iss/include/zcmp.hpp"
#include "cpu/iss/include/rv32a.hpp"

#if ISS_REG_WIDTH == 64
#include "cpu/iss/include/rv64c.hpp"
#endif
#include "cpu/iss/include/rv32m.hpp"
#include "cpu/iss/include/rv64m.hpp"
#include "cpu/iss/include/rv64a.hpp"
#include "cpu/iss/include/rvd.hpp"
#include "cpu/iss/include/rvf.hpp"
#include "cpu/iss/include/rvXf16.hpp"
#include "cpu/iss/include/rvXf16alt.hpp"
#include "cpu/iss/include/rvXf8.hpp"
#include "cpu/iss/include/rv32Xfvec.hpp"
#include "cpu/iss/include/rv32Xfaux.hpp"
#include "cpu/iss/include/priv.hpp"
