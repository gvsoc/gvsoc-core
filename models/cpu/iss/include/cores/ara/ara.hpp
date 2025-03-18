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


class Ara : public vp::Block
{
public:
    Ara(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool reset);

    void insn_enqueue(iss_insn_t *insn, iss_reg_t pc);
    bool queue_is_full() { return this->queue_full.get(); }
    iss_reg_t base_addr_get() { return this->base_addr_queue.front(); }

    int stall_reg;
    std::queue<uint64_t> base_addr_queue;

private:
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    Iss &iss;

    vp::ClockEvent fsm_event;
    uint8_t queue_size;
    vp::Register<uint8_t> nb_pending_insn;
    std::queue<iss_insn_t *> pending_insns;
    std::queue<int64_t> pending_insns_timestamp;
    std::queue<iss_addr_t> pending_insns_pc;
    vp::Register<bool> queue_full;

};
