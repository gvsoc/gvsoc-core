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

#include <types.hpp>

class Decode
{
public:
    Decode(Iss &iss);
    void build();

    iss_insn_t *decode_pc(iss_insn_t *pc);

    vp::trace trace;

    static void flush_cache_sync(void *_this, bool active);

    int parse_isa();

private:
    int decode_opcode(iss_insn_t *insn, iss_opcode_t opcode);
    int decode_item(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_opcode_group(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_insn(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    uint64_t decode_ranges(iss_opcode_t opcode, iss_decoder_range_set_t *range_set, bool is_signed);
    int decode_info(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_arg_info_t *info, bool is_signed);

    Iss &iss;
};
