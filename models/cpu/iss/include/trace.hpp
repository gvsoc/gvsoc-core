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


#pragma once

#include <vp/vp.hpp>
#include <cpu/iss/include/types.hpp>



class Trace
{
public:
    Trace(Iss &iss);

    void build();
    void reset(bool active);

    void insn_trace_callback();
    void dump_debug_traces();

    // This will skip the dump of the current instruction. This is set back to false
    // immediately after current instruction is executed so that only once instruction dump
    // is skipped
    bool skip_insn_dump;
    // Contrary to skip_insn_dump. this can permanently disabled instruction dump until it is
    // explicitely reactivated
    bool dump_trace_enabled;
    // Used by some features like coprocessor to enable trace dump while main core is stalled
    // and blocking traces
    bool force_trace_dump;

    vp::Trace insn_trace;
    iss_insn_arg_t saved_args[ISS_MAX_DECODE_ARGS];
    iss_insn_arg_t check_args[ISS_MAX_DECODE_ARGS];
#ifdef CONFIG_ISS_HAS_VECTOR
    uint8_t saved_vargs[ISS_MAX_DECODE_ARGS][CONFIG_ISS_VLEN/8];
    uint8_t check_vargs[ISS_MAX_DECODE_ARGS][CONFIG_ISS_VLEN/8];
#endif
    int priv_mode;
    bool has_reg_dump = false;
    iss_reg_t reg_dump;
    bool has_str_dump = false;
    std::string str_dump;

private:

    Iss &iss;
};
