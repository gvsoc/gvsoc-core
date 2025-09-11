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
#include <cpu/iss/include/cores/snitch/regfile.hpp>
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

#if defined(CONFIG_GVSOC_ISS_SPATZ)
#include <cpu/iss/include/spatz.hpp>
#endif

#include <cpu/iss/include/types.hpp>

#include "OffloadReq.hpp"
#include "OffloadRsp.hpp"
#include "PipeRegs.hpp"
#include <cpu/iss/include/cores/snitch/ssr.hpp>

class IssWrapper;

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

    IssWrapper &top;

#if defined(CONFIG_GVSOC_ISS_SPATZ)
    Spatz spatz;
#endif

    Ssr ssr;

    bool snitch;
    bool fp_ss;

    // -----------USE IO PORT TO HANDLE REDMULE------------------
    vp::IoMaster redmule_itf;
    vp::IoReq*   redmule_req;
    uint16_t     redmule_mnk_reg [4];
    uint32_t     redmule_xwy_reg [4];

    // -----------USE MASTER AND SLAVE PORT TO HANDLE OFFLOAD REQUEST------------------

    // Handshaking interface
    vp::IoSlave acc_req_ready_itf;

    // Request and Response interface
    vp::WireSlave<OffloadReq *> acc_req_itf;
    vp::WireMaster<OffloadRsp *> acc_rsp_itf;
    vp::ClockEvent *event;
    OffloadReq acc_req;
    OffloadRsp acc_rsp;

    // Handshaking signals and functions
    bool acc_req_ready;
    static vp::IoReqStatus rsp_state(vp::Block *__this, vp::IoReq *req);
    bool check_state(iss_insn_t *insn);

    // Handle request and response functions
    static void handle_notif(vp::Block *__this, OffloadReq *req);
    static void handle_event(vp::Block *__this, vp::ClockEvent *event);
    bool handle_req(iss_insn_t *insn, iss_reg_t pc, bool is_write);

    // Temporary variable to process RAW caused by SSR and accessing stack pointer,
    // for result is written to memory directly.
    iss_addr_t mem_map;
    iss_reg_t mem_pc;

    // Add operation groups and store intructions like a FIFO
    // to abstract behavior of pipeline. Most of fp instructions are pipelined in Snitch.
    // Operation groups in FPU
    FIFODepth3 FMA_OPGROUP;
    FIFODepth1 DIVSQRT_OPGROUP;
    FIFODepth1 NONCOMP_OPGROUP;
    FIFODepth2 CONV_OPGROUP;
    FIFODepth3 DOTP_OPGROUP;
    // Operation group in LSU
    FIFODepth1 LSU_OPGROUP;

    // Timing model of pipelined FPU
    int get_latency(iss_insn_t insn, iss_reg_t pc, int timestamp);
    void ssr_latency(int diff);

private:
    bool barrier_update(bool is_write, iss_reg_t &value);
    static void barrier_sync(vp::Block *__this, bool value);
    bool ssr_access(bool is_write, iss_reg_t &value);

    vp::WireMaster<bool> barrier_req_itf;
    vp::WireSlave<bool> barrier_ack_itf;
    CsrReg barrier;
    bool waiting_barrier;

    vp::Trace trace_iss;
    CsrReg csr_ssr;

};



class IssWrapper : public vp::Component
{

public:
    IssWrapper(vp::ComponentConf &config);

    void start();
    void reset(bool active);
    void stop();

    Iss iss;

    vp::Trace trace;
    int dummy;
private:
};

static inline iss_reg_t fmode_get(Iss *iss, iss_insn_t *insn)
{
    return insn->fmode;
}


#include "cpu/iss/include/isa/rv64i.hpp"
#include "cpu/iss/include/isa/rv32i.hpp"
#if defined(CONFIG_GVSOC_ISS_SPATZ)
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
#ifdef CONFIG_GVSOC_ISS_SNITCH_PULP_V2
#include "cpu/iss/include/isa/pulp_v2.hpp"
#else
#include <cpu/iss/include/isa/xdma.hpp>
#include <cpu/iss/include/isa/redmule.hpp>
#include "cpu/iss/include/isa/rv32frep.hpp"
#include "cpu/iss/include/isa/rv32ssr.hpp"
#endif


#include <cpu/iss/include/cores/snitch/regfile_implem.hpp>
#include <cpu/iss/include/cores/snitch_fp_ss/exec_inorder_implem.hpp>
