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

class TraceEntry
{
public:
    iss_insn_arg_t saved_args[ISS_MAX_DECODE_ARGS];
#ifdef CONFIG_ISS_HAS_VECTOR
    uint8_t saved_vargs[ISS_MAX_DECODE_ARGS][CONFIG_ISS_VLEN/8*32];
#endif

    TraceEntry *next;
};


class Trace
{
public:
    Trace(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    void insn_trace_callback();
    void dump_debug_traces();

    void insn_trace_start(iss_insn_t *insn, iss_reg_t pc);
    void insn_trace_dump(iss_insn_t *insn, iss_reg_t pc, TraceEntry *entry=NULL);
    void event_dump(iss_insn_t *insn, iss_reg_t pc);

    vp::Trace insn_trace;
    int priv_mode;
    bool has_reg_dump = false;
    iss_reg_t reg_dump;
    bool has_str_dump = false;
    std::string str_dump;
    inline TraceEntry *detach_entry();

private:

    TraceEntry *get_entry();
    void release_entry(TraceEntry *entry);

    Iss &iss;

    TraceEntry *first_entry;

    vp::Trace state_event;
    vp::Trace pc_trace_event;
    vp::Trace active_pc_trace_event;
    vp::Trace func_trace_event;
    vp::Trace inline_trace_event;
    vp::Trace line_trace_event;
    vp::Trace file_trace_event;
    vp::Trace user_func_trace_event;
    vp::Trace user_inline_trace_event;
    vp::Trace user_line_trace_event;
    vp::Trace user_file_trace_event;
    vp::Trace binaries_trace_event;
    vp::Trace pcer_trace_event[32];
    vp::Trace insn_trace_event;
};

void iss_trace_save_args(Iss *iss, iss_insn_t *insn, bool save_out, TraceEntry *entry);
void iss_trace_dump(Iss *iss, iss_insn_t *insn, iss_reg_t pc, TraceEntry *entry);
void iss_trace_init(Iss *iss);
void iss_register_debug_info(Iss *iss, const char *binary);
int iss_trace_pc_info(iss_addr_t addr, const char **func, const char **inline_func, const char **file, int *line);

inline TraceEntry *Trace::detach_entry()
{
    TraceEntry *entry = this->first_entry;
    this->first_entry = entry->next;
    return entry;
}
