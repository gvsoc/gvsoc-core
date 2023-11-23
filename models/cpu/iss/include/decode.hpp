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

    bool decode_pc(iss_insn_t *insn, iss_reg_t pc);

    vp::Trace trace;

    static void flush_cache_sync(vp::Block *_this, bool active);

    void parse_isa();

    // decode
    vp::WireSlave<bool> flush_cache_itf;
    iss_insn_cache_t insn_cache;
    const char *isa;
    iss_reg_t misa_extensions;
    std::vector<iss_insn_t *> insn_tables;
    bool has_double;

private:
    int decode_opcode(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode);
    int decode_item(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_opcode_group(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_insn(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item);
    uint64_t decode_ranges(iss_opcode_t opcode, iss_decoder_range_set_t *range_set, bool is_signed);
    int decode_info(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_arg_info_t *info, bool is_signed);

    Iss &iss;
};
