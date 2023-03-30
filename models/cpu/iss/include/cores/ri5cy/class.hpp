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
#include <lsu.hpp>
#include <decode.hpp>
#include <trace.hpp>
#include <csr.hpp>
#include <dbgunit.hpp>
#include <exception.hpp>
#include <syscalls.hpp>
#include <timing.hpp>
#include <regfile.hpp>
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
#include <irq/irq_riscv.hpp>
#else
#include <irq/irq_external.hpp>
#endif
#include <core.hpp>
#include <mmu.hpp>
#include <pmp.hpp>
#include <exec/exec_inorder.hpp>
#include <prefetch/prefetch_single_line.hpp>
#include <gdbserver.hpp>

class IssWrapper;


class Iss
{
public:
    Iss(vp::component &top);

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

    iss_pulp_nn_t pulp_nn;
    iss_rnnext_t rnnext;

    vp::component &top;
};


class IssWrapper : public vp::component
{

public:
    IssWrapper(js::config *config);

    int build();
    void start();
    void reset(bool active);
    virtual void target_open();

    Iss iss;
};
