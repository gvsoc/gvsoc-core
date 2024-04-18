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
      gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(top, *this), trace(*this), csr(*this),
      regfile(*this), mmu(*this), pmp(*this), exception(*this), ssr(*this), top(top)
#if defined(CONFIG_GVSOC_ISS_INC_SPATZ)
      , spatz(*this)
#endif
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
    iss_insn_t insn = req->insn;
    unsigned int frm = req->frm;
    iss_opcode_t opcode = insn.opcode;
    insn.freg_addr = &_this->regfile.fregs[0];
    _this->trace_iss.msg("Received IO request (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);

    // Change address of input and ouput floating point registers.
    int rd = insn.out_regs[0];
    int rs1 = insn.in_regs[0];
    int rs2 = insn.in_regs[1];
    int rs3 = insn.in_regs[2];
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "rd index: %d, rs1 index: %d, rs2 index: %d, rs3 index: %d\n", rd, rs1, rs2, rs3);
    if (insn.in_regs_fp[0])
    {
        insn.in_regs_ref[0] = _this->regfile.freg_store_ref(rs1);
    }
    if (insn.in_regs_fp[1])
    {
        insn.in_regs_ref[1] = _this->regfile.freg_store_ref(rs2);
    }
    if (insn.in_regs_fp[2])
    {
        insn.in_regs_ref[2] = _this->regfile.freg_store_ref(rs3);
    }
    if (insn.out_regs_fp[0])
    {
        insn.out_regs_ref[0] = _this->regfile.freg_store_ref(rd);
    }

    // Update all integer register values from master side to subsystem side, 
    // useful when one of the input operand is integer register.
    for (int i=0; i<ISS_NB_REGS; i++)
    {
        _this->regfile.set_reg(i, *((iss_reg_t *)(insn.reg_addr)+i));
    }
    // Or only assign operands of needed register
    // Rewrite value of data_arga, because there's WAR in integer register if the sequencer is added.
    if (!insn.in_regs_fp[0])
    {
        _this->regfile.set_reg(rs1, insn.data_arga); 
    }

    // Update csr frm in subsystem
    _this->csr.fcsr.frm = frm;

    // Get input information for trace
    if (!_this->ssr.ssr_enable)
    {
        iss_trace_dump_in(_this, &insn, pc);
    }

    // Execute the instruction, and all the results(int/fp) are stored in subsystem regfile.
    // Use sync computing, execute instruction immediately and give back response after certain latency.
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Execute instruction and accumulate latency\n");
    _this->trace.priv_mode = _this->core.mode_get();
    _this->exec.insn_exec(&insn, pc);

    // Update loop counter if SSR enabled (work under no dm_event)
    _this->ssr.update_ssr();
    // Clear dm_read and dm_write flag after execution
    _this->ssr.clear_flags();

    // Assign dynamic latency to instruction latency if there's no static latency.
    // Or take MAX(insn->latency, stall_cycle_get())
    // If SSR is enabled, the latency of instruction includes memory access + execution time in fpu.
    if (_this->ssr.ssr_enable)
    {
        insn.latency += _this->exec.stall_cycles;
    }
    // If SSR is disabled, the instruction is either memory access or arithmetic instruction.
    if (insn.latency < _this->exec.stall_cycles)
    {
        insn.latency = _this->exec.stall_cycles;
    }
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Total latency of fp instruction (opcode: 0x%llx, pc: 0x%llx, latency: %d)\n", opcode, pc, insn.latency);

    // Get real latency considering pipeline stages in FPU.
    int final_latency = _this->get_latency(insn, pc, _this->top.clock.get_cycles());
    // Update the pipeline FIFOs.
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Calculate latency considering FPU pipeline: %d\n", final_latency);
    insn.latency = final_latency;

    // Regard the instruction execution part into event queue,
    // in order to realize the parallelism between master and slave.
    _this->acc_req.pc = pc;
    _this->acc_req.insn = insn;
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Enqueue the finished offloaded instruction\n");
    // Todo: Add insn.latency after stall_insn_dependency_account in corresponding handler function.
    if (!_this->event->is_enqueued())
    {
        _this->event->enqueue(insn.latency);
    }

    // Update integer regfile at the master side after calculation,
    // Assign int register output to input regfile in integer core.
    // Todo: differentiate between reg and reg64
    iss_reg_t data;
    if (!insn.out_regs_fp[0])
    {
        data = _this->regfile.get_reg(rd);
        *((iss_reg_t *)(insn.reg_addr)+rd) = data;

        #if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        insn.scoreboard_reg_timestamp_addr[rd] = _this->top.clock.get_cycles() + insn.latency;
        #endif   
    }

    // Update fflags in integer core after fp instruction.
    // *insn.fflags_addr = _this->csr.fcsr.fflags;

}

// This gets called when the offloaded instruction needs to be executed.
void Iss::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *_this = (Iss *)__this;

    _this->acc_req_ready = true;
    
    iss_reg_t pc = _this->acc_req.pc;
    bool isRead = !_this->acc_req.is_write;
    iss_insn_t insn = _this->acc_req.insn;
    iss_opcode_t opcode = insn.opcode;

    bool error;
    int rd;
    iss_reg_t data = 0;
    unsigned int fflags = 0;

    if (insn.opcode == 0)
    {
        error = true;
        rd = -1;
    }

    error = false;
    rd = insn.out_regs[0];

    // Output the instruction trace for output information.
    // When SSR is enabled, trace of input data need special handler, because we know the operands after dm_read or dm_write.
    if (_this->ssr.ssr_enable)
    {
        iss_trace_dump_in(_this, &insn, pc);
    }
    iss_trace_dump(_this, &insn, pc);
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Finish offload IO request (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);
     
    // Assign arguments to result.
    data = _this->regfile.get_reg(rd);
    fflags = _this->csr.fcsr.fflags;
    _this->acc_rsp = { .rd=rd, .error=error, .data=data, .fflags=fflags, .pc=pc, .insn=insn };
    
    // Send back response of the result
    if (_this->acc_rsp_itf.is_bound())
    {
        _this->acc_rsp_itf.sync(&_this->acc_rsp);
    }

    // Clear stalling cycle amount for the next instruction.
    _this->exec.stall_cycles = 0;

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

// Get called when we need to calculate the latency of instruction after considering pipeline registers in fpu.
int Iss::get_latency(iss_insn_t insn, iss_reg_t pc, int timestamp)
{
    int rs1 = insn.in_regs[0];
    int rs2 = insn.in_regs[1];
    int rs3 = insn.in_regs[2];
    if (!insn.in_regs_fp[0])
    {
        rs1 = -1;
    }
    if (!insn.in_regs_fp[1])
    {
        rs2 = -1;
    }
    if (!insn.in_regs_fp[2])
    {
        rs3 = -1;
    }

    // Determine operation groups dependent on the label of instruction
    bool fma_label = strstr(insn.decoder_item->u.insn.label, "add")
                    || strstr(insn.decoder_item->u.insn.label, "sub")
                    || strstr(insn.decoder_item->u.insn.label, "mul")
                    || strstr(insn.decoder_item->u.insn.label, "mac");

    bool divsqrt_label = strstr(insn.decoder_item->u.insn.label, "div")
                        || strstr(insn.decoder_item->u.insn.label, "sqrt");

    bool noncomp_label = strstr(insn.decoder_item->u.insn.label, "sgnj")
                        || strstr(insn.decoder_item->u.insn.label, "min")
                        || strstr(insn.decoder_item->u.insn.label, "max");

    bool conv_label = strstr(insn.decoder_item->u.insn.label, "fmv")
                    || strstr(insn.decoder_item->u.insn.label, "fcvt")
                    || strstr(insn.decoder_item->u.insn.label, "fcpka")
                    || strstr(insn.decoder_item->u.insn.label, "feq")
                    || strstr(insn.decoder_item->u.insn.label, "fle")
                    || strstr(insn.decoder_item->u.insn.label, "flt")
                    || strstr(insn.decoder_item->u.insn.label, "class");

    bool dotp_label = strstr(insn.decoder_item->u.insn.label, "dotp")
                    || strstr(insn.decoder_item->u.insn.label, "sum");

    bool lsu_label = strstr(insn.decoder_item->u.insn.label, "flw")
                    || strstr(insn.decoder_item->u.insn.label, "fsw")
                    || strstr(insn.decoder_item->u.insn.label, "fld")
                    || strstr(insn.decoder_item->u.insn.label, "fsd");

    int latency = insn.latency;
    int latency_pipe = 0;

    // Check status of all operation FIFOs and get the real pipelined latency.
    latency_pipe = this->FMA_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }
    latency_pipe = this->DIVSQRT_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }
    latency_pipe = this->NONCOMP_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }
    latency_pipe = this->CONV_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }
    latency_pipe = this->DOTP_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }
    latency_pipe = this->LSU_OPGROUP.get_latency(timestamp, insn.latency, rs1, rs2, rs3);
    if (latency_pipe < latency)
    {
        latency = latency_pipe;
    }

    // Store finished instruction as new entry inside its own operation group FIFO
    int rd = insn.out_regs[0];
    if (!insn.out_regs_fp[0])
    {
        rd = -1;
    }

    FpuOp entry = { .pc=pc, .insn_latency=insn.latency, .start_timestamp=timestamp, .end_timestamp=timestamp+latency, .rd=rd };

    // Store the new instruction execution information in its own operation group fifo.
    if (fma_label)
    {
        this->FMA_OPGROUP.enqueue(entry);
    }
    else if (divsqrt_label)
    {
        this->DIVSQRT_OPGROUP.enqueue(entry);
    }
    else if (noncomp_label)
    {
        this->NONCOMP_OPGROUP.enqueue(entry);
    }
    else if (conv_label)
    {
        this->CONV_OPGROUP.enqueue(entry);
    }
    else if (dotp_label)
    {
        this->DOTP_OPGROUP.enqueue(entry);
    }
    else if (lsu_label)
    {
        this->LSU_OPGROUP.enqueue(entry);
    }

    return latency;
}