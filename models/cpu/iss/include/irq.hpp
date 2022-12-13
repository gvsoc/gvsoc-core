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

#ifndef __CPU_ISS_IRQ_HPP
#define __CPU_ISS_IRQ_HPP

static inline int iss_irq_check(Iss *iss)
{
  if (iss->cpu.irq.req_debug && !iss->cpu.state.debug_mode)
  {
    iss->cpu.state.debug_mode = true;
    iss->cpu.csr.depc = iss->cpu.current_insn->addr;
    iss->cpu.irq.debug_saved_irq_enable = iss->cpu.irq.irq_enable;
    iss->cpu.irq.irq_enable = 0;
    iss->cpu.irq.req_debug = false;
    iss->cpu.current_insn = iss->cpu.irq.debug_handler;
    return 1;
  }
  else
  {
    int req_irq = iss->cpu.irq.req_irq;
    if (req_irq != -1 && iss->cpu.irq.irq_enable)
    {
      iss->trace.msg(vp::trace::LEVEL_TRACE, "Handling IRQ (irq: %d)\n", req_irq);

      iss->cpu.csr.epc = iss->cpu.current_insn->addr;
      iss->cpu.irq.saved_irq_enable = iss->cpu.irq.irq_enable;
      iss->cpu.irq.irq_enable = 0;
      iss->cpu.irq.req_irq = -1;
      iss->cpu.current_insn = iss->cpu.irq.vectors[req_irq];
      iss->cpu.csr.mcause = (1<<31) | (unsigned int)req_irq;

      iss->decode_trace.msg("Acknowledging interrupt (irq: %d)\n", req_irq);
      iss->irq_ack_itf.sync(req_irq);

      iss_perf_account_dependency_stall(iss, 4);

      prefetcher_fetch(iss, iss->cpu.current_insn);

      return 1;
    }
  }

  return 0;
}



static inline iss_insn_t *iss_irq_handle_mret(Iss *iss)
{
  iss->trigger_check_all();
  iss->cpu.irq.irq_enable = iss->cpu.irq.saved_irq_enable;
  iss->cpu.csr.mcause = 0;

  return insn_cache_get(iss, iss->cpu.csr.epc);

}



static inline iss_insn_t *iss_irq_handle_dret(Iss *iss)
{
  iss->trigger_check_all();
  iss->cpu.irq.irq_enable = iss->cpu.irq.debug_saved_irq_enable;
  iss->cpu.state.debug_mode = 0;

  return insn_cache_get(iss, iss->cpu.csr.depc);

}



static inline void iss_irq_enable(Iss *iss, int enable)
{
  iss->trace.msg(vp::trace::LEVEL_TRACE, "Setting IRQ enable (value: %d)\n",
    enable);

  iss->cpu.irq.irq_enable = enable;
  iss->irq_check();
}

static inline void iss_irq_req(Iss *iss, int irq)
{
  iss->cpu.irq.req_irq = irq;
}

static inline void iss_irq_set_vector_table(Iss *iss, iss_addr_t base)
{
  iss->trace.msg("Setting vector table (addr: 0x%x)\n", base);
  for (int i=0; i<32; i++)
  {
    iss->cpu.irq.vectors[i] = insn_cache_get(iss, base + i * 4);
  }

  for (int i=32; i<35; i++)
  {
    iss->cpu.irq.vectors[i] = insn_cache_get(iss, base + i * 4);
  }
  iss->cpu.irq.vector_base = base;
}

static inline void iss_irq_build(Iss *iss)
{
  for (int i=0; i<32; i++)
  {
    iss->cpu.irq.vectors[i] = NULL;
  }

}

static inline void iss_irq_init(Iss *iss)
{
  iss->cpu.irq.vector_base = 0;
  iss->cpu.state.elw_interrupted = 0;
  iss->cpu.irq.irq_enable = 0;
  iss->cpu.irq.saved_irq_enable = 0;
  iss->cpu.irq.req_irq = -1;
  iss->cpu.irq.req_debug = false;
#if defined(PRIV_1_10)
  iss->cpu.irq.debug_handler = insn_cache_get(iss, iss->cpu.config.debug_handler);
#endif
}

static inline void iss_irq_flush(Iss *iss)
{
  iss_irq_set_vector_table(iss, iss->cpu.irq.vector_base);
#if defined(PRIV_1_10)
  iss->cpu.irq.debug_handler = insn_cache_get(iss, iss->cpu.config.debug_handler);
#endif
}

#endif
