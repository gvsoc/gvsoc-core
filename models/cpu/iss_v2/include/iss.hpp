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


#pragma once

#include <vp/vp.hpp>
#include <cpu/iss_v2/include/insn_cache.hpp>
#include <cpu/iss_v2/include/decode.hpp>
#include <cpu/iss_v2/include/trace.hpp>
#include <cpu/iss_v2/include/syscalls.hpp>
#include <cpu/iss_v2/include/gdbserver.hpp>
#ifdef CONFIG_ISS_HAS_VECTOR
#include <cpu/iss_v2/include/vector.hpp>
#endif

class Iss;

class EmptyModule
{
public:
    EmptyModule(Iss &top) {}

    void start() {}
    void stop() {}
    void reset(bool active) {}
};

class Iss : public vp::Component
{

public:
    Iss(vp::ComponentConf &config);

    InsnCache insn_cache;
    Decode decode;
    Trace trace;
    Syscalls syscalls;
    Gdbserver gdbserver;
#ifdef CONFIG_ISS_HAS_VECTOR
    Vector vector;
#endif

    CONFIG_GVSOC_ISS_EXEC exec;
    CONFIG_GVSOC_ISS_REGFILE regfile;
    CONFIG_GVSOC_ISS_CSR csr;
    CONFIG_GVSOC_ISS_PREFETCH prefetch;
    CONFIG_GVSOC_ISS_LSU lsu;
    CONFIG_GVSOC_ISS_CORE core;
    CONFIG_GVSOC_ISS_IRQ irq;
    CONFIG_GVSOC_ISS_EXCEPTION exception;
    CONFIG_GVSOC_ISS_MMU mmu;
    CONFIG_GVSOC_ISS_EVENT timing;
    CONFIG_GVSOC_ISS_ARCH arch;
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    CONFIG_GVSOC_ISS_OFFLOAD offload;
#endif

private:
    void start();
    void reset(bool active);
    void stop();

};
