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

#ifndef __CPU_ISS_LSU_HPP
#define __CPU_ISS_LSU_HPP

#include "iss_core.hpp"

static inline void iss_lsu_load_resume(Iss *iss)
{
  // Nothing to do, the zero-extension was done by initializing the register to 0
}

static inline void iss_lsu_elw_resume(Iss *iss)
{
  // Clear pending elw to not replay it when the next interrupt occurs
  iss->cpu.state.elw_insn = NULL;
  iss->elw_stalled.set(false);
}

static inline void iss_lsu_load_signed_resume(Iss *iss)
{
  int reg = iss->cpu.state.stall_reg;
  iss_set_reg(iss, reg, iss_get_signed_value(iss_get_reg_untimed(iss, reg), iss->cpu.state.stall_size*8));
}

static inline void iss_lsu_load(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_set_reg(iss, reg, 0);
  if (!iss->data_req(addr, (uint8_t *)iss_reg_ref(iss, reg), size, false))
  {
    // We don't need to do anything as the target will write directly to the register
    // and we the zero extension is already managed by the initial value
  }
  else
  {
    iss->cpu.state.stall_callback = iss_lsu_load_resume;
    iss->cpu.state.stall_reg = reg;
  }
}

static inline void iss_lsu_elw(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_set_reg(iss, reg, 0);
  if (!iss->data_req(addr, (uint8_t *)iss_reg_ref(iss, reg), size, false))
  {
    // We don't need to do anything as the target will write directly to the register
    // and we the zero extension is already managed by the initial value
  }
  else
  {
    iss->cpu.state.stall_callback = iss_lsu_elw_resume;
    iss->cpu.state.stall_reg = reg;
  }
}

static inline void iss_lsu_load_signed(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  if (!iss->data_req(addr, (uint8_t *)iss_reg_ref(iss, reg), size, false))
  {
    iss_set_reg(iss, reg, iss_get_signed_value(iss_get_reg_untimed(iss, reg), size*8));
  }
  else
  {
    iss->cpu.state.stall_callback = iss_lsu_load_signed_resume;
    iss->cpu.state.stall_reg = reg;
    iss->cpu.state.stall_size = size;
  }
}

static inline void iss_lsu_store_resume(Iss *iss)
{
  // For now we don't have to do anything as the register was written directly
  // by the request but we cold support sign-extended loads here;
}

static inline void iss_lsu_store(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  if (!iss->data_req(addr, (uint8_t *)iss_reg_store_ref(iss, reg), size, true))
  {
    // For now we don't have to do anything as the register was written directly
    // by the request but we cold support sign-extended loads here;
  }
  else
  {
    iss->cpu.state.stall_callback = iss_lsu_store_resume;
    iss->cpu.state.stall_reg = reg;
  }
}

static inline void iss_lsu_load_perf(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_pccr_account_event(iss, CSR_PCER_LD, 1);
  iss_lsu_load(iss, insn, addr, size, reg);
}

static inline void iss_lsu_elw_perf(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_pccr_account_event(iss, CSR_PCER_LD, 1);
  iss_lsu_elw(iss, insn, addr, size, reg);
}

static inline void iss_lsu_load_signed_perf(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_pccr_account_event(iss, CSR_PCER_LD, 1);
  iss_lsu_load_signed(iss, insn, addr, size, reg);
}

static inline void iss_lsu_store_perf(Iss *iss, iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
  iss_pccr_account_event(iss, CSR_PCER_ST, 1);
  iss_lsu_store(iss, insn, addr, size, reg);
}

static inline void iss_lsu_check_stack_access(Iss *iss, int reg, iss_addr_t addr)
{
  // Could be optimized when decoding instruction by pointing to different instruction
  // handlers when register 2 is seen but the gain is small compared to the cost of the full
  // load.
  if (iss->cpu.csr.stack_conf && reg == 2)
  {
    if (addr < iss->cpu.csr.stack_start || addr >= iss->cpu.csr.stack_end)
    {
      iss->trace.fatal("SP-based access outside stack (addr: 0x%x, stack_start: 0x%x, stack_end: 0x%x)\n", addr, iss->cpu.csr.stack_start, iss->cpu.csr.stack_end);
    }
  }
}

void Iss::exec_misaligned(void *__this, vp::clock_event *event)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling second half of misaligned access\n");

  // As the 2 load accesses for misaligned access are generated by the
  // wrapper, we need to account the extra access here.
  _this->cpu.state.insn_cycles++;
  iss_pccr_account_event(_this, CSR_PCER_LD, 1);

  if (_this->data_req_aligned(_this->misaligned_addr, _this->misaligned_data,
    _this->misaligned_size, _this->misaligned_is_write) == vp::IO_REQ_OK)
  {
    _this->dump_trace_enabled = true;
    iss_exec_insn_terminate(_this);
    _this->cpu.state.insn_cycles += _this->io_req.get_latency() + 1;
    _this->trigger_check_all();
  }
  else
  {
    _this->trace.warning("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
  }
}

inline int Iss::data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{
  decode_trace.msg("Data request (addr: 0x%lx, size: 0x%x, is_write: %d)\n", addr, size, is_write);
  vp::io_req *req = &io_req;
  req->init();
  req->set_addr(addr);
  req->set_size(size);
  req->set_is_write(is_write);
  req->set_data(data_ptr);
  int err = data.req(req);
  if (err == vp::IO_REQ_OK) 
  {
    this->cpu.state.insn_cycles += req->get_latency();
    return 0;
  }
  else if (err == vp::IO_REQ_INVALID) 
  {
    vp_warning_always(&this->warning, "Invalid access (pc: 0x%" PRIxFULLREG ", offset: 0x%" PRIxFULLREG ", size: 0x%x, is_write: %d)\n", this->cpu.current_insn->addr, addr, size, is_write);
  }
  
  this->trace.msg(vp::trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
  iss_exec_insn_stall(this);
  return err;
}

#define ADDR_MASK (~(ISS_REG_WIDTH/8 - 1))

inline int Iss::data_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write)
{

  iss_addr_t addr0 = addr & ADDR_MASK;
  iss_addr_t addr1 = (addr + size - 1) & ADDR_MASK;

  if (likely(addr0 == addr1))
    return data_req_aligned(addr, data_ptr, size, is_write);
  else
    return data_misaligned_req(addr, data_ptr, size, is_write);
}

#endif
