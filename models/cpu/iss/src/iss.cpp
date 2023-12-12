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

    this->iss.gdbserver.start();
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
#ifdef CONFIG_GVSOC_ISS_SNITCH
    this->iss.spatz.reset(active);
#endif
}

IssWrapper::IssWrapper(vp::ComponentConf &config)
    : vp::Component(config), iss(*this)
{
    this->iss.syscalls.build();
    this->iss.decode.build();
    this->iss.exec.build();
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


#ifdef CONFIG_GVSOC_ISS_SNITCH
    this->iss.spatz.build();
#endif

    traces.new_trace("wrapper", &this->trace, vp::DEBUG);
}
