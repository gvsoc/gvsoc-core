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


#include <vp/vp.hpp>
#include "iss.hpp"

void Iss::irq_check()
{
  this->trigger_check_all();
}


void Iss::wait_for_interrupt()
{
  // The instruction loop is checking for IRQs only if interrupts are globally enable
  // while wfi ends as soon as one interrupt is active even if interrupts are globally disabled,
  // so we have to check now if we can really go to sleep.
  if (this->cpu.irq.req_irq == -1)
  {
    wfi.set(true);
    iss_exec_insn_stall(this);
  }
}


void Iss::elw_irq_unstall()
{
  this->trace.msg(vp::trace::LEVEL_TRACE, "%s %d\n", __FILE__, __LINE__);

  this->trace.msg("Interrupting pending elw\n");
  this->cpu.current_insn = this->cpu.state.elw_insn;
  // Keep the information that we interrupted it, so that features like HW loop
  // knows that the instruction is being replayed
  this->cpu.state.elw_interrupted = 1;
}


void Iss::irq_req_sync(void *__this, int irq)
{
  Iss *_this = (Iss *)__this;

  _this->trace.msg(vp::trace::LEVEL_TRACE, "Received IRQ (irq: %d)\n", irq);

  _this->cpu.irq.req_irq = irq;

  if (irq != -1 && _this->wfi.get())
  {
    _this->wfi.set(false);
    _this->stalled_dec();
    iss_exec_insn_terminate(_this);
    iss_irq_check(_this);
  }

  if (_this->elw_stalled.get() && irq != -1 && _this->cpu.irq.irq_enable)
  {
    _this->elw_irq_unstall();
  }

  _this->trigger_check_all();
}
