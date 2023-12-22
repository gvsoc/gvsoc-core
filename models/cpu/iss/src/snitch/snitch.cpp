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

#include "cpu/iss/include/iss.hpp"
#include <string.h>


Iss::Iss(IssWrapper &top)
    : prefetcher(*this), exec(top, *this), insn_cache(*this), decode(*this), timing(*this), core(*this), irq(*this),
      gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(*this), trace(*this), csr(*this),
      regfile(*this), mmu(*this), pmp(*this), exception(*this), spatz(*this), top(top)
{
    this->csr.declare_csr(&this->barrier,  "barrier",   0x7C2);
    this->barrier.register_callback(std::bind(&Iss::barrier_update, this, std::placeholders::_1,
        std::placeholders::_2));

    this->barrier_ack_itf.set_sync_meth(&Iss::barrier_sync);
    this->top.new_slave_port("barrier_ack", &this->barrier_ack_itf, (vp::Block *)this);

    this->top.new_master_port("barrier_req", &this->barrier_req_itf, (vp::Block *)this);


    this->snitch = true;
    this->fp_ss = false;
    this->top.traces.new_trace("offload", &this->trace_iss, vp::DEBUG);


    // -----------USE MASTER AND SLAVE PORT TO HANDLE OFFLOAD REQUEST------------------
    this->top.new_master_port("acc_req", &this->acc_req_itf, (vp::Block *)this);

    this->acc_rsp_itf.set_sync_meth(&Iss::handle_result);
    this->top.new_slave_port("acc_rsp", &this->acc_rsp_itf, (vp::Block *)this);

}


// This gets called when the barrier csr is accessed
bool Iss::barrier_update(bool is_write, iss_reg_t &value)
{
    if (!is_write)
    {
        // Since syncing the barrier can immediatly trigger it, we need
        // to flag that we are waiting fro the barrier.
        this->waiting_barrier = true;

        // Notify the global barrier
        if (this->barrier_req_itf.is_bound())
        {
            this->barrier_req_itf.sync(1);
        }

        // Now we have to check if the barrier was already reached and the barrier
        // sync function already called.
        if (this->waiting_barrier)
        {
            // If not, stall the core, this will get unstalled when the barrier sync is called
            this->exec.insn_stall();
        }
    }

    return false;
}


// This gets called when the barrier is reached
void Iss::barrier_sync(vp::Block *__this, bool value)
{
    Iss *_this = (Iss *)__this;

    // Clear the flag
    _this->waiting_barrier = false;

    // In case the barrier is reached as soon as we notify it, we are not yet stalled
    // and should not to anything
    if (_this->exec.is_stalled())
    {
        // Otherwise unstall the core
        _this->exec.stalled_dec();
        _this->exec.insn_terminate();
    }
}


// -----------USE MASTER AND SLAVE PORT TO HANDLE OFFLOAD REQUEST------------------
// This get called when there is floating-point instruction detected in decode stage.
// Maybe change output type from void to bool later if inserting scoreboard.
void Iss::handle_req(iss_insn_t *insn, iss_reg_t pc, bool is_write)
{
    iss_opcode_t opcode = insn->opcode;
    this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Offload request (opcode: 0x%lx, pc: 0x%lx)\n", opcode, pc);

    // Assign arguments to request.
    this->acc_req = { .pc=pc, .insn=insn, .is_write=is_write };
    
    // Todo: currently stall anyway until the accelerator sends back result.
    this->acc_stall = true;

    // Offload request if the port is connected
    if (this->acc_req_itf.is_bound())
    {
        this->acc_req_itf.sync(&this->acc_req);
    }

    // Todo: Check if the subsystem side accept the offloaded instruction.
    // If there's a accelerator stall, stop the current event queue at the master side.
    if (this->acc_stall)
    {
        // If acc_stall, stall the core, this will get unstalled when the response (handle_result) is called
        this->exec.insn_stall();
    }
}

// This get called when it receives result from subsystem.
void Iss::handle_result(vp::Block *__this, OffloadRsp *result)
{
    Iss *_this = (Iss *)__this;

    // Todo: Might need post-processing of result.

    // Pass values of floating point registers from subsystem to core side.
    for (int i=0; i<ISS_NB_REGS; i++)
    {
        _this->regfile.set_freg(i, *((iss_freg_t *)(_this->acc_req.insn->freg_addr)+i));
    }

    // Output instruction trace for debugging.
    // iss_trace_dump(_this, _this->acc_req.insn, _this->acc_req.pc);
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Get accelerator response\n");

    // Clear the stall flag.
    _this->acc_stall = false;

    // Unstall the core when we receive the response.
    if (_this->exec.is_stalled())
    {
        _this->exec.stalled_dec();
        _this->exec.insn_terminate();
    }
}