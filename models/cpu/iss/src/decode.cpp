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

#include "cpu/iss/include/iss.hpp"
#include <string.h>
#include <stdexcept>

Decode::Decode(Iss &iss)
    : iss(iss)
{
}

void Decode::build()
{
    iss.top.traces.new_trace("decoder", &this->trace, vp::DEBUG);
    this->flush_cache_itf.set_sync_meth(&Decode::flush_cache_sync);
    this->iss.top.new_slave_port("flush_cache", &this->flush_cache_itf, (vp::Block *)this);
    string isa = this->iss.top.get_js_config()->get_child_str("isa");
    this->isa = strdup(isa.c_str());
    this->has_double = this->iss.top.get_js_config()->get_child_bool("has_double");
}

void Decode::reset(bool active)
{
    if (active)
    {
        this->iss.insn_cache.flush();
    }
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

    insn->expand_table = NULL;
    insn->latency = 0;
    insn->resource_id = item->u.insn.resource_id;
    insn->resource_latency = item->u.insn.resource_latency;
    insn->resource_bandwidth = item->u.insn.resource_bandwidth;

    insn->decoder_item = item;
    insn->size = item->u.insn.size;
    insn->nb_out_reg = 0;
    insn->nb_in_reg = 0;
    insn->latency = item->u.insn.latency;

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
                arg->u.reg.index += 8;

            if (darg->type == ISS_DECODER_ARG_TYPE_IN_REG)
            {
                if (darg->u.reg.id >= insn->nb_in_reg)
                    insn->nb_in_reg = darg->u.reg.id + 1;

                insn->in_regs[darg->u.reg.id] = arg->u.reg.index;

                if (darg->flags & ISS_DECODER_ARG_FLAG_FREG)
                {
                    insn->in_regs_ref[darg->u.reg.id] = this->iss.regfile.freg_ref(arg->u.reg.index);
                }
                else
                {
                    insn->in_regs_ref[darg->u.reg.id] = this->iss.regfile.reg_ref(arg->u.reg.index);
                }
            }
            else
            {
                if (darg->u.reg.id >= insn->nb_out_reg)
                    insn->nb_out_reg = darg->u.reg.id + 1;

                if (arg->u.reg.index == 0 && !(darg->flags & ISS_DECODER_ARG_FLAG_FREG))
                {
                    insn->out_regs[darg->u.reg.id] = ISS_NB_REGS;
                }
                else
                {
                    insn->out_regs[darg->u.reg.id] = arg->u.reg.index;
                }

                if (darg->flags & ISS_DECODER_ARG_FLAG_FREG)
                {
                    insn->out_regs_ref[darg->u.reg.id] = this->iss.regfile.freg_store_ref(arg->u.reg.index);
                }
                else
                {
                    if (darg->u.reg.id == 0)
                    {
                        insn->out_regs_ref[darg->u.reg.id] = this->iss.regfile.reg_store_ref(arg->u.reg.index);
                    }
                    else
                    {
                        insn->out_regs_ref[darg->u.reg.id] = &null_reg;
                    }
                }
            }

            if (darg->type == ISS_DECODER_ARG_TYPE_OUT_REG && darg->u.reg.latency != 0)
            {
                iss_reg_t index;

                // We can stall the next instruction either if latency is superior
                // to 2 (due to number of pipeline stages) or if there is a data
                // dependency

                // Go through the registers and set the handler to the stall handler
                // in case we find a register dependency so that we can properly
                // handle the stall

#if defined(CONFIG_GVSOC_ISS_TIMED)
                // If no dependency was found, apply the one for the pipeline stages
                if (darg->u.reg.latency != 0)
                {
                    if (insn->stall_handler == NULL)
                    {
                        insn->stall_handler = insn->handler;
                        insn->stall_fast_handler = insn->fast_handler;
                        insn->handler = this->iss.exec.insn_stalled_callback_get();
                        insn->fast_handler = this->iss.exec.insn_stalled_fast_callback_get();
                    }
                }
#endif
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
            insn->in_regs_ref[darg->u.indirect_imm.reg.id] = this->iss.regfile.reg_ref(arg->u.indirect_imm.reg_index);
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
            insn->in_regs_ref[darg->u.indirect_reg.base_reg.id] = this->iss.regfile.reg_ref(arg->u.indirect_reg.base_reg_index);
            if (darg->u.indirect_reg.base_reg.id >= insn->nb_in_reg)
                insn->nb_in_reg = darg->u.indirect_reg.base_reg.id + 1;

            arg->u.indirect_reg.offset_reg_index = this->decode_info(insn, opcode, &darg->u.indirect_reg.offset_reg.info, false);
            if (darg->u.indirect_reg.offset_reg.flags & ISS_DECODER_ARG_FLAG_COMPRESSED)
                arg->u.indirect_reg.offset_reg_index += 8;
            insn->in_regs[darg->u.indirect_reg.offset_reg.id] = arg->u.indirect_reg.offset_reg_index;
            insn->in_regs_ref[darg->u.indirect_reg.offset_reg.id] = this->iss.regfile.reg_ref(arg->u.indirect_reg.offset_reg_index);
            if (darg->u.indirect_reg.offset_reg.id >= insn->nb_in_reg)
                insn->nb_in_reg = darg->u.indirect_reg.offset_reg.id + 1;

            break;
        }
    }

    insn->fast_handler = item->u.insn.fast_handler;
    insn->handler = item->u.insn.handler;

#if defined(CONFIG_GVSOC_ISS_RI5KY)
    if (insn->hwloop_handler != NULL)
    {
        iss_reg_t (*hwloop_handler)(Iss *, iss_insn_t *, iss_reg_t pc) = insn->hwloop_handler;
        insn->hwloop_handler = insn->handler;
        insn->handler = hwloop_handler;
        insn->fast_handler = hwloop_handler;
    }
#endif

    this->iss.gdbserver.decode_insn(insn, pc);
    this->iss.exec.decode_insn(insn, pc);

    if (item->u.insn.resource_id != -1)
    {
        insn->resource_handler = insn->handler;
        insn->fast_handler = iss_resource_offload;
        insn->handler = iss_resource_offload;
    }

    insn->is_macro_op = item->u.insn.is_macro_op;

    if (item->u.insn.decode != NULL)
    {
        item->u.insn.decode(&this->iss, insn, pc);
    }

#if defined(CONFIG_GVSOC_ISS_TIMED)
    if (insn->latency)
    {
        insn->stall_handler = insn->handler;
        insn->stall_fast_handler = insn->fast_handler;
        insn->handler = this->iss.exec.insn_stalled_callback_get();
        insn->fast_handler = this->iss.exec.insn_stalled_fast_callback_get();
    }
#endif

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

    if (iss.trace.insn_trace.get_active() || iss.timing.insn_trace_event.get_event_active())
    {
        insn->saved_handler = insn->handler;
        insn->handler = this->iss.exec.insn_trace_callback_get();
        insn->fast_handler = this->iss.exec.insn_trace_callback_get();
    }
}



bool Decode::iss_decode_insn(iss_insn_t *insn, iss_reg_t pc)
{
    if (!insn->fetched)
    {
        if (!this->iss.prefetcher.fetch(pc))
        {
            return false;
        }

        insn->fetched = true;
    }

    this->decode_pc(insn, pc);

    return true;
}



iss_reg_t iss_decode_pc_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (!iss->decode.iss_decode_insn(insn, pc))
    {
        return pc;
    }

    return iss->exec.insn_exec(insn, pc);
}

std::vector<iss_decoder_item_t *> *Decode::get_insns_from_tag(std::string tag)
{
    return __iss_isa_set.tag_insns[tag];
}