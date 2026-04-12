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
#if defined(CONFIG_ISS_HAS_VECTOR)
    this->vector.start();
#endif

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
    this->pmp.start();
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
#if defined(CONFIG_ISS_HAS_VECTOR)
    this->vector.stop();
#endif

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
    this->pmp.stop();
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
#if defined(CONFIG_ISS_HAS_VECTOR)
    this->vector.reset(active);
#endif

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
    this->pmp.reset(active);
    this->timing.reset(active);
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    this->offload.reset(active);
#endif
}

Iss::Iss(vp::ComponentConf &config)
: vp::Component(config), insn_cache(*this), decode(*this), trace(*this), syscalls(*this), gdbserver(*this),

#if defined(CONFIG_ISS_HAS_VECTOR)
vector(*this),
#endif
exec(*this), regfile(*this),
csr(*this), prefetch(*this), lsu(*this), core(*this), irq(*this), exception(*this), mmu(*this),
pmp(*this), timing(*this), arch(*this)
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
, offload(*this)
#endif
{
}

std::string Iss::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
    std::vector<std::string> args, std::string req)
{
    if (args.size() >= 2 && args[0] == "breakpoint_insert")
    {
        uint64_t addr = strtoull(args[1].c_str(), NULL, 0);
        this->gdbserver.gdbserver_breakpoint_insert(addr);
        return "ok";
    }
    else if (args.size() >= 2 && args[0] == "breakpoint_remove")
    {
        uint64_t addr = strtoull(args[1].c_str(), NULL, 0);
        this->gdbserver.gdbserver_breakpoint_remove(addr);
        return "ok";
    }
    else if (args.size() >= 1 && args[0] == "breakpoint_status")
    {
        if (this->gdbserver.breakpoint_hit)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "hit=1 addr=0x%lx",
                (unsigned long)this->gdbserver.breakpoint_hit_addr);
            return std::string(buf);
        }
        return "hit=0";
    }
    else if (args.size() >= 1 && args[0] == "resume")
    {
        if (this->exec.halted.get())
        {
            this->gdbserver.breakpoint_hit = false;
            this->exec.halted.set(false);
            this->exec.retain_dec();
        }
        return "ok";
    }

    return "";
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new Iss(config);
}
