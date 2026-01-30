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

IrqRiscv::IrqRiscv(Iss &iss)
    : iss(iss)
{
    iss.traces.new_trace("irq", &this->trace, vp::DEBUG);

    this->iss.csr.mideleg.register_callback(std::bind(&IrqRiscv::mideleg_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.mip.register_callback(std::bind(&IrqRiscv::mip_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.mie.register_callback(std::bind(&IrqRiscv::mie_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.sip.register_callback(std::bind(&IrqRiscv::sip_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.sie.register_callback(std::bind(&IrqRiscv::sie_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.mtvec.register_callback(std::bind(&IrqRiscv::mtvec_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->iss.csr.stvec.register_callback(std::bind(&IrqRiscv::stvec_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    this->msi_itf.set_sync_meth(&IrqRiscv::msi_sync);
    this->iss.new_slave_port("msi", &this->msi_itf, (vp::Block *)this);

    this->mti_itf.set_sync_meth(&IrqRiscv::mti_sync);
    this->iss.new_slave_port("mti", &this->mti_itf, (vp::Block *)this);

    this->mei_itf.set_sync_meth(&IrqRiscv::mei_sync);
    this->iss.new_slave_port("mei", &this->mei_itf, (vp::Block *)this);

    this->sei_itf.set_sync_meth(&IrqRiscv::sei_sync);
    this->iss.new_slave_port("sei", &this->sei_itf, (vp::Block *)this);

    for (int i=0; i<20; i++)
    {
        this->external_irq_itf[i].set_sync_meth_muxed(&IrqRiscv::external_irq_sync, i + 12);
        this->iss.new_slave_port("external_irq_" + std::to_string(i + 12),
            &this->external_irq_itf[i], (vp::Block *)this);
    }
}

void IrqRiscv::global_enable(int enable)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Setting IRQ enable (value: %d)\n",
                    enable);

    this->iss.irq.irq_enable.set(enable);
    this->iss.exec.switch_to_full_mode();
}

void IrqRiscv::reset(bool active)
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

void IrqRiscv::external_irq_sync(vp::Block *__this, bool value, int id)
{
    IrqRiscv *_this = (IrqRiscv *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<id)) | (value << id);
    _this->check_interrupts();
}

void IrqRiscv::msi_sync(vp::Block *__this, bool value)
{
    IrqRiscv *_this = (IrqRiscv *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_SOFT)) | (value << IRQ_M_SOFT);
    _this->check_interrupts();
}

void IrqRiscv::mti_sync(vp::Block *__this, bool value)
{
    IrqRiscv *_this = (IrqRiscv *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_TIMER)) | (value << IRQ_M_TIMER);

    _this->check_interrupts();
}

void IrqRiscv::mei_sync(vp::Block *__this, bool value)
{
    IrqRiscv *_this = (IrqRiscv *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_M_EXT)) | (value << IRQ_M_EXT);
    _this->check_interrupts();
}

void IrqRiscv::sei_sync(vp::Block *__this, bool value)
{
    IrqRiscv *_this = (IrqRiscv *)__this;
    _this->iss.csr.mip.value =  (_this->iss.csr.mip.value & ~(1<<IRQ_S_EXT)) | (value << IRQ_S_EXT);
    _this->check_interrupts();
}

bool IrqRiscv::mideleg_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
{
    return true;
}

bool IrqRiscv::mip_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::mie_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::sip_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::sie_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::mtvec_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::stvec_access(iss_insn_t *insn, bool is_write, iss_reg_t &value)
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

bool IrqRiscv::mtvec_set(iss_addr_t base)
{
    base &= ~(3ULL);
    this->trace.msg("Setting mtvec (addr: 0x%x)\n", base);

    return true;
}

bool IrqRiscv::stvec_set(iss_addr_t base)
{
    base &= ~(3ULL);
    this->trace.msg("Setting stvec (addr: 0x%x)\n", base);
    return true;
}

void IrqRiscv::cache_flush()
{
}

void IrqRiscv::wfi_handle(iss_insn_t *insn)
{
    // The instruction loop is checking for IRQs only if interrupts are globally enable
    // while wfi ends as soon as one interrupt is active even if interrupts are globally disabled,
    // so we have to check now if we can really go to sleep.
    if ((this->iss.csr.mie.value & this->iss.csr.mip.value) == 0)
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
            this->iss.exec.wfi_start = this->iss.clock.get_cycles();
            this->iss.exec.retain_inc();
            this->wfi_entry = this->iss.exec.insn_hold(insn);

        }
    }
}

void IrqRiscv::check_interrupts()
{
    iss_reg_t pending_interrupts = this->iss.csr.mie.value & this->iss.csr.mip.value;

    if (pending_interrupts && !this->iss.exec.irq_locked)
    {
        this->iss.exec.switch_to_full_mode();

        if (this->iss.exec.wfi.get())
        {
            this->iss.exec.wfi.set(false);
            if (this->iss.exec.stall_cycles > (this->iss.clock.get_cycles() - this->iss.exec.wfi_start))
            {
                this->iss.exec.stall_cycles -= (this->iss.clock.get_cycles() - this->iss.exec.wfi_start);
            }
            else
            {
                this->iss.exec.stall_cycles = 0;
            }
            this->iss.exec.retain_dec();
            this->iss.exec.insn_terminate(this->wfi_entry);
        }
    }
}

int IrqRiscv::check()
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
