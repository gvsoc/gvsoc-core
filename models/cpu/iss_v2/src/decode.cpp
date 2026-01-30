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

#include <string.h>
#include <cpu/iss_v2/include/iss.hpp>
#include <cpu/iss_v2/include/utils.hpp>

Decode::Decode(Iss &iss)
    : iss(iss)
{
    iss.traces.new_trace("decoder", &this->trace, vp::DEBUG);
    // this->flush_cache_itf.set_sync_meth(&Decode::flush_cache_sync);
    this->iss.new_slave_port("flush_cache", &this->flush_cache_itf, (vp::Block *)this);
    string isa = this->iss.get_js_config()->get_child_str("isa");
    this->isa = strdup(isa.c_str());
    this->has_double = this->iss.get_js_config()->get_child_bool("has_double");
}

uint64_t Decode::decode_ranges(iss_opcode_t opcode, iss_decoder_range_set_t *range_set, bool is_signed)
{
    int nb_ranges = range_set->nb_ranges;
    iss_decoder_range_t *ranges = range_set->ranges;
    ;
    uint64_t result = 0;
    int bits = 0;
    for (int i = 0; i < nb_ranges; i++)
    {
        iss_decoder_range_t *range = &ranges[i];
        result |= iss_get_field(opcode, range->bit, range->width) << range->shift;
        int last_bit = range->width + range->shift;
        if (last_bit > bits)
            bits = last_bit;
    }
    if (is_signed)
        result = iss_get_signed_value(result, bits);
    return result;
}

void Decode::insn_set_in_reg(iss_insn_t *insn, iss_decoder_arg_t *darg, int reg)
{
    insn->in_regs[darg->u.reg.id] = reg;
    if (darg->flags & ISS_DECODER_ARG_FLAG_VREG)
    {
        insn->sb_vreg_mask |= 1 << reg;
    }
    else
    {
        insn->sb_reg_mask |= 1 << reg;
    }
}

void Decode::insn_set_out_reg(iss_insn_t *insn, iss_decoder_arg_t *darg, int reg)
{
    insn->out_regs[darg->u.reg.id] = reg;
    if (darg->flags & ISS_DECODER_ARG_FLAG_VREG)
    {
        insn->sb_vreg_mask |= 1 << reg;
        insn->sb_out_vreg_mask |= 1 << reg;
    }
    else
    {
        insn->sb_reg_mask |= 1 << reg;
        insn->sb_out_reg_mask |= 1 << reg;
    }
}

int Decode::decode_info(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_arg_info_t *info, bool is_signed)
{
    if (info->type == ISS_DECODER_VALUE_TYPE_RANGE)
    {
        return this->decode_ranges(opcode, &info->u.range_set, is_signed);
    }
    else if (info->type == ISS_DECODER_VALUE_TYPE_UIM)
    {
        return info->u.uim;
    }
    else if (info->type == ISS_DECODER_VALUE_TYPE_SIM)
    {
        return info->u.sim;
    }

    return 0;
}

int Decode::decode_insn(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item)
{
    if (!item->is_active)
        return -1;

    insn->desc = &item->u.insn;
    insn->expand_table = NULL;
    insn->latency = 0;
    insn->resource_id = item->u.insn.resource_id;
    insn->resource_latency = item->u.insn.resource_latency;
    insn->resource_bandwidth = item->u.insn.resource_bandwidth;
    insn->flags = item->u.insn.flags;

    insn->decoder_item = item;
    insn->size = item->u.insn.size;
    insn->nb_out_reg = 0;
    insn->nb_in_reg = 0;
    insn->latency = item->u.insn.latency;
    insn->sb_reg_mask = 0;
    insn->sb_out_reg_mask = 0;
    insn->sb_vreg_mask = 0;
    insn->sb_out_vreg_mask = 0;

    for (int i = 0; i < item->u.insn.nb_args; i++)
    {
        insn->out_regs[i] = -1;
        insn->in_regs[i] = -1;
    }

    for (int i = 0; i < item->u.insn.nb_args; i++)
    {
        iss_decoder_arg_t *darg = &item->u.insn.args[i];
        iss_insn_arg_t *arg = &insn->args[i];
        arg->type = darg->type;
        arg->flags = darg->flags;

        switch (darg->type)
        {
        case ISS_DECODER_ARG_TYPE_IN_REG:
        case ISS_DECODER_ARG_TYPE_OUT_REG:
            arg->u.reg.index = this->decode_info(insn, opcode, &darg->u.reg.info, false);

            if (darg->flags & ISS_DECODER_ARG_FLAG_COMPRESSED)
            {
                arg->u.reg.index += 8;
            }

            if ((darg->flags & ISS_DECODER_ARG_FLAG_FREG) && !(darg->flags & ISS_DECODER_ARG_FLAG_VREG))
            {
                arg->u.reg.index = this->iss.regfile.get_reg_gid(arg->u.reg.index);
            }

            if (darg->type == ISS_DECODER_ARG_TYPE_IN_REG)
            {
                if (darg->u.reg.id >= insn->nb_in_reg)
                    insn->nb_in_reg = darg->u.reg.id + 1;

                this->insn_set_in_reg(insn, darg, arg->u.reg.index);
            }
            else
            {
                if (darg->u.reg.id >= insn->nb_out_reg)
                    insn->nb_out_reg = darg->u.reg.id + 1;

                if (arg->u.reg.index == 0 && !(darg->flags & ISS_DECODER_ARG_FLAG_VREG))
                {
                    insn->out_regs[darg->u.reg.id] = ISS_DUMMY_REG;
                }
                else
                {
                    this->insn_set_out_reg(insn, darg, arg->u.reg.index);
                }
            }

            break;

        case ISS_DECODER_ARG_TYPE_UIMM:
            arg->u.uim.value = this->decode_ranges(opcode, &darg->u.uimm.info.u.range_set, darg->u.uimm.is_signed);
            insn->uim[darg->u.uimm.id] = arg->u.uim.value;
            break;

        case ISS_DECODER_ARG_TYPE_SIMM:
            arg->u.sim.value = this->decode_ranges(opcode, &darg->u.simm.info.u.range_set, darg->u.simm.is_signed);
            insn->sim[darg->u.simm.id] = arg->u.sim.value;
            break;

        case ISS_DECODER_ARG_TYPE_INDIRECT_IMM:
            arg->u.indirect_imm.reg_index = this->decode_info(insn, opcode, &darg->u.indirect_imm.reg.info, false);
            if (darg->u.indirect_imm.reg.flags & ISS_DECODER_ARG_FLAG_COMPRESSED)
                arg->u.indirect_imm.reg_index += 8;
            insn->in_regs[darg->u.indirect_imm.reg.id] = arg->u.indirect_imm.reg_index;
            insn->sb_reg_mask |= 1 << arg->u.indirect_imm.reg_index;
            if (darg->u.indirect_imm.reg.id >= insn->nb_in_reg)
                insn->nb_in_reg = darg->u.indirect_imm.reg.id + 1;
            arg->u.indirect_imm.imm = this->decode_info(insn, opcode, &darg->u.indirect_imm.imm.info, darg->u.indirect_imm.imm.is_signed);
            insn->sim[darg->u.indirect_imm.imm.id] = arg->u.indirect_imm.imm;
            break;

        case ISS_DECODER_ARG_TYPE_INDIRECT_REG:
            arg->u.indirect_reg.base_reg_index = this->decode_info(insn, opcode, &darg->u.indirect_reg.base_reg.info, false);
            if (darg->u.indirect_reg.base_reg.flags & ISS_DECODER_ARG_FLAG_COMPRESSED)
                arg->u.indirect_reg.base_reg_index += 8;
            insn->in_regs[darg->u.indirect_reg.base_reg.id] = arg->u.indirect_reg.base_reg_index;
            insn->sb_reg_mask |= 1 << arg->u.indirect_reg.base_reg_index;
            if (darg->u.indirect_reg.base_reg.id >= insn->nb_in_reg)
                insn->nb_in_reg = darg->u.indirect_reg.base_reg.id + 1;

            arg->u.indirect_reg.offset_reg_index = this->decode_info(insn, opcode, &darg->u.indirect_reg.offset_reg.info, false);
            if (darg->u.indirect_reg.offset_reg.flags & ISS_DECODER_ARG_FLAG_COMPRESSED)
                arg->u.indirect_reg.offset_reg_index += 8;
            insn->in_regs[darg->u.indirect_reg.offset_reg.id] = arg->u.indirect_reg.offset_reg_index;
            insn->sb_reg_mask |= 1 << arg->u.indirect_reg.offset_reg_index;
            if (darg->u.indirect_reg.offset_reg.id >= insn->nb_in_reg)
                insn->nb_in_reg = darg->u.indirect_reg.offset_reg.id + 1;

            break;

        default:
            break;
        }
    }

    insn->fast_handler = item->u.insn.fast_handler;
    insn->handler = item->u.insn.handler;

    this->iss.gdbserver.decode_insn(insn, pc);

// #if defined(CONFIG_GVSOC_ISS_TIMED)
//     if (item->u.insn.resource_id != -1)
//     {
//         insn->resource_handler = insn->handler;
//         insn->fast_handler = iss_resource_offload;
//         insn->handler = iss_resource_offload;
//     }
// #endif

    insn->is_macro_op = item->u.insn.is_macro_op;

    if (item->u.insn.decode != NULL)
    {
        item->u.insn.decode(&this->iss, insn, pc);
    }

    if (item->u.insn.stub_handler != NULL)
    {
        insn->stub_handler = insn->handler;
        insn->handler = item->u.insn.stub_handler;
        insn->fast_handler = item->u.insn.stub_handler;
    }

    return 0;
}

int Decode::decode_opcode_group(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item)
{
    iss_opcode_t group_opcode = (opcode >> item->u.group.bit) & ((1ULL << item->u.group.width) - 1);
    iss_decoder_item_t *group_item_other = NULL;

    for (int i = 0; i < item->u.group.nb_groups; i++)
    {
        iss_decoder_item_t *group_item = item->u.group.groups[i];
        if (group_opcode == group_item->opcode && !group_item->opcode_others)
        {
            return this->decode_item(insn, pc, opcode, group_item);
        }
        if (group_item->opcode_others)
            group_item_other = group_item;
    }

    if (group_item_other)
        return this->decode_item(insn, pc, opcode, group_item_other);

    return -1;
}

iss_decoder_item_t *iss_isa_get(Iss *iss)
{
    return __iss_isa_set.isa_set;
}

int Decode::decode_item(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode, iss_decoder_item_t *item)
{
    if (item->is_insn)
        return this->decode_insn(insn, pc, opcode, item);
    else
        return this->decode_opcode_group(insn, pc, opcode, item);
}

int Decode::decode_opcode(iss_insn_t *insn, iss_reg_t pc, iss_opcode_t opcode)
{
    return this->decode_item(insn, pc, opcode, __iss_isa_set.isa_set);
}



static iss_reg_t iss_exec_insn_illegal(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->decode.trace.msg("Executing illegal instruction\n");
    iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
    return pc;
}



void Decode::decode_pc(iss_insn_t *insn, iss_reg_t pc)
{
    this->trace.msg("Decoding instruction (pc: 0x%lx)\n", pc);

    iss_opcode_t opcode = insn->opcode;

    this->trace.msg("Got opcode (opcode: 0x%lx)\n", opcode);

    if (this->decode_opcode(insn, pc, opcode))
    {
        this->trace.msg("Unknown instruction\n");
        insn->handler = iss_exec_insn_illegal;
        insn->fast_handler = iss_exec_insn_illegal;
        return;
    }

    insn->opcode = opcode;
}


std::vector<iss_decoder_item_t *> *Decode::get_insns_from_tag(std::string tag)
{
    return __iss_isa_set.tag_insns[tag];
}

std::vector<iss_decoder_item_t *> *Decode::get_insns_from_isa(std::string tag)
{
    return __iss_isa_set.isa_insns[tag];
}

iss_decoder_item_t *Decode::get_insn(std::string name)
{
    return __iss_isa_set.insns[name];
}


// void Decode::flush_cache_sync(vp::Block *__this, bool active)
// {
//     Decode *_this = (Decode *)__this;
//     // Delay the flush to the next instruction in case we are in the middle of an instruction
//     _this->iss.exec.pending_flush = true;
//     _this->iss.exec.switch_to_full_mode();
// }
