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

    // Tells at which cycle each float register is available.
    int64_t scoreboard_freg_timestamp[ISS_NB_FREGS];

private:
    // Insn stub handler for loads and stores to synchronize snitch with spatz VLSU
    static iss_reg_t load_store_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    // Insn stub handler to track dependencies between scalar and vector float operations
    static iss_reg_t float_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

    Iss &iss;
    vp::Trace trace;
};
