/*
 * Copyright (C) 2020 SAS, ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include "cpu/iss/include/iss.hpp"
#include <string.h>


void IssWrapper::start()
{
    vp_assert_always(this->iss.lsu.data.is_bound(), &this->trace, "Data master port is not connected\n");
    vp_assert_always(this->iss.prefetcher.fetch_itf.is_bound(), &this->trace, "Fetch master port is not connected\n");
    // vp_assert_always(this->irq_ack_itf.is_bound(), &this->trace, "IRQ ack master port is not connected\n");

    this->trace.msg("ISS start (fetch: %d, boot_addr: 0x%lx)\n",
        iss.exec.fetch_enable_reg.get(), get_js_config()->get_child_int("boot_addr"));

    this->iss.timing.background_power.leakage_power_start();
    this->iss.timing.background_power.dynamic_power_start();

    this->iss.lsu.start();
    this->iss.gdbserver.start();
}

void IssWrapper::stop()
{
    this->iss.insn_cache.stop();
    this->iss.gdbserver.stop();
}

void IssWrapper::reset(bool active)
{
    this->iss.prefetcher.reset(active);
    this->iss.csr.reset(active);
    this->iss.exec.reset(active);
    this->iss.core.reset(active);
    this->iss.mmu.reset(active);
    this->iss.pmp.reset(active);
    this->iss.irq.reset(active);
    this->iss.lsu.reset(active);
    this->iss.timing.reset(active);
    this->iss.trace.reset(active);
    this->iss.regfile.reset(active);
    this->iss.decode.reset(active);
    this->iss.gdbserver.reset(active);
    this->iss.syscalls.reset(active);
    this->iss.vector.reset(active);
    this->iss.ara.reset(active);

    this->do_flush = false;
}

IssWrapper::IssWrapper(vp::ComponentConf &config)
    : vp::Component(config), iss(*this),
    fsm_event(this, &IssWrapper::fsm_handler)
{
    this->iss.syscalls.build();
    this->iss.decode.build();
    this->iss.exec.build();
    this->iss.insn_cache.build();
    this->iss.dbgunit.build();
    this->iss.csr.build();
    this->iss.lsu.build();
    this->iss.irq.build();
    this->iss.trace.build();
    this->iss.timing.build();
    this->iss.gdbserver.build();
    this->iss.core.build();
    this->iss.mmu.build();
    this->iss.pmp.build();
    this->iss.exception.build();
    this->iss.prefetcher.build();
    this->iss.vector.build();
    this->iss.ara.build();

    traces.new_trace("wrapper", &this->trace, vp::DEBUG);

    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("all"))
        {
            insn->u.insn.stub_handler = &IssWrapper::insn_stub_handler;
        }

#ifdef CONFIG_ISS_HAS_VECTOR
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_isa("v"))
        {
            insn->u.insn.stub_handler = &IssWrapper::vector_insn_stub_handler;
            insn->u.insn.flags |= ISS_INSN_FLAGS_VECTOR;
        }
#endif
    }
}

void IssWrapper::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    IssWrapper *_this = (IssWrapper *)__this;

    if (_this->pending_insns.size() == 0)
    {
        _this->fsm_event.disable();
        return;
    }

    if (_this->pending_insns_done.front() &&
        _this->pending_insns_timestamp.front() <= _this->clock.get_cycles())
    {
        iss_insn_t *insn = _this->pending_insns.front();
        iss_addr_t pc = _this->pending_insns_pc.front();
        _this->pending_insns.pop();
        _this->pending_insns_done.pop();
        _this->pending_insns_timestamp.pop();
        _this->pending_insns_pc.pop();

        if ((insn->flags & ISS_INSN_FLAGS_VECTOR) == 0)
        {
            iss_addr_t next_pc = insn->stub_handler(&_this->iss, insn, pc);

            for (int i=0; i<insn->nb_out_reg; i++)
            {
                if (!insn->out_regs_fp[i])
                {
                    _this->iss.regfile.scoreboard_reg_timestamp[insn->out_regs[i]] = _this->clock.get_cycles();
                }
                else
                {
                    _this->iss.regfile.scoreboard_freg_timestamp[insn->out_regs[i]] = _this->clock.get_cycles();
                }
            }

            if (_this->do_flush && _this->pending_insns.size() == 0)
            {
                _this->do_flush = false;
                _this->iss.exec.current_insn = next_pc;
            }
        }
    }
}

iss_reg_t IssWrapper::insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->top.do_flush)
    {
        return pc;
    }

    for (int i=0; i<insn->nb_out_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[i].u.reg.flags & ISS_DECODER_ARG_FLAG_FREG) == 0)
        {
            iss->regfile.scoreboard_reg_invalidate(insn->out_regs[i]);
        }
        else
        {
            iss->regfile.scoreboard_freg_invalidate(insn->out_regs[i]);
        }
    }

    iss->top.pending_insns.push(insn);
    iss->top.pending_insns_done.push(true);
    iss->top.pending_insns_timestamp.push(iss->top.clock.get_cycles() + 1);
    iss->top.pending_insns_pc.push(pc);
    iss->top.fsm_event.enable();

    if (insn->decoder_item->u.insn.tags[ISA_TAG_BRANCH_ID])
    {
        iss->top.do_flush = true;
        return pc;
    }

    return iss_insn_next(iss, insn, pc);
}

iss_reg_t IssWrapper::vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (iss->ara.queue_is_full())
    {
        return pc;
    }

    for (int i=0; i<insn->nb_in_reg; i++)
    {
        if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_VREG) == 0)
        {
            if ((insn->decoder_item->u.insn.args[insn->nb_out_reg + i].u.reg.flags & ISS_DECODER_ARG_FLAG_FREG) == 0)
            {
                if (iss->regfile.scoreboard_reg_timestamp[insn->in_regs[i]] == -1)
                {
                    return pc;
                }
            }
            else
            {
                if (iss->regfile.scoreboard_freg_timestamp[insn->in_regs[i]] == -1)
                {
                    return pc;
                }
            }
        }
    }

    iss->top.pending_insns.push(insn);
    iss->top.pending_insns_done.push(true);
    iss->top.pending_insns_timestamp.push(iss->top.clock.get_cycles() + 20);
    iss->top.pending_insns_pc.push(pc);
    iss->top.fsm_event.enable();

    iss->ara.insn_enqueue(insn, pc);

    return iss_insn_next(iss, insn, pc);
}
