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
    this->event = this->top.event_new((vp::Block *)this, handle_event);
    
    this->top.new_master_port("acc_req_ready", &this->acc_req_ready_itf);

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
// Check whether the subsystem is busy or idle.
bool Iss::check_state(iss_insn_t *insn)
{
    // Hierarchical instruction scoreboard, 
    // only stall the current floating point instruction if the previous one is waiting for offloading.
    // 1. fp instruction: get successful handshaking and continue.
    // 2. int instruction: continue without check, except data dependency, some int operands depend on previous fp result.
    // And also int instruction will be stalled if previous fp is still waiting to be offloaded.

    this->check_req.init();

    // Set the request is_write bit to differentiate whether it's a sequenceable instruction.
    // This is important because the handshaking methods and instruction lanes in the sequencer are different. 
    if (insn->isn_seq_op)
    {
        // Instructions in bypass lane
        this->check_req.set_is_write(false);
    }
    else
    {
        // Sequencable instructions
        this->check_req.set_is_write(true);
    }

    int idle = this->acc_req_ready_itf.req(&this->check_req);

    if (idle == vp::IO_REQ_OK)
    {
        return true;
    }
    else if (idle == vp::IO_REQ_INVALID)
    {
        return false;
    }
}


// This get called when there is floating-point instruction detected in decode stage.
// Maybe change output type from void to bool later if inserting scoreboard.
bool Iss::handle_req(iss_insn_t *insn, iss_reg_t pc, bool is_write)
{
    iss_opcode_t opcode = insn->opcode;
    this->trace_iss.msg("Send offload request (opcode: 0x%lx, pc: 0x%lx)\n", opcode, pc);
    // this->trace_iss.msg("Send offload request (insn: %p)\n", insn);
    
    // Assign arguments to request.
    this->insn = *((iss_insn_t *)insn);
    this->pc = pc;
    this->is_write=is_write;
    this->frm = this->csr.fcsr.frm;

    // Assign arguments to request.
    this->acc_req = { .pc=pc, .insn=this->insn, .is_write=is_write, .frm =frm };

    // Offload request if the port is connected
    if (this->acc_req_itf.is_bound())
    {
        this->acc_req_itf.sync(&this->acc_req);
    }
    // Todo: Increment the latency going through i_spill_register_acc_demux_req.
    // if (this->event->is_enqueued())
    // {
    //     this->event->enqueue(1);
    // }

    insn->handler = iss_decode_pc_handler;
    insn->fast_handler = iss_decode_pc_handler;

    // If the output register of fp instruction is integer type, set timestamp of scoreboard at integer regfile in integer core.
    // And the following instruction with data dependency will execute scoreboard_reg_check when loading the operands,
    // which will call stall_load_dependency_account.
    // Assign timestamp as the lower bound of latency, which will be updated to the exact value after the instruction is executed.
    if (!this->insn.out_regs_fp[0])
    {
    #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        this->regfile.scoreboard_reg_set_timestamp(insn->out_regs[0], this->top.clock.get_cycles() + insn->latency);
    #endif   
    }

    return false;
}


// This gets called when the offloaded instruction go through spill register.
void Iss::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *_this = (Iss *)__this;

    // Assign arguments to request.
    _this->acc_req = { .pc=_this->pc, .insn=_this->insn, .is_write=_this->is_write, .frm =_this->frm };

    // Offload request if the port is connected
    if (_this->acc_req_itf.is_bound())
    {
        _this->acc_req_itf.sync(&_this->acc_req);
    }
}


// This get called when it receives result from subsystem.
void Iss::handle_result(vp::Block *__this, OffloadRsp *result)
{
    Iss *_this = (Iss *)__this;

    // Get input information for trace
    iss_trace_dump_in(_this, &result->insn, result->pc);

    // Set scoreboard valid when the instruction finishes execution.
    if (!result->insn.out_regs_fp[0])
    {
        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        _this->regfile.scoreboard_reg_valid[result->insn.out_regs[0]] = true;
        #endif   
    }

    // Post-processing of result.
    // Update fflags in integer core after fp instruction.
    // Hardward doesn't stall CSR_FFLAGS when the fp instruction hasn't responded correct status value.
    _this->csr.fcsr.fflags = result->fflags;

    // Pass values of floating point registers from subsystem to core side.
    // Assign results here because we use fp register value only for trace, not for later computation.
    for (int i=0; i<ISS_NB_REGS; i++)
    {
        _this->regfile.set_freg(i, *((iss_freg_t *)(result->insn.freg_addr)+i));
    }

    // Output instruction trace for debugging.
    _this->trace_iss.msg("Get accelerator response (opcode: 0x%lx, pc: 0x%lx)\n", result->insn.opcode, result->pc);
    iss_trace_dump(_this, &result->insn, result->pc);
    
}