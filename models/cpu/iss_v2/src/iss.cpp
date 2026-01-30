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

#include "cpu/iss_v2/include/iss.hpp"



void Iss::start()
{
    this->trace.start();
    this->syscalls.start();
    this->gdbserver.start();
    this->vector.start();

    this->arch.start();
    this->exec.start();
    this->regfile.start();
    this->csr.start();
    this->prefetch.start();
    this->lsu.start();
    this->core.start();
    this->irq.start();
    this->exception.start();
    this->mmu.start();
    this->timing.start();
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    this->offload.start();
#endif
}

void Iss::stop()
{
    this->insn_cache.stop();
    this->trace.stop();
    this->syscalls.stop();
    this->gdbserver.stop();
    this->vector.stop();

    this->arch.stop();
    this->exec.stop();
    this->regfile.stop();
    this->csr.stop();
    this->prefetch.stop();
    this->lsu.stop();
    this->core.stop();
    this->irq.stop();
    this->exception.stop();
    this->mmu.stop();
    this->timing.stop();
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    this->offload.stop();
#endif
}

void Iss::reset(bool active)
{
    this->insn_cache.reset(active);
    this->trace.reset(active);
    this->syscalls.reset(active);
    this->gdbserver.reset(active);
    this->vector.reset(active);

    this->arch.reset(active);
    this->exec.reset(active);
    this->regfile.reset(active);
    this->csr.reset(active);
    this->prefetch.reset(active);
    this->lsu.reset(active);
    this->core.reset(active);
    this->irq.reset(active);
    this->exception.reset(active);
    this->mmu.reset(active);
    this->timing.reset(active);
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    this->offload.reset(active);
#endif
}

Iss::Iss(vp::ComponentConf &config)
: vp::Component(config), insn_cache(*this), decode(*this), trace(*this), syscalls(*this), gdbserver(*this),
vector(*this), exec(*this), regfile(*this),
csr(*this), prefetch(*this), lsu(*this), core(*this), irq(*this), exception(*this), mmu(*this), timing(*this), arch(*this)
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
, offload(*this)
#endif
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new Iss(config);
}
