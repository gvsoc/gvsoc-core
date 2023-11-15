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
#include "cpu/iss/include/iss.hpp"

DbgUnit::DbgUnit(Iss &iss)
    : iss(iss)
{
}

void DbgUnit::build()
{
    iss.top.traces.new_trace("dbgunit", &this->trace, vp::DEBUG);

    halt_itf.set_sync_meth(&DbgUnit::halt_sync);
    this->iss.top.new_slave_port("halt", &halt_itf, (vp::Block *)this);

    this->iss.top.new_master_port("halt_status", &halt_status_itf);

    this->iss.top.new_reg("do_step", &this->do_step, false);

    this->riscv_dbg_unit = this->iss.top.get_js_config()->get_child_bool("riscv_dbg_unit");
}

void DbgUnit::debug_req()
{
    this->iss.irq.req_debug = true;
    this->iss.exec.switch_to_full_mode();
    this->iss.exec.wfi.set(false);
    this->iss.exec.busy_enter();
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

        this->iss.exec.halted.set(halted);

        if (this->halt_status_itf.is_bound())
            this->halt_status_itf.sync(this->iss.exec.halted.get());
    }
}

void DbgUnit::halt_sync(void *__this, bool halted)
{
    DbgUnit *_this = (DbgUnit *)__this;

    _this->trace.msg("Received halt signal sync (halted: 0x%d)\n", halted);
    _this->set_halt_mode(halted, HALT_CAUSE_HALT);
}
