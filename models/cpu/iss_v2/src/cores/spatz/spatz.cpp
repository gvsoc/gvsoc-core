/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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

#include "cpu/iss_v2/include/iss.hpp"

Spatz::Spatz(Iss &iss)
: iss(iss), vu(iss)
{
#ifdef CONFIG_GVSOC_ISS_SNITCH_BARRIER_CSR
    this->iss.traces.new_trace("barrier", &this->barrier_trace, vp::DEBUG);

    this->iss.csr.declare_csr(&this->barrier, "barrier", 0x7C2);
    this->barrier.register_callback(std::bind(&Spatz::barrier_update,
        this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    this->barrier_ack_itf.set_sync_meth(&Spatz::barrier_sync);
    this->iss.new_slave_port("barrier_ack", &this->barrier_ack_itf, (vp::Block *)this);
    this->iss.new_master_port("barrier_req", &this->barrier_req_itf);
#endif
}

void Spatz::start()
{
#ifdef CONFIG_GVSOC_ISS_SPATZ_MULDIV_OFFLOAD
    // Scalar mul / div are offloaded to the vector unit's integer lanes:
    // tag the decoder items so the exec path hands them to
    // SpatzEvents::event_insn_latency_account (multiplies with their
    // latency, divides with a marker resolved from the operands). The
    // decoder table is shared by all the cores, so this is idempotent.
    for (const char *tag : {"mul", "mulh"})
    {
        for (iss_decoder_item_t *item : *this->iss.decode.get_insns_from_tag(tag))
        {
            item->u.insn.latency = SpatzEvents::MUL_LATENCY;
        }
    }
    for (iss_decoder_item_t *item : *this->iss.decode.get_insns_from_tag("div"))
    {
        item->u.insn.latency = SpatzEvents::DIV_TAG_LATENCY;
    }
#endif
}

void Spatz::reset(bool active)
{
    this->vu.reset(active);
#ifdef CONFIG_GVSOC_ISS_SNITCH_BARRIER_CSR
    if (active)
    {
        this->barrier_waiting = false;
        this->barrier_stalled = false;
    }
#endif
}

#ifdef CONFIG_GVSOC_ISS_SNITCH_BARRIER_CSR
bool Spatz::barrier_update(iss_insn_t *insn, bool is_write, iss_reg_t &value)
{
    if (!is_write && this->barrier_req_itf.is_bound())
    {
        this->barrier_trace.msg(vp::Trace::LEVEL_DEBUG, "Entering barrier\n");

        // The last core to arrive gets its ack synchronously, from inside
        // the notification: flag the wait first so that it is seen.
        this->barrier_waiting = true;
        this->barrier_req_itf.sync(1);

        if (this->barrier_waiting)
        {
            // Stall the core until barrier_sync releases it
            this->barrier_stalled = true;
            this->iss.exec.busy_exit();
            this->iss.exec.retain_inc();
        }
    }
    return false;
}

void Spatz::barrier_sync(vp::Block *__this, bool value)
{
    Spatz *_this = (Spatz *)__this;

    _this->barrier_trace.msg(vp::Trace::LEVEL_DEBUG, "Leaving barrier\n");

    _this->barrier_waiting = false;
    if (_this->barrier_stalled)
    {
        _this->barrier_stalled = false;
        _this->iss.exec.busy_enter();
        _this->iss.exec.retain_dec();
    }
}
#endif
