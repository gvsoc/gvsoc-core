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
}
