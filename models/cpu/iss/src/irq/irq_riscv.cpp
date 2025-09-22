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
#include <cpu/iss/include/irq/irq_riscv_implem.hpp>
#include <cpu/iss/include/iss.hpp>


Irq::Irq(Iss &iss)
    : iss(iss)
{
}

void Irq::build()
{
    iss.top.traces.new_trace("irq", &this->trace, vp::DEBUG);

    this->iss.csr.mideleg.register_callback(std::bind(&Irq::mideleg_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.mip.register_callback(std::bind(&Irq::mip_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.mie.register_callback(std::bind(&Irq::mie_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.sip.register_callback(std::bind(&Irq::sip_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.sie.register_callback(std::bind(&Irq::sie_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.mtvec.register_callback(std::bind(&Irq::mtvec_access, this, std::placeholders::_1, std::placeholders::_2));
    this->iss.csr.stvec.register_callback(std::bind(&Irq::stvec_access, this, std::placeholders::_1, std::placeholders::_2));

    this->msi_itf.set_sync_meth(&Irq::msi_sync);
    this->iss.top.new_slave_port("msi", &this->msi_itf, (vp::Block *)this);

    this->mti_itf.set_sync_meth(&Irq::mti_sync);
    this->iss.top.new_slave_port("mti", &this->mti_itf, (vp::Block *)this);

    this->mei_itf.set_sync_meth(&Irq::mei_sync);
    this->iss.top.new_slave_port("mei", &this->mei_itf, (vp::Block *)this);

    this->sei_itf.set_sync_meth(&Irq::sei_sync);
    this->iss.top.new_slave_port("sei", &this->sei_itf, (vp::Block *)this);

    for (int i=0; i<20; i++)
    {
        this->external_irq_itf[i].set_sync_meth_muxed(&Irq::external_irq_sync, i + 12);
        this->iss.top.new_slave_port("external_irq_" + std::to_string(i + 12),
            &this->external_irq_itf[i], (vp::Block *)this);
    }
}

void Irq::reset(bool active)
{
    if (active)
    {
        this->irq_enable.set(0);
        this->req_irq = -1;
        this->req_debug = false;
        this->debug_handler = this->iss.exception.debug_handler_addr;
    }
    else
    {
        this->mtvec_set(this->iss.exec.bootaddr_reg.get() & ~((1 << 8) - 1));
        this->stvec_set(this->iss.exec.bootaddr_reg.get() & ~((1 << 8) - 1));
    }
}

void Irq::external_irq_sync(vp::Block *__this, bool value, int id)
{
    Irq *_this = (Irq *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<id)) | (value << id);
    _this->check_interrupts();
}

void Irq::msi_sync(vp::Block *__this, bool value)
{
    Irq *_this = (Irq *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_SOFT)) | (value << IRQ_M_SOFT);
    _this->check_interrupts();
}

void Irq::mti_sync(vp::Block *__this, bool value)
{
    Irq *_this = (Irq *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_TIMER)) | (value << IRQ_M_TIMER);

    _this->check_interrupts();
}

void Irq::mei_sync(vp::Block *__this, bool value)
{
    Irq *_this = (Irq *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_EXT)) | (value << IRQ_M_EXT);
    _this->check_interrupts();
}

void Irq::sei_sync(vp::Block *__this, bool value)
{
    Irq *_this = (Irq *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_S_EXT)) | (value << IRQ_S_EXT);
    _this->check_interrupts();
}

bool Irq::mideleg_access(bool is_write, iss_reg_t &value)
{
    return true;
}

bool Irq::mip_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->iss.csr.mip.value = value & 0x00000AAA;
    }
    else
    {
        value = this->iss.csr.mip.value;
    }
    this->check_interrupts();
    return false;
}

bool Irq::mie_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->iss.csr.mie.value = value;
    }
    else
    {
        value = this->iss.csr.mie.value;
    }
    this->check_interrupts();
    return false;
}

bool Irq::sip_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->iss.csr.mip.value = (this->iss.csr.mip.value & ~0x00000222) | (value & 0x00000222);
    }
    else
    {
        value = this->iss.csr.mip.value & 0x00000222;
    }
    this->check_interrupts();
    return false;
}

bool Irq::sie_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->iss.csr.mie.value = (this->iss.csr.mie.value & ~0x00000222) | (value & 0x00000222);
    }
    else
    {
        value = this->iss.csr.mie.value & 0x00000222;
    }
    this->check_interrupts();
    return false;
}

bool Irq::mtvec_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->mtvec_set(value);
        this->iss.csr.mtvec.value = value & ~1;
        return false;
    }
    else
    {
        return true;
    }
}

bool Irq::stvec_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->stvec_set(value);
        this->iss.csr.stvec.value = value & ~1;
        return false;
    }
    else
    {
        return true;
    }
}

bool Irq::mtvec_set(iss_addr_t base)
{
    base &= ~(3ULL);
    this->trace.msg("Setting mtvec (addr: 0x%x)\n", base);

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
}

void Irq::wfi_handle()
{
    // The instruction loop is checking for IRQs only if interrupts are globally enable
    // while wfi ends as soon as one interrupt is active even if interrupts are globally disabled,
    // so we have to check now if we can really go to sleep.
    //if ((this->iss.csr.mie.value & this->iss.csr.mip.value) == 0) //ZL-MOD 22092025 Hot fix for MAGIA! TBD.
    {
#ifdef CONFIG_GVSOC_ISS_EXEC_WAKEUP_COUNTER
        if (this->iss.exec.wakeup.get())
        {
            this->iss.exec.wakeup.dec(1);
        }
        else
#endif
        {
            this->iss.exec.wfi.set(true);
            this->iss.exec.wfi_start = this->iss.top.clock.get_cycles();
            this->iss.exec.busy_exit();
            this->iss.exec.insn_stall();
        }
    }
}

void Irq::check_interrupts()
{
    iss_reg_t pending_interrupts = this->iss.csr.mie.value & this->iss.csr.mip.value;

    if (pending_interrupts && !this->iss.exec.irq_locked)
    {
        this->iss.exec.switch_to_full_mode();

        if (this->iss.exec.wfi.get())
        {
            this->iss.exec.wfi.set(false);
            if (this->iss.exec.stall_cycles > (this->iss.top.clock.get_cycles() - this->iss.exec.wfi_start))
            {
                this->iss.exec.stall_cycles -= (this->iss.top.clock.get_cycles() - this->iss.exec.wfi_start);
            }
            else
            {
                this->iss.exec.stall_cycles = 0;
            }
            this->iss.exec.busy_enter();
            this->iss.exec.stalled_dec();
            this->iss.exec.insn_terminate();
        }
    }
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
        iss_reg_t pending_interrupts = this->iss.csr.mie.value & this->iss.csr.mip.value;

        if (pending_interrupts)
        {
            int next_mode = PRIV_M;

            // First check if the interrupt is handled in M mode
            iss_reg_t enabled_interrupts = pending_interrupts & ~this->iss.csr.mideleg.value;
            bool m_irq_enabled = (this->iss.core.mode_get() == PRIV_M && this->iss.csr.mstatus.mie) ||
                this->iss.core.mode_get() < PRIV_M;

            if (!enabled_interrupts || !m_irq_enabled)
            {
                // Otherwise, check if it is handled in S mode
                next_mode = PRIV_S;
                enabled_interrupts = pending_interrupts & this->iss.csr.mideleg.value;
                bool s_irq_enabled = (this->iss.core.mode_get() == PRIV_S && this->iss.csr.mstatus.sie) ||
                    this->iss.core.mode_get() < PRIV_S;

                if (!s_irq_enabled)
                {
                    enabled_interrupts = 0;
                }
            }

            if (enabled_interrupts)
            {
                int irq;
                if ((enabled_interrupts >> 11) & 1)
                {
                    irq = 11;
                }
                else if ((enabled_interrupts >> 3) & 1)
                {
                    irq = 3;
                }
                else if ((enabled_interrupts >> 7) & 1)
                {
                    irq = 7;
                }
                else if ((enabled_interrupts >> 9) & 1)
                {
                    irq = 9;
                }
                else if ((enabled_interrupts >> 1) & 1)
                {
                    irq = 1;
                }
                else if ((enabled_interrupts >> 5) & 1)
                {
                    irq = 5;
                }
                else
                {
                    irq = ffs(enabled_interrupts) - 1;
                }

                this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling IRQ (irq: %d)\n", irq);

                this->iss.exec.interrupt_taken();
                this->iss.csr.mepc.value = this->iss.exec.current_insn;
                this->iss.csr.mstatus.mpie = this->irq_enable.get();
                this->iss.csr.mstatus.mie = 0;

                if (next_mode == PRIV_M)
                {
                    this->iss.csr.mepc.value = this->iss.exec.current_insn;
                    this->iss.csr.mstatus.mie = 0;
                    this->iss.csr.mstatus.mpie = this->iss.irq.irq_enable.get();
                    this->iss.csr.mstatus.mpp = this->iss.core.mode_get();
                    this->iss.exec.current_insn = this->iss.csr.mtvec.value;
                    this->iss.csr.mcause.value = (1ULL << (ISS_REG_WIDTH - 1)) | (unsigned int)irq;
                }
                else
                {
                    this->iss.csr.sepc.value = this->iss.exec.current_insn;
                    this->iss.csr.mstatus.sie = 0;
                    this->iss.csr.mstatus.spie = this->iss.irq.irq_enable.get();
                    this->iss.csr.mstatus.spp = this->iss.core.mode_get();
                    this->iss.exec.current_insn = this->iss.csr.stvec.value;
                    this->iss.csr.scause.value = (1ULL << (ISS_REG_WIDTH - 1)) | (unsigned int)irq;
                }
                this->iss.core.mode_set(next_mode);

                this->irq_enable.set(0);

                this->iss.timing.stall_insn_dependency_account(4);

                return 1;
            }
        }
    }

    return 0;
}
