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

DbgUnit::DbgUnit(Iss &iss)
    : iss(iss)
{
}

void DbgUnit::build()
{
    iss.traces.new_trace("dbgunit", &this->trace, vp::DEBUG);

    dbg_unit.set_req_meth(&DbgUnit::dbg_unit_req);
    this->iss.new_slave_port(this, "dbg_unit", &dbg_unit);


    halt_itf.set_sync_meth(&DbgUnit::halt_sync);
    this->iss.new_slave_port(this, "halt", &halt_itf);

    this->iss.new_master_port("halt_status", &halt_status_itf);

    this->iss.new_reg("do_step", &this->do_step, false);

    this->riscv_dbg_unit = this->iss.get_js_config()->get_child_bool("riscv_dbg_unit");
}

void DbgUnit::debug_req()
{
    this->iss.irq.req_debug = true;
    this->iss.exec.switch_to_full_mode();
    this->iss.exec.wfi.set(false);
}

void DbgUnit::set_halt_mode(bool halted, int cause)
{
    if (this->riscv_dbg_unit)
    {
        if (!this->iss.exec.debug_mode)
        {
            this->iss.csr.dcsr = (this->iss.csr.dcsr & ~(0x7 << 6)) | (cause << 6);
            this->debug_req();
        }
    }
    else
    {
        this->halt_cause = cause;

        if (this->iss.exec.halted.get() && !halted)
        {
            this->iss.get_clock()->release();
        }
        else if (!this->iss.exec.halted.get() && halted)
        {
            this->iss.get_clock()->retain();
        }

        this->iss.exec.halted.set(halted);

        if (this->halt_status_itf.is_bound())
            this->halt_status_itf.sync(this->iss.exec.halted.get());
    }
}

void DbgUnit::halt_core()
{
    this->trace.msg("Halting core\n");

    if (this->iss.exec.prev_insn == NULL)
        this->ppc = 0;
    else
        this->ppc = this->iss.exec.prev_insn->addr;
    this->npc = this->iss.exec.current_insn->addr;
}

void DbgUnit::halt_sync(void *__this, bool halted)
{
    DbgUnit *_this = (DbgUnit *)__this;

    _this->trace.msg("Received halt signal sync (halted: 0x%d)\n", halted);
    _this->set_halt_mode(halted, HALT_CAUSE_HALT);
}

vp::io_req_status_e DbgUnit::dbg_unit_req(void *__this, vp::io_req *req)
{
    DbgUnit *_this = (DbgUnit *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    _this->trace.msg("IO access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

    if (size != ISS_REG_WIDTH / 8)
        return vp::IO_REQ_INVALID;

    if (offset >= 0x4000)
    {
        offset -= 0x4000;

        if (size != 4)
            return vp::IO_REQ_INVALID;

        bool err;
        if (req->get_is_write())
            err = iss_csr_write(&_this->iss, offset / 4, *(iss_reg_t *)data);
        else
            err = iss_csr_read(&_this->iss, offset / 4, (iss_reg_t *)data);

        if (err)
            return vp::IO_REQ_INVALID;
    }
    else if (offset >= 0x2000)
    {
        if (!_this->iss.exec.halted.get())
        {
            _this->trace.force_warning("Trying to access debug registers while core is not halted\n");
            return vp::IO_REQ_INVALID;
        }

        if (offset == 0x2000)
        {
            if (req->get_is_write())
            {
                // Writing NPC will force the core to jump to the written PC
                // even if the core is sleeping
                iss_cache_flush(&_this->iss);
                _this->npc = *(iss_reg_t *)data;
                _this->iss.exec.pc_set(_this->npc);
                _this->iss.exec.wfi.set(false);
            }
            else
                *(iss_reg_t *)data = _this->npc;
        }
        else if (offset == 0x2004)
        {
            if (req->get_is_write())
                _this->trace.force_warning("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
            else
                *(iss_reg_t *)data = _this->ppc;
        }
        else
        {
            return vp::IO_REQ_INVALID;
        }
    }
    else if (offset >= 0x400)
    {
        offset -= 0x400;
        int reg_id = offset / 4;

        if (!_this->iss.exec.halted.get())
        {
            _this->trace.force_warning("Trying to access GPR while core is not halted\n");
            return vp::IO_REQ_INVALID;
        }

        if (reg_id >= ISS_NB_REGS)
            return vp::IO_REQ_INVALID;

        if (req->get_is_write())
            _this->iss.regfile.set_reg(reg_id, *(iss_reg_t *)data);
        else
            *(iss_reg_t *)data = _this->iss.regfile.get_reg(reg_id);
    }
    else if (offset < 0x80)
    {
        if (offset == 0x00)
        {
            if (req->get_is_write())
            {
                bool step_mode = (*(iss_reg_t *)data) & 1;
                bool halt_mode = ((*(iss_reg_t *)data) >> 16) & 1;
                _this->trace.msg("Writing DBG_CTRL (value: 0x%x, halt: %d, step: %d)\n", *(iss_reg_t *)data, halt_mode, step_mode);

                _this->set_halt_mode(halt_mode, HALT_CAUSE_HALT);
                _this->iss.exec.step_mode.set(step_mode);
            }
            else
            {
                *(iss_reg_t *)data = (_this->iss.exec.halted.get() << 16) | _this->iss.exec.step_mode.get();
            }
        }
        else if (offset == 0x04)
        {
            if (req->get_is_write())
            {
                _this->hit_reg = *(iss_reg_t *)data;
            }
            else
            {
                *(iss_reg_t *)data = _this->hit_reg;
            }
        }
        else if (offset == 0x0C)
        {
            if (req->get_is_write())
                return vp::IO_REQ_INVALID;

            *(iss_reg_t *)data = _this->halt_cause;
        }
    }
    else
    {
        _this->trace.force_warning("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
        return vp::IO_REQ_INVALID;
    }

    return vp::IO_REQ_OK;
}
