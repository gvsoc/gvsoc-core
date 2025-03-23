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

class AraBlock : public vp::Block
{
public:
    AraBlock(Block *parent, std::string name) : vp::Block(parent, name) {}
    virtual bool is_full() = 0;
    virtual void enqueue_insn(PendingInsn *pending_insn) = 0;
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
    vp::Trace trace_active_box;
    vp::Trace trace_active;
    vp::Trace trace_addr;
    vp::Trace trace_size;
    vp::Trace trace_is_write;
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
        nb_blocks
    } blocks_e;

    Ara(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool reset);
    void isa_init();

    void insn_end(PendingInsn *insn);
    void insn_enqueue(PendingInsn *insn);
    bool queue_is_full() { return this->queue_full.get(); }
    iss_reg_t base_addr_get() { return this->base_addr_queue.front(); }

    Iss &iss;

    std::queue<uint64_t> base_addr_queue;
    PendingInsn *pending_insn;

private:
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::Trace trace;
    vp::Trace trace_active;
    vp::Trace trace_active_box;
    int nb_lanes;
    vp::ClockEvent fsm_event;
    uint8_t queue_size;
    vp::Register<uint8_t> nb_pending_insn;
    std::queue<PendingInsn *> pending_insns;
    vp::Register<bool> queue_full;
    std::vector<AraBlock *> blocks;
    int64_t scoreboard_valid_ts[ISS_NB_VREGS];
    bool scoreboard_in_use[ISS_NB_VREGS];
    int on_going_insn_iter;
};
