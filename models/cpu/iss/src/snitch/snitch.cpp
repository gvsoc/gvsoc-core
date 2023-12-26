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
    this->event = this->top.event_new((vp::Block *)this, handle_event);

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
bool Iss::handle_req(iss_insn_t *insn, iss_reg_t pc, bool is_write)
{
    iss_opcode_t opcode = insn->opcode;
    this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Offload request (opcode: 0x%lx, pc: 0x%lx)\n", opcode, pc);
    
    // Todo: Check if the subsystem side accept the offloaded instruction.
    // The fp instruction can be offloaded when the accelerator queue is empty.
    // If there's a accelerator stall, stop the current event queue at the master side.
    // Use insn_stall() or insn_hold&insn_resume()
    // 1. insn_stall(): stall and then start from the stall point, choose this way here because we don't want to 
    // resume back to instr_event and start from next instruction directly.
    // 2. insn_hold&insn_resume(): use meth function to solve the remaining execution and then return back to next instruction directly.
    this->acc_stall = true;
    if (this->acc_stall)
    {
        // If acc_stall, stall the core, this will get unstalled when the event (handle_event) is called
        this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Stall integer core event queue\n");
        this->exec.insn_stall();
    }

    // Todo: currently stall when last fp instruction has been offloaded.
    // Hierarchical instruction scoreboard, 
    // only stall the current floating point instruction if the fp subsystem hasn't finished the previous one 
    // (This situation happens when the fp instruction latency greater than zero).
    // Implement conditional acc_stall, stall fp and not stall int.
    // 1. fp instruction: get previous response and continue, compare timestamp 
    // of current global one and the previous/undergoing fp instruction.
    // 2. int instruction: continue without check, except data dependency,
    // some int operands depend on previous fp result.
    if (insn->is_fp_op)
    {
        // Compare timestamp
        int64_t diff = this->fp_past_timestamp - this->top.clock.get_cycles() - this->exec.instr_event.stall_cycle_get();
        if (unlikely(diff >= 0))
        {
            this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Stall current fp instruction for %d cycles\n", diff+1);

            // Assign arguments to request.
            this->insn = insn;
            this->pc = pc;
            this->is_write=is_write;

            // Update the latest past floating point instruction finishing time
            this->fp_past_timestamp = this->fp_past_timestamp + insn->latency + 1;
            this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Set fp timestamp for next instruction: %d\n", fp_past_timestamp );

            // Todo: write a function to stall the core for n cycles, and restart again from stall point by itself
            // Postpone offload happening n cycles later using enqueue.
            if (!this->event->is_enqueued())
            {
                this->event->enqueue(diff+1);
            }

            // Return true so that pc is not updated.
            // This also prevent the core from continuing to the next instruction.
            return true;
        }
        else
        {
            // For normal fp instruction with latency = NUM_PIPELINE_STAGE - 1.
            // Offload immediately
            if (!this->event->is_enqueued())
            {
                this->event->enqueue(0);
            }
        }
    }

    // Assign arguments to request variables.
    this->insn = insn;
    this->pc = pc;
    this->is_write=is_write;

    // Update the latest past floating point instruction finishing time
    this->fp_past_timestamp = this->top.clock.get_cycles() + insn->latency;
    this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Set fp timestamp for next instruction: %d\n", fp_past_timestamp );
    
    return false;
}


// This gets called when the offloaded instruction needs to be delayed.
void Iss::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *_this = (Iss *)__this;

    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Handling delayed instruction (opcode: 0x%lx, pc: 0x%lx)\n", _this->insn->opcode, _this->pc);

    // Assign arguments to request.
    _this->acc_req = { .pc=_this->pc, .insn=_this->insn, .is_write=_this->is_write };

    // Offload request if the port is connected
    if (_this->acc_req_itf.is_bound())
    {
        _this->acc_req_itf.sync(&_this->acc_req);
    }

    // Call the core to start the next instruction if the previous one has already been offloaded.
    // Clear the stall flag.
    _this->acc_stall = false;

    // Unstall the core when we receive the response.
    if (_this->exec.is_stalled())
    {
        _this->exec.stalled_dec();
    }
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Ready for the next instruction\n");
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
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Get accelerator response\n");
    iss_trace_dump(_this, _this->acc_req.insn, _this->acc_req.pc);
    
}