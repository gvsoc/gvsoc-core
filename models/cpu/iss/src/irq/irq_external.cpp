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
#include <irq/irq_external_implem.hpp>
#include <iss.hpp>

Irq::Irq(Iss &iss)
    : iss(iss)
{
}

void Irq::build()
{
    for (int i = 0; i < 32; i++)
    {
        this->vectors[i] = NULL;
    }
    iss.top.traces.new_trace("irq", &this->trace, vp::DEBUG);

    irq_req_itf.set_sync_meth(&Irq::irq_req_sync);
    this->iss.top.new_slave_port(this, "irq_req", &irq_req_itf);
    this->iss.top.new_master_port("irq_ack", &irq_ack_itf);

    this->iss.csr.mtvec.register_callback(std::bind(&Irq::vector_table_set, this, std::placeholders::_1));

}


void Irq::reset(bool active)
{
    if (active)
    {
        this->irq_req = -1;
        this->iss.exec.elw_interrupted = 0;
        this->vector_base = 0;
        this->irq_enable = 0;
        this->req_irq = -1;
        this->req_debug = false;
        this->debug_handler = insn_cache_get(&this->iss, this->iss.exception.debug_handler_addr);
    }
    else
    {
        this->vector_table_set(this->iss.exec.bootaddr_reg.get() & ~((1 << 8) - 1));

    }
}


bool Irq::vector_table_set(iss_addr_t base)
{
    this->trace.msg("Setting vector table (addr: 0x%x)\n", base);
    for (int i = 0; i < 32; i++)
    {
        this->iss.irq.vectors[i] = insn_cache_get(&this->iss, base + i * 4);
    }

    for (int i = 32; i < 35; i++)
    {
        this->iss.irq.vectors[i] = insn_cache_get(&this->iss, base + i * 4);
    }
    this->iss.irq.vector_base = base;

    return true;
}

void Irq::cache_flush()
{
    this->vector_table_set(this->iss.irq.vector_base);
    this->iss.irq.debug_handler = insn_cache_get(&this->iss, this->iss.exception.debug_handler_addr);
}

void Irq::wfi_handle()
{
    // The instruction loop is checking for IRQs only if interrupts are globally enable
    // while wfi ends as soon as one interrupt is active even if interrupts are globally disabled,
    // so we have to check now if we can really go to sleep.
    if (this->req_irq == -1)
    {
        this->iss.exec.wfi.set(true);
        this->iss.exec.busy_exit();
        this->iss.exec.insn_stall();
    }
}

void Irq::elw_irq_unstall()
{
    this->trace.msg("Interrupting pending elw\n");
    this->iss.exec.current_insn = this->iss.exec.elw_insn;
    // Keep the information that we interrupted it, so that features like HW loop
    // knows that the instruction is being replayed
    this->iss.exec.elw_interrupted = 1;
    this->iss.exec.busy_enter();
}

void Irq::irq_req_sync(void *__this, int irq)
{
    Irq *_this = (Irq *)__this;

    _this->trace.msg(vp::trace::LEVEL_TRACE, "Received IRQ (irq: %d)\n", irq);

    _this->req_irq = irq;

    if (irq != -1 && _this->iss.exec.wfi.get())
    {
        _this->iss.exec.wfi.set(false);
        _this->iss.exec.busy_enter();
        _this->iss.exec.stalled_dec();
        _this->iss.exec.insn_terminate();
    }

    if (_this->iss.lsu.elw_stalled.get() && irq != -1 && _this->irq_enable)
    {
        _this->elw_irq_unstall();
    }

    _this->iss.exec.switch_to_full_mode();
}

int Irq::check()
{
    if (this->req_debug && !this->iss.exec.debug_mode)
    {
        this->iss.exec.debug_mode = true;
        this->iss.csr.depc = this->iss.exec.current_insn->addr;
        this->debug_saved_irq_enable = this->irq_enable;
        this->irq_enable = 0;
        this->req_debug = false;
        this->iss.exec.current_insn = this->debug_handler;
        return 1;
    }
    else
    {
        int req_irq = this->req_irq;
        if (req_irq != -1 && this->irq_enable)
        {
            this->trace.msg(vp::trace::LEVEL_TRACE, "Handling IRQ (irq: %d)\n", req_irq);

            this->iss.csr.mepc.value = this->iss.exec.current_insn->addr;
            this->iss.csr.mstatus.mpie = this->irq_enable;
            this->iss.csr.mstatus.mie = 0;
            this->irq_enable = 0;
            this->req_irq = -1;
            this->iss.exec.current_insn = this->vectors[req_irq];
            this->iss.csr.mcause.value = (1 << 31) | (unsigned int)req_irq;

            this->trace.msg("Acknowledging interrupt (irq: %d)\n", req_irq);
            this->irq_ack_itf.sync(req_irq);

            this->iss.timing.stall_insn_dependency_account(4);

            return 1;
        }
    }

    return 0;
}
