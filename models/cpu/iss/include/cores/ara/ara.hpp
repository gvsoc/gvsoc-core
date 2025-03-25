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

#pragma once

#include <queue>
#include "cpu/iss/include/types.hpp"
#include "vp/clock/clock_event.hpp"
#include "vp/register.hpp"

class Ara;
class IssWrapper;
class PendingInsn;

// This represents a generic HW block where vector instructions can be forwarded
class AraBlock : public vp::Block
{
public:
    AraBlock(Block *parent, std::string name) : vp::Block(parent, name) {}
    // False if the block can accept a new instruction
    virtual bool is_full() = 0;
    // Enqueue a new instruction to the block
    virtual void enqueue_insn(PendingInsn *pending_insn) = 0;
};

// Generic block for computation. Used for FPU and sliding to dispatch instructions in parallel
// and assign cost based on vector length, number of lanes and lmul
class AraVcompute : public AraBlock
{
public:
    AraVcompute(Ara &ara, std::string name);
    void reset(bool active) override;
    bool is_full() override { return this->insns.size() == 4; }
    void enqueue_insn(PendingInsn *pending_insn) override;

private:
    // Handler for internal handler
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    Ara &ara;
    // Used for this block system traces
    vp::Trace trace;
    // VCD trace for active state
    vp::Trace trace_active;
    // VCD trace for PC of instruction being processed
    vp::Trace trace_pc;

    vp::Trace trace_label;
    vp::ClockEvent fsm_event;
    std::queue<PendingInsn *> insns;
    PendingInsn *pending_insn;
};

class AraVlsu : public AraBlock
{
public:

    void reset(bool active) override;
    AraVlsu(Ara &ara, int nb_lanes);
    bool is_full() override { return this->insns.size() == 4; }
    void enqueue_insn(PendingInsn *pending_insn) override;

    static void handle_insn_load(AraVlsu *vlsu, iss_insn_t *insn);
    static void handle_insn_store(AraVlsu *vlsu, iss_insn_t *insn);

private:
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    Ara &ara;
    vp::Trace trace;
    vp::Trace trace_active;
    vp::Trace trace_addr;
    vp::Trace trace_size;
    vp::Trace trace_is_write;
    vp::Trace trace_pc;
    vp::Trace trace_label;
    vp::ClockEvent fsm_event;
    int nb_lanes;
    std::queue<PendingInsn *> insns;
    uint8_t *pending_velem;
    iss_addr_t pending_addr;
    bool pending_is_write;
    iss_addr_t pending_elem_size;
    iss_addr_t pending_size;
};

class Ara : public vp::Block
{
public:
    typedef enum
    {
        vlsu_id,
        vfpu_id,
        vslide_id,
        nb_blocks
    } blocks_e;

    Ara(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool reset);
    void isa_init();

    void insn_end(PendingInsn *insn);
    void insn_enqueue(PendingInsn *insn);
    bool queue_is_full() { return this->queue_full.get(); }

    Iss &iss;

    PendingInsn *pending_insn;
    int nb_lanes;

private:
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    PendingInsn *pending_insn_alloc(PendingInsn *cva6_pending_insn);

    vp::Trace trace;
    vp::Trace trace_active;
    vp::Trace trace_active_box;
    vp::Trace trace_label;
    vp::ClockEvent fsm_event;
    uint8_t queue_size;
    vp::Register<uint8_t> nb_pending_insn;
    unsigned int nb_waiting_insn;
    std::vector<PendingInsn> pending_insns;
    int insn_first_enqueued;
    int insn_first;
    int insn_last;
    vp::Register<bool> queue_full;
    std::vector<AraBlock *> blocks;
    int64_t scoreboard_valid_ts[ISS_NB_VREGS];
    unsigned int scoreboard_in_use[ISS_NB_VREGS];
};
