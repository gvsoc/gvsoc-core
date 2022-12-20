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

#include "iss.hpp"
#include <string.h>

int Iss::build()
{
    this->syscalls.build();
    this->decode.build();
    this->exec.build();
    this->dbgunit.build();
    this->csr.build();
    this->lsu.build();
    this->irq.build();
    this->trace.build();
    this->timing.build();
    this->gdbserver.build();
    this->exception.build();
    this->irq.build();
    this->prefetcher.build();

    traces.new_trace("wrapper", this->get_trace(), vp::DEBUG);

    this->target_open();

    return 0;
}

void Iss::start()
{

    vp_assert_always(this->lsu.data.is_bound(), this->get_trace(), "Data master port is not connected\n");
    vp_assert_always(this->prefetcher.fetch_itf.is_bound(), this->get_trace(), "Fetch master port is not connected\n");
    // vp_assert_always(this->irq_ack_itf.is_bound(), &this->trace, "IRQ ack master port is not connected\n");

    this->get_trace()->msg("ISS start (fetch: %d, is_active: %d, boot_addr: 0x%lx)\n",
        exec.fetch_enable_reg.get(), exec.is_active_reg.get(), get_config_int("boot_addr"));

    this->timing.background_power.leakage_power_start();
    this->timing.background_power.dynamic_power_start();

    this->gdbserver.start();
}

void Iss::reset(bool active)
{
    this->prefetcher.reset(active);
    this->exec.reset(active);
    this->irq.reset(active);
    this->lsu.reset(active);
    this->timing.reset(active);
    this->trace.reset(active);
    this->regfile.reset(active);
    this->decode.reset(active);
}

Iss::Iss(js::config *config)
    : vp::component(config), prefetcher(*this), exec(*this), decode(*this), timing(*this), irq(*this),
      gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(*this), trace(*this), csr(*this), regfile(*this), exception(*this)
{
}

void Iss::target_open()
{

}
