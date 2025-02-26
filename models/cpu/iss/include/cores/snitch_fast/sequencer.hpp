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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#pragma once

#include <deque>
#include "cpu/iss/include/types.hpp"

class IssWrapper;

struct SequencerFrepEntry
{
    bool is_inner;
    iss_reg_t max_rpt;
    iss_reg_t max_inst;
    iss_reg_t stagger_max;
    iss_reg_t stagger_mask;
    int insn_count;
};

class Sequencer : public vp::Block
{
public:

    Sequencer(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool active);

    iss_reg_t frep_handle(iss_insn_t *insn, iss_reg_t pc, bool is_inner);
    inline iss_reg_t lsu_pop_base()
    {
        return this->lsu_buffer[this->insn_count];
    }

private:
    static iss_reg_t non_float_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    static iss_reg_t sequence_buffer_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    static iss_reg_t direct_branch_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    static void fsm_handler(vp::Block *block, vp::ClockEvent *event);
    void frep_pop_check();

    Iss &iss;
    vp::Trace trace;

    vp::ClockEvent fsm_event;

    int total_insn_count;
    int total_insn_read_count;
    int insn_count;
    int rpt_count;
    std::deque<iss_insn_t *> buffer;
    std::deque<iss_reg_t> lsu_buffer;
    std::deque<SequencerFrepEntry> frep_buffer;
    int stall_reg;
    iss_insn_t *input_insn;
    bool stalled_insn;
    bool frep_enabled;
    SequencerFrepEntry frep_current;
    bool input_insn_is_sequence;
    iss_reg_t input_insn_reg;
};
