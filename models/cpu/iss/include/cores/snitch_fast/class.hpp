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
#include <cpu/iss/include/vector.hpp>
#include <cpu/iss/include/cores/snitch_fast/regfile.hpp>
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
#include <cpu/iss/include/irq/irq_riscv.hpp>
#else
#include <cpu/iss/include/irq/irq_external.hpp>
#endif
#include <cpu/iss/include/core.hpp>
#include <cpu/iss/include/mmu.hpp>
#include <cpu/iss/include/pmp.hpp>
#include <cpu/iss/include/memcheck.hpp>
#include <cpu/iss/include/insn_cache.hpp>
#include <cpu/iss/include/exec/exec_inorder.hpp>
#include <cpu/iss/include/prefetch/prefetch_single_line.hpp>
#include <cpu/iss/include/gdbserver.hpp>
#include <cpu/iss/include/cores/ara/ara.hpp>

#if defined(CONFIG_GVSOC_ISS_SPATZ)
#include <cpu/iss/include/spatz.hpp>
#endif
#include <cpu/iss/include/cores/snitch_fast/ssr.hpp>
#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
#include <cpu/iss/include/cores/spatz/fpu_sequencer.hpp>
#else
#include <cpu/iss/include/cores/snitch_fast/sequencer.hpp>
#endif
#include <cpu/iss/include/cores/snitch_fast/fpu_lsu.hpp>

class IssWrapper;

#define ISS_INSN_FLAGS_VECTOR (1<< 0)



class Iss
{
public:
    Iss(IssWrapper &top);

    Exec exec;
    InsnCache insn_cache;
    Timing timing;
    Core core;
    SnitchRegfile regfile;
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
#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
    Vector vector;
    Ara ara;
#endif

    IssWrapper &top;

#if defined(CONFIG_GVSOC_ISS_SPATZ)
    Spatz spatz;
#endif

    Ssr ssr;
    Sequencer sequencer;
    FpuLsu fpu_lsu;

    CsrReg csr_fmode;


private:
    bool barrier_update(bool is_write, iss_reg_t &value);
    static void barrier_sync(vp::Block *__this, bool value);

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
    void stop();
    void insn_commit(PendingInsn *pending_insn);
    static iss_reg_t vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

    Iss iss;

private:
    PendingInsn &pending_insn_alloc();
    PendingInsn &pending_insn_enqueue(iss_insn_t *insn, iss_reg_t pc);

    vp::Trace trace;

    int max_pending_insn = 8;
    bool do_flush;
    std::vector<PendingInsn> pending_insns;
    int insn_first;
    int insn_last;
    int nb_pending_insn;
};

typedef struct
{
} SnitchInsnData;

static_assert(sizeof(SnitchInsnData) <= sizeof(iss_insn_t::data), "SnitchInsnData size exceeds iss_insn_t::data size");

static inline iss_reg_t fmode_get(Iss *iss, iss_insn_t *insn)
{
    return iss->csr_fmode.value;
}


#include "cpu/iss/include/isa/rv64i.hpp"
#include "cpu/iss/include/isa/rv32i.hpp"
#if defined(CONFIG_GVSOC_ISS_SPATZ)
#error 1
#include "cpu/iss/include/isa/rv32v.hpp"
#endif
#include "cpu/iss/include/isa/rv32c.hpp"
#include "cpu/iss/include/isa/zcmp.hpp"
#include "cpu/iss/include/isa/rv32a.hpp"

#if ISS_REG_WIDTH == 64
#include "cpu/iss/include/isa/rv64c.hpp"
#endif
#include "cpu/iss/include/isa/rv32m.hpp"
#include "cpu/iss/include/isa/rv64m.hpp"
#include "cpu/iss/include/isa/rv64a.hpp"
#include "cpu/iss/include/isa/rvd.hpp"
#include "cpu/iss/include/isa/rvf.hpp"
#include "cpu/iss/include/isa/rvXf16.hpp"
#include "cpu/iss/include/isa/rvXf16alt.hpp"
#include "cpu/iss/include/isa/rvXf16_switch.hpp"
#include "cpu/iss/include/isa/rvXf8.hpp"
#include "cpu/iss/include/isa/rvXf8alt.hpp"
#include "cpu/iss/include/isa/rvXf8_switch.hpp"
#include "cpu/iss/include/isa/rv32Xfvec.hpp"
#include "cpu/iss/include/isa/rv32Xfaux.hpp"
#include "cpu/iss/include/isa/priv.hpp"
#include <cpu/iss/include/isa/xdma.hpp>
#if !defined(CONFIG_GVSOC_ISS_USE_SPATZ)
#include "cpu/iss/include/isa/rv32frep.hpp"
#include "cpu/iss/include/isa/rv32ssr.hpp"
#endif

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
#include "cpu/iss/include/isa/rv32v_timed.hpp"
#endif

#include <cpu/iss/include/exec/exec_inorder_implem.hpp>
#include <cpu/iss/include/cores/snitch_fast/regfile_implem.hpp>
#if !defined(CONFIG_GVSOC_ISS_USE_SPATZ)
#include <cpu/iss/include/cores/snitch_fast/isa.hpp>
#endif
