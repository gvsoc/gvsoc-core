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


Iss::Iss(vp::Component &top)
    : prefetcher(*this), exec(*this), decode(*this), timing(*this), core(*this), irq(*this),
      gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(*this), trace(*this), csr(*this),
      regfile(*this), mmu(*this), pmp(*this), exception(*this), spatz(*this), top(top)
// Iss::Iss(vp::Component &top)
//     : exec(*this), decode(*this), timing(*this), core(*this), irq(*this),
//       gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(*this), trace(*this), csr(*this),
//       regfile(*this), mmu(*this), pmp(*this), exception(*this), spatz(*this), top(top)
{
    this->csr.declare_csr(&this->barrier,  "barrier",   0x7C2);
    this->barrier.register_callback(std::bind(&Iss::barrier_update, this, std::placeholders::_1,
        std::placeholders::_2));

    this->barrier_ack_itf.set_sync_meth(&Iss::barrier_sync);
    this->top.new_slave_port("barrier_ack", &this->barrier_ack_itf, (vp::Block *)this);

    this->top.new_master_port("barrier_req", &this->barrier_req_itf, (vp::Block *)this);


    this->snitch = true;
    this->fp_ss = true;

    this->top.traces.new_trace("accelerator", &this->trace_iss, vp::DEBUG);
    this->acc_rsp_itf.set_req_meth(&Iss::acc_request);
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


int Iss::send_acc_req(iss_insn_t *insn, iss_reg_t pc, bool is_write)
{
    return 0;
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


vp::IoReqStatus Iss::acc_request(vp::Block *__this, vp::IoReq *req)
{
    Iss *_this = (Iss *)__this;
    vp::IoReqStatus result;
    
    iss_reg_t pc = req->get_addr();
    bool isRead = !req->get_is_write();
    iss_insn_t *insn = req->get_insn();
    iss_opcode_t opcode = insn->opcode;
    insn->freg_addr = &_this->regfile.fregs[0];
    
    if (insn == NULL)
    {
        result = vp::IO_REQ_INVALID;
    }

    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Received IO req (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);

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

    _this->exec.insn_exec(insn, pc);

    for (int i=0; i<ISS_NB_REGS; i++)
    {
        _this->regfile.set_reg(i, *((iss_reg_t *)(insn->reg_addr)+i));
    }

    // _this->trace.priv_mode = _this->core.mode_get();
    // iss_trace_save_args(_this, insn, _this->trace.saved_args, false);
    iss_trace_dump(_this, insn, pc);
    _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Finish offload IO req (opcode: 0x%llx, pc: 0x%llx, isRead: %d)\n", opcode, pc, isRead);
    
    result = vp::IO_REQ_OK;
    
    return result;
}


// void Iss::handle_acc(vp::Block *__this, vp::ClockEvent *event)
// {
//     Iss *_this = (Iss *)__this;

//     _this->trace_iss.msg(vp::Trace::LEVEL_TRACE, "Handle acc request\n");

// }