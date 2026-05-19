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

#ifndef CONFIG_GVSOC_ISS_HWLOOP_OBJ
// Default hwloop slot is empty (cores that don't enable hardware loops
// pay nothing for the dispatch hook).
#include <cpu/iss_v2/include/hwloop/empty.hpp>
#define CONFIG_GVSOC_ISS_HWLOOP_OBJ HwloopEmpty
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

    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
        std::vector<std::string> args, std::string req) override;

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
    CONFIG_GVSOC_ISS_PMP pmp;
    CONFIG_GVSOC_ISS_EVENT timing;
    CONFIG_GVSOC_ISS_HWLOOP_OBJ hwloop;
    CONFIG_GVSOC_ISS_ARCH arch;
#ifdef CONFIG_GVSOC_ISS_OFFLOAD
    CONFIG_GVSOC_ISS_OFFLOAD offload;
#endif

private:
    void start();
    void reset(bool active);
    void stop();

};

// Inline Regfile methods that need a complete Iss type (the scoreboard
// stall hook reaches into iss.timing.event_scoreboard_stall). Pulled
// in here, after Iss is fully defined, so every iss_v2 build picks up
// the inline body without each core's gen() having to add it via
// add_implem_include.
#include <cpu/iss_v2/include/regfile_implem.hpp>
