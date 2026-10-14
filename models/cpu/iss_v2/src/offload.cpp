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

Offload::Offload(Iss &iss)
: iss(iss)
{
    this->iss.new_master_port("offload", &this->offload_itf);

    this->offload_grant_itf.set_sync_meth(&Offload::offload_grant);
    this->iss.new_slave_port("offload_grant", &this->offload_grant_itf, (vp::Block *)this);
}

void Offload::reset(bool active)
{
    if (active)
    {
        this->denied = false;
        this->pending_entry = NULL;
    }
}

bool Offload::offload_insn(IssOffloadInsn<iss_reg_t> *insn)
{
    this->offload_itf.sync(insn);

    if (!this->denied && insn->granted) return false;

    this->denied = true;
    return true;
}

void Offload::offload_insn_result(IssOffloadInsn<iss_reg_t> *offload, iss_insn_t *insn, int reg)
{
    this->offload_itf.sync(offload);

    if (offload->granted)
    {
        this->iss.regfile.set_reg(reg, offload->result);
        return;
    }

    this->pending_reg = reg;
    this->iss.exec.busy_exit();
    this->iss.exec.retain_inc();
    this->pending_entry = this->iss.exec.insn_hold(insn);
}

void Offload::offload_grant(vp::Block *__this, IssOffloadInsnGrant<iss_reg_t> *result)
{
    Offload *_this = (Offload *)__this;
    _this->denied = false;

    if (_this->pending_entry)
    {
        InsnEntry *entry = _this->pending_entry;
        _this->pending_entry = NULL;
        _this->iss.regfile.set_reg(_this->pending_reg, result->result);
        _this->iss.exec.busy_enter();
        _this->iss.exec.retain_dec();
        _this->iss.exec.insn_terminate(entry);
    }
}
