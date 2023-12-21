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
#include <cpu/iss/include/irq/irq_external_implem.hpp>
#include <cpu/iss/include/iss.hpp>

Irq::Irq(Iss &iss)
    : iss(iss)
{
}

void Irq::build()
{
    for (int i = 0; i < 32; i++)
    {
        this->vectors[i] = 0;
    }
    iss.top.traces.new_trace("irq", &this->trace, vp::DEBUG);

    irq_req_itf.set_sync_meth(&Irq::irq_req_sync);
    this->iss.top.new_slave_port("irq_req", &irq_req_itf, (vp::Block *)this);
    this->iss.top.new_master_port("irq_ack", &irq_ack_itf);

    this->iss.top.new_reg("irq_enable", &this->irq_enable, false);

    this->iss.csr.mtvec.register_callback(std::bind(&Irq::mtvec_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.stvec.register_callback(std::bind(&Irq::stvec_access, this, std::placeholders::_1, std::placeholders::_2));

}


void Irq::reset(bool active)
{
    if (active)
    {
        this->irq_req = -1;
        this->iss.exec.elw_interrupted = 0;
        this->irq_enable.set(0);
        this->req_irq = -1;
        this->req_debug = false;
        this->debug_handler = this->iss.exception.debug_handler_addr;
    }
    else
    {
        this->mtvec_set(this->iss.exec.bootaddr_reg.get() & ~((1 << 8) - 1));

    }
}

bool Irq::mtvec_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->mtvec_set(value);
    }

    return true;
}

bool Irq::stvec_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->stvec_set(value);
    }

    return true;
}

bool Irq::mtvec_set(iss_addr_t base)
{
    base &= ~(3ULL);
    this->trace.msg("Setting mtvec (addr: 0x%x)\n", base);

    for (int i = 0; i < 32; i++)
    {
        this->iss.irq.vectors[i] = base + i * 4;
    }

    for (int i = 32; i < 35; i++)
    {
        this->iss.irq.vectors[i] = base + i * 4;
    }

    return true;
}

bool Irq::stvec_set(iss_addr_t base)
{
    base &= ~(3ULL);
    this->trace.msg("Setting stvec (addr: 0x%x)\n", base);
    return true;
}

void Irq::cache_flush()
{
    this->mtvec_set(this->iss.csr.mtvec.value);
    this->iss.irq.debug_handler = this->iss.exception.debug_handler_addr;
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

void Irq::irq_req_sync(vp::Block *__this, int irq)
{
    Irq *_this = (Irq *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received IRQ (irq: %d)\n", irq);

    _this->req_irq = irq;

    if (irq != -1 && _this->iss.exec.wfi.get())
    {
        _this->iss.exec.wfi.set(false);
        _this->iss.exec.busy_enter();
        _this->iss.exec.stalled_dec();
        _this->iss.exec.insn_terminate();
    }

    if (_this->iss.lsu.elw_stalled.get() && irq != -1 && _this->irq_enable.get())
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
        this->iss.csr.depc = this->iss.exec.current_insn;
        this->debug_saved_irq_enable = this->irq_enable.get();
        this->irq_enable.set(0);
        this->req_debug = false;
        this->iss.exec.current_insn = this->debug_handler;

        return 1;
    }
    else
    {
        int req_irq = this->req_irq;

        if (req_irq != -1 && this->irq_enable.get() && !this->iss.exec.irq_locked)
        {
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling IRQ (irq: %d)\n", req_irq);

            this->iss.exec.interrupt_taken();
            this->iss.csr.mepc.value = this->iss.exec.current_insn;
            this->iss.csr.mstatus.mpie = this->irq_enable.get();
            this->iss.csr.mstatus.mie = 0;

            int next_mode = PRIV_M;
            int id = 0;
            if ((this->iss.csr.mideleg.value >> id) & 1)
            {
                next_mode = PRIV_S;
            }

            if (next_mode == PRIV_M)
            {
                this->iss.csr.mepc.value = this->iss.exec.current_insn;
                this->iss.csr.mstatus.mie = 0;
                this->iss.csr.mstatus.mpie = this->iss.irq.irq_enable.get();
                this->iss.csr.mstatus.mpp = this->iss.core.mode_get();
            }
            else
            {
                this->iss.csr.sepc.value = this->iss.exec.current_insn;
                this->iss.csr.mstatus.sie = 0;
                this->iss.csr.mstatus.spie = this->iss.irq.irq_enable.get();
                this->iss.csr.mstatus.spp = this->iss.core.mode_get();
            }
            this->iss.core.mode_set(next_mode);

            this->irq_enable.set(0);
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
