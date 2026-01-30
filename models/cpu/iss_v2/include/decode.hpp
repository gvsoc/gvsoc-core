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

#include <cpu/iss/include/types.hpp>

class Decode
{
public:
    Decode(Iss &iss);
    void build();
    void reset(bool active);

    void decode_pc(iss_insn_t *insn, iss_reg_t pc);

    vp::Trace trace;

    static void flush_cache_sync(vp::Block *_this, bool active);
    inline bool is_decoded(iss_insn_t *insn);

    // decode
    vp::WireSlave<bool> flush_cache_itf;
    const char *isa;
    bool has_double;

    std::vector<iss_decoder_item_t *> *get_insns_from_tag(std::string tag);
    std::vector<iss_decoder_item_t *> *get_insns_from_isa(std::string tag);
    iss_decoder_item_t *get_insn(std::string name);

private:
    int decode_opcode(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode);
    int decode_item(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_opcode_group(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_insn(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    uint64_t decode_ranges(iss_opcode_t opcode, iss_decoder_range_set_t *range_set, bool is_signed);
    int decode_info(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_arg_info_t *info, bool is_signed);
    void insn_set_in_reg(iss_insn_t *insn, iss_decoder_arg_t *darg, int reg);
    void insn_set_out_reg(iss_insn_t *insn, iss_decoder_arg_t *darg, int reg);

    Iss &iss;
};


typedef struct
{
    const char *tag;
    iss_decoder_item_t **insn;
} iss_tag_insns_t;

typedef struct iss_isa_set_s
{
    iss_decoder_item_t *isa_set;
    std::unordered_map<std::string, std::vector<iss_decoder_item_t *> *> &tag_insns;
    std::unordered_map<std::string, std::vector<iss_decoder_item_t *> *> &isa_insns;
    std::unordered_map<std::string, iss_decoder_item_t *> &insns;
    bool initialized;
} iss_isa_set_t;

extern iss_isa_set_t __iss_isa_set;

static inline iss_isa_set_t *iss_get_isa_set()
{
    return &__iss_isa_set;
}

inline bool Decode::is_decoded(iss_insn_t *insn)
{
    return insn->handler != NULL;
}
