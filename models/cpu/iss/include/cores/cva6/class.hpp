/*
 * Copyright (C) 2020 SAS, ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
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
#include <cpu/iss/include/irq/irq_riscv.hpp>
#include <cpu/iss/include/core.hpp>
#include <cpu/iss/include/mmu.hpp>
#include <cpu/iss/include/vector.hpp>
#include <cpu/iss/include/pmp.hpp>
#include <cpu/iss/include/memcheck.hpp>
#include <cpu/iss/include/insn_cache.hpp>
#include <cpu/iss/include/exec/exec_inorder.hpp>
#include <cpu/iss/include/prefetch/prefetch_single_line.hpp>
#include <cpu/iss/include/gdbserver.hpp>
#include <cpu/iss/include/cores/ara/ara.hpp>

class IssWrapper;

#define ISS_INSN_FLAGS_VECTOR (1<< 0)


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
    Mmu mmu;
    Pmp pmp;
    Exception exception;
    Memcheck memcheck;
    Vector vector;
    Ara ara;

    IssWrapper &top;
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
    static iss_reg_t insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    static iss_reg_t vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::ClockEvent fsm_event;
    vp::Trace trace;
    bool do_flush;
    std::queue<iss_insn_t *> pending_insns;
    std::queue<bool> pending_insns_done;
    std::queue<uint64_t> pending_insns_timestamp;
    std::queue<iss_addr_t> pending_insns_pc;
};

inline Iss::Iss(IssWrapper &top)
    : prefetcher(*this), exec(top, *this), insn_cache(*this), decode(*this), timing(*this),
    core(*this), irq(*this), gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(top, *this),
    trace(*this), csr(*this), regfile(top, *this), mmu(*this), pmp(*this), exception(*this),
    memcheck(top, *this), vector(*this), ara(top, *this), top(top)
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
#include "cpu/iss/include/isa/rv32v.hpp"
#include "cpu/iss/include/isa/rvd.hpp"
#include "cpu/iss/include/isa/priv.hpp"

#include <cpu/iss/include/exec/exec_inorder_implem.hpp>
