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

extern iss_isa_tag_t __iss_isa_tags[];

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
    this->parse_isa();
    insn_cache_init(&this->iss);
}

void Decode::reset(bool active)
{
    if (active)
    {
        iss_cache_flush(&this->iss);
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

                insn->out_regs[darg->u.reg.id] = arg->u.reg.index;

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
                iss_reg_t next_pc = pc + insn->size;
                iss_insn_t *next = insn_cache_get_insn(&this->iss, next_pc);
                if (!next)
                {
                    return -2;
                }

                if (!insn_cache_is_decoded(&this->iss, next) && !iss_decode_insn(&this->iss, next, next_pc))
                {
                    return -2;
                }

                // We can stall the next instruction either if latency is superior
                // to 2 (due to number of pipeline stages) or if there is a data
                // dependency

                // Go through the registers and set the handler to the stall handler
                // in case we find a register dependency so that we can properly
                // handle the stall
                bool set_pipe_latency = true;
                for (int j = 0; j < next->nb_in_reg; j++)
                {
                    if (next->in_regs[j] == arg->u.reg.index)
                    {
                        insn->latency += darg->u.reg.latency;
                        set_pipe_latency = false;
                        break;
                    }
                }

                // If no dependency was found, apply the one for the pipeline stages
                if (set_pipe_latency && darg->u.reg.latency > PIPELINE_STAGES)
                {
                    next->latency += darg->u.reg.latency - PIPELINE_STAGES + 1;
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

    if (insn->hwloop_handler != NULL)
    {
        iss_reg_t (*hwloop_handler)(Iss *, iss_insn_t *, iss_reg_t pc) = insn->hwloop_handler;
        insn->hwloop_handler = insn->handler;
        insn->handler = hwloop_handler;
        insn->fast_handler = hwloop_handler;
    }

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

    if (insn->latency)
    {
        insn->stall_handler = insn->handler;
        insn->stall_fast_handler = insn->fast_handler;
        insn->handler = this->iss.exec.insn_stalled_callback_get();
        insn->fast_handler = this->iss.exec.insn_stalled_fast_callback_get();
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

iss_decoder_item_t *iss_isa_get(Iss *iss, const char *name)
{
    for (int i = 0; i < __iss_isa_set.nb_isa; i++)
    {
        iss_isa_t *isa = &__iss_isa_set.isa_set[i];

        if (strcmp(isa->name, name) == 0)
        {
            return isa->tree;
        }
    }

    return NULL;
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
    for (int i = 0; i < __iss_isa_set.nb_isa; i++)
    {
        iss_isa_t *isa = &__iss_isa_set.isa_set[i];
        int err = this->decode_item(insn, pc, opcode, isa->tree);
        if (err == 0)
        {
            return 0;
        }
        else if (err == -2)
        {
            return -2;
        }
    }

    return -1;
}



void iss_decode_activate_isa(Iss *cpu, char *name)
{
    iss_isa_tag_t *isa = &__iss_isa_tags[0];
    while (isa->name)
    {
        if (strcmp(isa->name, name) == 0)
        {
            iss_decoder_item_t **insn_ptr = isa->insns;
            while (*insn_ptr)
            {
                iss_decoder_item_t *insn = *insn_ptr;
                insn->is_active = true;
                insn_ptr++;
            }
        }
        isa++;
    }
}



static iss_reg_t iss_exec_insn_illegal(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss->decode.trace.msg("Executing illegal instruction\n");
    iss->exception.raise(pc, ISS_EXCEPT_ILLEGAL);
    return pc;
}



bool Decode::decode_pc(iss_insn_t *insn, iss_reg_t pc)
{
    this->trace.msg("Decoding instruction (pc: 0x%lx)\n", pc);

    iss_opcode_t opcode = insn->opcode;

    this->trace.msg("Got opcode (opcode: 0x%lx)\n", opcode);

    int err = this->decode_opcode(insn, pc, opcode);

    if (err == -1)
    {
        this->trace.msg("Unknown instruction\n");
        insn->handler = iss_exec_insn_illegal;
        insn->fast_handler = iss_exec_insn_illegal;
        return true;
    }
    else if (err == -2)
    {
        return false;
    }

    insn->opcode = opcode;

    if (iss.trace.insn_trace.get_active() || iss.timing.insn_trace_event.get_event_active())
    {
        insn->saved_handler = insn->handler;
        insn->handler = this->iss.exec.insn_trace_callback_get();
        insn->fast_handler = this->iss.exec.insn_trace_callback_get();
    }

    return true;
}



bool iss_decode_insn(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (!insn->fetched)
    {
        if (!iss->prefetcher.fetch(pc))
        {
            return false;
        }

        insn->fetched = true;
    }

    if (!iss->decode.decode_pc(insn, pc))
    {
        return false;
    }

    return true;
}



iss_reg_t iss_decode_pc_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (!iss_decode_insn(iss, insn, pc))
    {
        return pc;
    }

    return iss->exec.insn_exec(insn, pc);
}



void Decode::parse_isa()
{
    // TODO this should now be all moved to python generator
    Iss *iss = &this->iss;
    const char *current = iss->decode.isa;
    int len = strlen(current);

    bool arch_rv32 = false;
    bool arch_rv64 = false;

    if (strncmp(current, "rv32", 4) == 0)
    {
        current += 4;
        len -= 4;
        arch_rv32 = true;
    }
    else if (strncmp(current, "rv64", 4) == 0)
    {
        current += 4;
        len -= 4;
        arch_rv64 = true;
    }
    else
    {
        throw std::runtime_error("Unsupported ISA: " + std::string(current));
    }

    iss_decode_activate_isa(iss, (char *)"priv");
    iss_decode_activate_isa(iss, (char *)"trap_return");
    iss_decode_activate_isa(iss, (char *)"priv_smmu");

    bool has_f = false;
    bool has_d = false;
    bool has_c = false;
    bool has_f16 = false;
    bool has_f16alt = false;
    bool has_f8 = false;
    bool has_fvec = false;
    bool has_faux = false;
    uint32_t misa = 0;

    while (len > 0)
    {
        switch (*current)
        {
        case 'd':
            misa |= 1 << 3;
            has_d = true; // D needs F
        case 'f':
            misa |= 1 << 5;
            has_f = true;
        case 'a':
            misa |= 1 << 0;
        case 'v':
        case 'i':
            misa |= 1 << 8;
        case 'm':
        {
            misa |= 1 << 12;
            char name[2];
            name[0] = *current;
            name[1] = 0;
            iss_decode_activate_isa(iss, name);
            current++;
            len--;
            break;
        }
        case 'c':
        {
            misa |= 1 << 2;
            iss_decode_activate_isa(iss, (char *)"c");
            current++;
            len--;
            has_c = true;
            break;
        }
        case 'X':
        {
            char *token = strtok(strdup(current), "X");

            while (token)
            {
                iss_decode_activate_isa(iss, token);

                if (strcmp(token, "pulpv2") == 0)
                {
#ifdef CONFIG_GVSOC_ISS_RI5KY
                    iss_isa_pulpv2_activate(iss);
#endif
                }
                else if (strcmp(token, "corev") == 0)
                {
#ifdef CONFIG_GVSOC_ISS_RI5KY
                    iss_isa_corev_activate(iss);
#endif
                }
                else if (strcmp(token, "gap8") == 0)
                {
#ifdef CONFIG_GVSOC_ISS_RI5KY
                    iss_isa_pulpv2_activate(iss);
                    iss_decode_activate_isa(iss, (char *)"pulpv2");
#endif
                }
                else if (strcmp(token, "f16") == 0)
                {
                    has_f16 = true;
                }
                else if (strcmp(token, "f16alt") == 0)
                {
                    has_f16alt = true;
                }
                else if (strcmp(token, "f8") == 0)
                {
                    has_f8 = true;
                }
                else if (strcmp(token, "fvec") == 0)
                {
                    has_fvec = true;
                }
                else if (strcmp(token, "faux") == 0)
                {
                    has_faux = true;
                }

                token = strtok(NULL, "X");
            }

            len = 0;

            break;
        }
        default:
            throw std::runtime_error("Unknwon ISA descriptor: " + *current);
        }
    }

    //
    // Activate inter-dependent ISA extension subsets
    //

    // Compressed floating-point instructions
    if (has_c)
    {
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"cf");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"cd");
    }

    // For F Extension
    if (has_f)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f");
        // Vectors
        if (has_fvec && has_d)
        { // make sure FLEN >= 64
            iss_decode_activate_isa(iss, (char *)"f32vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f32vecno32d");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            // nothing for scalars as expansions are to fp32
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f32auxvec");
        }
    }

    if (has_d)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64d");
    }

    this->has_double = has_d;

    // For Xf16 Extension
    if (has_f16)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f16");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f16f");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f16d");
        // Vectors
        if (has_fvec && has_f)
        { // make sure FLEN >= 32
            iss_decode_activate_isa(iss, (char *)"f16vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f16vecno32d");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f16vecd");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f16aux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f16auxvec");
        }
    }

    // For Xf16alt Extension
    if (has_f16alt)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f16alt");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f16altf");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f16altd");
        if (has_f16)
            iss_decode_activate_isa(iss, (char *)"f16altf16");
        // Vectors
        if (has_fvec && has_f)
        { // make sure FLEN >= 32
            iss_decode_activate_isa(iss, (char *)"f16altvec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f16altvecno32d");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f16altvecd");
            if (has_f16)
                iss_decode_activate_isa(iss, (char *)"f16altvecf16");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f16altaux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f16altauxvec");
        }
    }

    // For Xf8 Extension
    if (has_f8)
    {
        if (arch_rv64)
            iss_decode_activate_isa(iss, (char *)"rv64f8");
        if (has_f)
            iss_decode_activate_isa(iss, (char *)"f8f");
        if (has_d)
            iss_decode_activate_isa(iss, (char *)"f8d");
        if (has_f16)
            iss_decode_activate_isa(iss, (char *)"f8f16");
        if (has_f16alt)
            iss_decode_activate_isa(iss, (char *)"f8f16alt");
        // Vectors
        if (has_fvec && (has_f16 || has_f16alt || has_f))
        { // make sure FLEN >= 16
            iss_decode_activate_isa(iss, (char *)"f8vec");
            if (!(arch_rv32 && has_d))
                iss_decode_activate_isa(iss, (char *)"f8vecno32d");
            if (has_f)
                iss_decode_activate_isa(iss, (char *)"f8vecf");
            if (has_d)
                iss_decode_activate_isa(iss, (char *)"f8vecd");
            if (has_f16)
                iss_decode_activate_isa(iss, (char *)"f8vecf16");
            if (has_f16alt)
                iss_decode_activate_isa(iss, (char *)"f8vecf16alt");
        }
        // Auxiliary Ops
        if (has_faux)
        {
            iss_decode_activate_isa(iss, (char *)"f8aux");
            if (has_fvec)
                iss_decode_activate_isa(iss, (char *)"f8auxvec");
        }
    }

#ifdef CONFIG_GVSOC_ISS_SUPERVISOR_MODE
    misa |= 1 << 18;
#endif

#ifdef CONFIG_GVSOC_ISS_USER_MODE
    misa |= 1 << 20;
#endif

    this->misa_extensions = misa;
}
