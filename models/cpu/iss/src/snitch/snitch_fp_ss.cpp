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
    this->fp_ss = true;
    this->acc_req_ready = true;
    this->top.traces.new_trace("accelerator", &this->trace_iss, vp::DEBUG);


    // -----------USE MASTER AND SLAVE PORT TO HANDLE OFFLOAD REQUEST------------------
    this->event = this->top.event_new((vp::Block *)this, handle_event);

    this->acc_req_itf.set_sync_meth(&Iss::handle_notif);
    this->top.new_slave_port("acc_req", &this->acc_req_itf, (vp::Block *)this);

    this->top.new_master_port("acc_rsp", &this->acc_rsp_itf, (vp::Block *)this);

    this->acc_req_ready_itf.set_req_meth(&Iss::rsp_state);
    this->top.new_slave_port("acc_req_ready", &this->acc_req_ready_itf, (vp::Block *)this);

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
// This gets called when it receives request from master side.
void Iss::handle_notif(vp::Block *__this, OffloadReq *req)
{
    Iss *_this = (Iss *)__this;

    // The subsystem is busy upon receiving a new request.
    _this->acc_req_ready = false;
    
    // Obtain arguments from request.
    iss_reg_t pc = req->pc;
    bool isRead = !req->is_write;
    iss_insn_t *insn = req->insn;
    unsigned int frm = req->frm;
    iss_opcode_t opcode = insn->opcode;
    insn->freg_addr = &_this->regfile.fregs[0];
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Received IO request (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);

    // Change address of input and ouput floating point registers.
    int rd = insn->out_regs[0];
    int rs1 = insn->in_regs[0];
    int rs2 = insn->in_regs[1];
    int rs3 = insn->in_regs[2];
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "rd index: %d, rs1 index: %d, rs2 index: %d, rs3 index: %d\n", rd, rs1, rs2, rs3);
    if (insn->in_regs_fp[0])
    {
        insn->in_regs_ref[0] = _this->regfile.freg_store_ref(rs1);
    }
    if (insn->in_regs_fp[1])
    {
        insn->in_regs_ref[1] = _this->regfile.freg_store_ref(rs2);
    }
    if (insn->in_regs_fp[2])
    {
        insn->in_regs_ref[2] = _this->regfile.freg_store_ref(rs3);
    }
    if (insn->out_regs_fp[0])
    {
        insn->out_regs_ref[0] = _this->regfile.freg_store_ref(rd);
    }

    // Update all integer register values from master side to subsystem side, 
    // useful when one of the input operand is integer register.
    // _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Update integer regfile from address: 0x%llx\n", insn->reg_addr);
    for (int i=0; i<ISS_NB_REGS; i++)
    {
        _this->regfile.set_reg(i, *((iss_reg_t *)(insn->reg_addr)+i));
    }
    // Or only assign operands of needed register
    // Rewrite value of data_arga, because there's WAR in integer register if the sequencer is added.
    if (!insn->in_regs_fp[0])
    {
        _this->regfile.set_reg(rs1, insn->data_arga); 
    }

    // The following part is not needed because the fp instruction starts execution only if all previous integer instruction has been finished.
    // If the input register of fp instruction is integer type, set timestamp of scoreboard at integer regfile in subsystem.
    // Add input register scoreboard check instead of writing in each exec handler function.
    // REG_GET() function in each handler will deal with latency by calling stall_load_dependency_account() function in iss->regfile.get_reg.
    // Floating point register dependency doesn't need to be checked, because current instruction starts executing only if the previous one finishes (in-order).
    /*
    if (!insn->in_regs_fp[0])
    {
        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        _this->regfile.scoreboard_reg_set_timestamp(rs1, insn->scoreboard_reg_timestamp_addr[rs1]);
        #endif
    }
    if (!insn->in_regs_fp[1])
    {
        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        _this->regfile.scoreboard_reg_set_timestamp(rs2, insn->scoreboard_reg_timestamp_addr[rs2]);
        #endif
    }
    if (!insn->in_regs_fp[2])
    {
        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        _this->regfile.scoreboard_reg_set_timestamp(rs3, insn->scoreboard_reg_timestamp_addr[rs3]);
        #endif
    }
    */

    // Update csr frm in subsystem
    _this->csr.fcsr.frm = frm;

    // Execute the instruction, and all the results(int/fp) are stored in subsystem regfile.
    // Use sync computing, execute instruction immediately and give back response after certain latency.
    _this->exec.insn_exec(insn, pc);

    // Assign dynamic latency to instruction latency if there's no static latency.
    // Or take MAX(insn->latency, stall_cycle_get())
    if(!insn->latency)
    {
        insn->latency = _this->exec.instr_event.stall_cycle_get();
    }

    // Put the instruction execution part into event queue,
    // in order to realize the parallelism between master and slave.
    // Todo: Specify latency here as the latency of instruction.
    _this->acc_req.pc = pc;
    _this->acc_req.insn = insn;
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Enqueue the offloaded instruction\n");
    // Todo: Add insn->latency after stall_insn_dependency_account in corresponding handler function.
    if (!_this->event->is_enqueued())
    {
        _this->event->enqueue(insn->latency);
    }
    // _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Offloaded instruction in event queue\n");

    // Assign int register output to input regfile in integer core.
    // Todo: differentiate between reg and reg64
    iss_reg_t data;
    if (!insn->out_regs_fp[0])
    {
        data = _this->regfile.get_reg(rd);
        *((iss_reg_t *)(insn->reg_addr)+rd) = data;

        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        insn->scoreboard_reg_timestamp_addr[rd] = _this->top.clock.get_cycles() + insn->latency;
        #endif   
    }

    // Update fflags in integer core after fp instruction.
    // *insn->fflags_addr = _this->csr.fcsr.fflags;

}

// This gets called when the offloaded instruction needs to be executed.
void Iss::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *_this = (Iss *)__this;

    _this->acc_req_ready = true;
    // _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Start handling event\n");
    
    iss_reg_t pc = _this->acc_req.pc;
    bool isRead = !_this->acc_req.is_write;
    iss_insn_t *insn = _this->acc_req.insn;
    iss_opcode_t opcode = insn->opcode;

    bool error;
    int rd;
    iss_reg_t data = 0;
    unsigned int fflags = 0;

    if (insn == NULL)
    {
        error = true;
        rd = -1;
    }

    error = false;
    rd = insn->out_regs[0];
    // Update integer regfile at the master side after calculation,
    // assign integer register value from subsystem to integer core.
    // Only update the regfile in integer core if the output register is int type.
    // for (int i=0; i<ISS_NB_REGS; i++)
    // {
    //     _this->regfile.set_reg(i, *((iss_reg_t *)(insn->reg_addr)+i));
    // }

    // Update register index for trace if it's an instruction under frep configuration.
    // int nb_args = insn->decoder_item->u.insn.nb_args;
    // for (int i = 0; i < nb_args; i++)
    // {
    //     iss_decoder_arg_t *arg = &insn->decoder_item->u.insn.args[i];
    //     iss_insn_arg_t *insn_arg = &insn->args[i];
    //     if ((arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG) && (insn_arg->u.reg.index != 0 || arg->flags & ISS_DECODER_ARG_FLAG_FREG))
    //     {
    //         if (arg->type == ISS_DECODER_ARG_TYPE_OUT_REG)
    //         {
    //             _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "arg->u.reg.index: %d\n", arg->u.reg.id);
    //             insn_arg->u.reg.index = insn->out_regs[arg->u.reg.id];
    //         }
    //         else if (arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
    //         {
    //             _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "arg->u.reg.index: %d\n", arg->u.reg.id);
    //             insn_arg->u.reg.index = insn->in_regs[arg->u.reg.id];
    //         }
    //         _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "insn_arg->u.reg.index: %d\n", insn_arg->u.reg.index);
    //     }
    // }

    // Output the instruction trace.
    iss_trace_dump(_this, insn, pc);
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Finish offload IO request (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);
     
    // Assign arguments to result.
    // Todo: assign value for data if the output register is integer register.
    data = _this->regfile.get_reg(rd);
    fflags = _this->csr.fcsr.fflags;
    _this->acc_rsp = { .rd=rd, .error=error, .data=data, .fflags=fflags, .pc=pc, .insn=insn };
    
    // Send back response of the result
    if (_this->acc_rsp_itf.is_bound())
    {
        _this->acc_rsp_itf.sync(&_this->acc_rsp);
    }

    // Clear stalling cycle amount for the next instruction.
    _this->exec.instr_event.stall_cycle_set(0);

}

// Get called for handshanking and respond if the subsystem is busy or not
vp::IoReqStatus Iss::rsp_state(vp::Block *__this, vp::IoReq *req)
{
    Iss *_this = (Iss *)__this;
    bool acc_req_ready = _this->acc_req_ready;

    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Subsystem side acceleration request handshaking signal: %d\n", acc_req_ready);

    if (acc_req_ready)
    {
       return vp::IO_REQ_OK; 
    }
    else
    {
        return vp::IO_REQ_INVALID;
    }
}