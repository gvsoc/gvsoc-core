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
#include <algorithm>
#include <vector>

Trace::Trace(Iss &iss)
    : iss(iss)
{
}


void Trace::build()
{
    this->iss.top.traces.new_trace("insn", &this->insn_trace, vp::DEBUG);
    this->insn_trace.register_callback(std::bind(&Trace::insn_trace_callback, this));
    iss_trace_init(&this->iss);

    for (auto x : this->iss.top.get_js_config()->get("**/debug_binaries")->get_elems())
    {
        iss_register_debug_info(&this->iss, x->get_str().c_str());
    }

}

void Trace::reset(bool active)
{
    if (active)
    {
        this->dump_trace_enabled = true;
    }
}

#define PC_INFO_ARRAY_SIZE (64 * 1024)

#define MAX_DEBUG_INFO_WIDTH 32

class iss_pc_info
{
public:
    unsigned int base;
    char *func;
    char *inline_func;
    char *file;
    int line;
    iss_pc_info *next;
};

static bool pc_infos_is_init = false;
static iss_pc_info *pc_infos[PC_INFO_ARRAY_SIZE];
static std::vector<std::string> binaries;

static void add_pc_info(unsigned int base, char *func, char *inline_func, char *file, int line)
{
    iss_pc_info *pc_info = new iss_pc_info();

    int index = base & (PC_INFO_ARRAY_SIZE - 1);
    pc_info->next = pc_infos[index];
    pc_infos[index] = pc_info;

    pc_info->base = base;
    pc_info->func = strdup(func);
    pc_info->inline_func = strdup(inline_func);
    pc_info->file = strdup(file);
    pc_info->line = line;
}

static iss_pc_info *get_pc_info(unsigned int base)
{
    int index = base & (PC_INFO_ARRAY_SIZE - 1);

    iss_pc_info *pc_info = pc_infos[index];

    while (pc_info && pc_info->base != base)
    {
        pc_info = pc_info->next;
    }

    return pc_info;
}

int iss_trace_pc_info(iss_addr_t addr, const char **func, const char **inline_func, const char **file, int *line)
{
    iss_pc_info *info = get_pc_info(addr);
    if (info == NULL)
        return -1;

    *func = info->func;
    *inline_func = info->inline_func;
    *file = info->file;
    *line = info->line;

    return 0;
}

void iss_register_debug_info(Iss *iss, const char *binary)
{
    if (std::find(binaries.begin(), binaries.end(), std::string(binary)) != binaries.end())
        return;

    binaries.push_back(std::string(binary));

    FILE *file = fopen(binary, "r");
    if (file != NULL)
    {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, file)) != -1)
        {
            char *token = strtok(line, " ");
            char *tokens[5];
            int index = 0;
            while (token)
            {
                tokens[index++] = token;
                token = strtok(NULL, " ");
            }
            if (index == 5)
                add_pc_info(strtol(tokens[0], NULL, 16), tokens[1], tokens[2], tokens[3], atoi(tokens[4]));
        }
    }
}

static inline char iss_trace_get_mode(int mode)
{
    switch (mode)
    {
    case 0:
        return 'U';
    case 1:
        return 'S';
    case 2:
        return 'H';
    case 3:
        return 'M';
    }
    return ' ';
}

static inline int iss_trace_dump_reg(Iss *iss, iss_insn_t *insn, iss_decoder_arg_t *arg, char *buff, unsigned int reg, bool is_long = true)
{
    if (is_long)
    {
#ifndef ISS_SINGLE_REGFILE
        if (arg->flags & ISS_DECODER_ARG_FLAG_FREG)
        {
            return sprintf(buff, "f%d", reg);
        }
        else
#endif
        {
            if (reg == 0)
            {
                return sprintf(buff, "0");
            }
            else if (reg == 1)
            {
                return sprintf(buff, "ra");
            }
            else if (reg == 2)
            {
                return sprintf(buff, "sp");
            }
            else if (reg >= 8 && reg <= 9)
            {
                return sprintf(buff, "s%d", reg - 8);
            }
            else if (reg >= 18 && reg <= 27)
            {
                return sprintf(buff, "s%d", reg - 16);
            }
            else if (reg == 4)
            {
                return sprintf(buff, "tp");
            }
            else if (reg >= 10 && reg <= 17)
            {
                return sprintf(buff, "a%d", reg - 10);
            }
            else if (reg >= 5 && reg <= 7)
            {
                return sprintf(buff, "t%d", reg - 5);
            }
            else if (reg >= 28 && reg <= 31)
            {
                return sprintf(buff, "t%d", reg - 25);
            }
            else if (reg == 3)
            {
                return sprintf(buff, "gp");
            }
            else if (reg >= ISS_NB_REGS)
            {
                return sprintf(buff, "f%d", reg - ISS_NB_REGS);
            }
        }
    }

    return sprintf(buff, "x%d", reg);
}

static char *iss_trace_dump_reg_value(Iss *iss, iss_insn_t *insn, char *buff, bool is_out, int reg, uint64_t saved_value, iss_decoder_arg_t *arg, iss_decoder_arg_t **prev_arg, bool is_long)
{
    char regStr[16];
    iss_trace_dump_reg(iss, insn, arg, regStr, reg, is_long);
    if (is_long)
        buff += sprintf(buff, "%3.3s", regStr);
    else
        buff += sprintf(buff, "%s", regStr);

    if (is_out)
        buff += sprintf(buff, "=");
    else
        buff += sprintf(buff, ":");
    if (arg->flags & ISS_DECODER_ARG_FLAG_REG64)
        buff += sprintf(buff, "%" PRIxFULLREG64 " ", saved_value);
    else if (arg->flags & ISS_DECODER_ARG_FLAG_FREG)
    {
        if (iss->decode.has_double)
        {
            buff += sprintf(buff, "%16.16lx ", (uint64_t)saved_value);
        }
        else
        {
            buff += sprintf(buff, "%8.8x ", (uint32_t)saved_value);
        }
    }
    else
        buff += sprintf(buff, "%" PRIxFULLREG " ", (iss_reg_t)saved_value);
    return buff;
}

static char *iss_trace_dump_arg_value(Iss *iss, iss_insn_t *insn, char *buff, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_insn_arg_t *saved_arg, iss_decoder_arg_t **prev_arg, int dump_out, bool is_long)
{
    if ((arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG) && (insn_arg->u.reg.index != 0 || arg->flags & ISS_DECODER_ARG_FLAG_FREG))
    {
        if ((dump_out && arg->type == ISS_DECODER_ARG_TYPE_OUT_REG) || (!dump_out && arg->type == ISS_DECODER_ARG_TYPE_IN_REG))
        {
            buff = iss_trace_dump_reg_value(iss, insn, buff, arg->type == ISS_DECODER_ARG_TYPE_OUT_REG, insn_arg->u.reg.index, (arg->flags & ISS_DECODER_ARG_FLAG_REG64) || (arg->flags & ISS_DECODER_ARG_FLAG_FREG) ? saved_arg->u.reg.value_64 : saved_arg->u.reg.value, arg, prev_arg, is_long);
        }
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
    {
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_imm.reg_index, saved_arg->u.indirect_imm.reg_value, arg, prev_arg, is_long);
        iss_addr_t addr;
        if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
        {
            addr = saved_arg->u.indirect_imm.reg_value;
            if (dump_out)
                buff = iss_trace_dump_reg_value(iss, insn, buff, 1, insn_arg->u.indirect_imm.reg_index, addr + insn_arg->u.indirect_imm.imm, arg, prev_arg, is_long);
        }
        else
        {
            addr = saved_arg->u.indirect_imm.reg_value + insn_arg->u.indirect_imm.imm;
        }
        if (!dump_out)
            buff += sprintf(buff, " PA:%" PRIxFULLREG " ", addr);
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
    {
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_reg.offset_reg_index, saved_arg->u.indirect_reg.offset_reg_value, arg, prev_arg, is_long);
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_reg.base_reg_index, saved_arg->u.indirect_reg.base_reg_value, arg, prev_arg, is_long);
        iss_addr_t addr;
        if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
        {
            addr = saved_arg->u.indirect_reg.base_reg_value;
            if (dump_out)
                buff = iss_trace_dump_reg_value(iss, insn, buff, 1, insn_arg->u.indirect_reg.base_reg_index, addr + insn_arg->u.indirect_reg.offset_reg_value, arg, prev_arg, is_long);
        }
        else
        {
            addr = saved_arg->u.indirect_reg.base_reg_value + saved_arg->u.indirect_reg.offset_reg_value;
        }
        if (!dump_out)
            buff += sprintf(buff, " PA:%" PRIxFULLREG " ", addr);
    }
    *prev_arg = arg;
    return buff;
}

static char *iss_trace_dump_arg(Iss *iss, iss_insn_t *insn, char *buff, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_decoder_arg_t **prev_arg, bool is_long)
{
    if (*prev_arg != NULL && (*prev_arg)->type != ISS_DECODER_ARG_TYPE_NONE && (*prev_arg)->type != ISS_DECODER_ARG_TYPE_FLAG && ((arg->type != ISS_DECODER_ARG_TYPE_IN_REG && arg->type != ISS_DECODER_ARG_TYPE_OUT_REG) || arg->u.reg.dump_name))
    {
        if (is_long)
            buff += sprintf(buff, ", ");
        else
            buff += sprintf(buff, ",");
    }

    if (arg->type != ISS_DECODER_ARG_TYPE_NONE)
    {
        if (arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
        {
            if (arg->u.reg.dump_name)
                buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.reg.index, is_long);
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_UIMM)
        {
            if (insn_arg->flags & ISS_DECODER_ARG_FLAG_DUMP_NAME)
            {
                buff += sprintf(buff, "%s", insn_arg->name);
            }
            else
            {
                buff += sprintf(buff, "0x%" PRIxREG, insn_arg->u.uim.value);
            }
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_SIMM)
        {
            if (insn_arg->flags & ISS_DECODER_ARG_FLAG_DUMP_NAME)
            {
                buff += sprintf(buff, "%s", insn_arg->name);
            }
            else
            {
                buff += sprintf(buff, "%" PRIdREG, insn_arg->u.sim.value);
            }
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
        {
            buff += sprintf(buff, "%" PRIdREG "(", insn_arg->u.indirect_imm.imm);
            if (arg->flags & ISS_DECODER_ARG_FLAG_PREINC)
                buff += sprintf(buff, "!");
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_imm.reg_index, is_long);
            if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
                buff += sprintf(buff, "!");
            buff += sprintf(buff, ")");
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
        {
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_reg.offset_reg_index, is_long);
            buff += sprintf(buff, "(");
            if (arg->flags & ISS_DECODER_ARG_FLAG_PREINC)
                buff += sprintf(buff, "!");
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_reg.base_reg_index, is_long);
            if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
                buff += sprintf(buff, "!");
            buff += sprintf(buff, ")");
        }
        *prev_arg = arg;
    }
    return buff;
}

static char *trace_dump_debug(Iss *iss, iss_insn_t *insn, iss_reg_t pc, char *buff)
{
    char *name = (char *)"-";
    char *file = (char *)"-";
    uint32_t line = 0;
    char *inline_func = (char *)"-";
    iss_pc_info *pc_info = get_pc_info(pc);
    if (pc_info)
    {
        name = pc_info->func;
        file = pc_info->file;
        line = pc_info->line;
        inline_func = pc_info->inline_func;
    }

    int line_len = sprintf(buff, ":%d", line);
    if (line_len > 5)
        line_len = 5;
    int max_name_len = MAX_DEBUG_INFO_WIDTH - line_len;

    int len = snprintf(buff, max_name_len + 1, "%s", inline_func);
    if (len > max_name_len)
        len = max_name_len;

    len += sprintf(buff + len, ":%d", line);

    char *start_buff = buff;

    if (len > MAX_DEBUG_INFO_WIDTH)
        len = MAX_DEBUG_INFO_WIDTH;

    for (int i = len; i < MAX_DEBUG_INFO_WIDTH + 1; i++)
    {
        sprintf(buff + i, " ");
    }

    return buff + MAX_DEBUG_INFO_WIDTH + 1;
}

static void iss_trace_dump_insn(Iss *iss, iss_insn_t *insn, iss_reg_t pc, char *buff, int buffer_size, iss_insn_arg_t *saved_args, bool is_long, int mode, bool is_event)
{

    char *init_buff = buff;
    static int max_len = 20;
    static int max_arg_len = 17;
    int len;

    if (is_long)
    {
        if (binaries.size())
            buff = trace_dump_debug(iss, insn, pc, buff);
    }

    if (!is_event)
    {
        buff += sprintf(buff, "%c %" PRIxFULLREG " ", iss_trace_get_mode(mode), pc);
    }

    if (!is_long)
    {
        buff += sprintf(buff, "%" PRIxFULLREG " ", insn->opcode);
    }

    char *start_buff = buff;

    buff += sprintf(buff, "%s ", insn->decoder_item->u.insn.label);

    if (is_long)
    {
        len = buff - start_buff;

        if (len > max_len)
            max_len = len;
        else
        {
            memset(buff, ' ', max_len - len);
            buff += max_len - len;
        }
    }

    iss_decoder_arg_t *prev_arg = NULL;
    start_buff = buff;
    int nb_args = insn->decoder_item->u.insn.nb_args;
    for (int i = 0; i < nb_args; i++)
    {
        buff = iss_trace_dump_arg(iss, insn, buff, &insn->args[i], &insn->decoder_item->u.insn.args[i], &prev_arg, is_long);
    }
    if (nb_args != 0)
        buff += sprintf(buff, " ");

    if (!is_event)
    {
        len = buff - start_buff;

        if (len > max_arg_len)
            max_arg_len = len;
        else
        {
            memset(buff, ' ', max_arg_len - len);
            buff += max_arg_len - len;
        }
    }

    if (!is_event)
    {
        prev_arg = NULL;
        for (int i = 0; i < nb_args; i++)
        {
            buff = iss_trace_dump_arg_value(iss, insn, buff, &insn->args[i], &insn->decoder_item->u.insn.args[i], &saved_args[i], &prev_arg, 1, is_long);
        }
        for (int i = 0; i < nb_args; i++)
        {
            buff = iss_trace_dump_arg_value(iss, insn, buff, &insn->args[i], &insn->decoder_item->u.insn.args[i], &saved_args[i], &prev_arg, 0, is_long);
        }

        buff += sprintf(buff, "\n");
    }
}

static void iss_trace_save_arg(Iss *iss, iss_insn_t *insn, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_insn_arg_t *saved_arg, bool save_out)
{
    if (arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
    {
        if (save_out && arg->type == ISS_DECODER_ARG_TYPE_OUT_REG ||
            !save_out && arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
        {
            if (arg->flags & ISS_DECODER_ARG_FLAG_REG64)
            {
                saved_arg->u.reg.value_64 = iss->regfile.get_reg64(insn_arg->u.reg.index);
            }
            else if (arg->flags & ISS_DECODER_ARG_FLAG_FREG)
            {
                #ifdef CONFIG_GVSOC_ISS_SNITCH
                // Special case: when SSR is enabled, the operand value is no longer from the register file.
                // The register file is bypassed and we go to ssr_fregs to fetch data.
                if (iss->ssr.ssr_enable & insn_arg->u.reg.index>=0 & insn_arg->u.reg.index<=2)
                {
                    saved_arg->u.reg.value_64 = iss->ssr.ssr_fregs[insn_arg->u.reg.index];
                }
                else
                {
                    saved_arg->u.reg.value_64 = iss->regfile.get_freg(insn_arg->u.reg.index);
                }
                #endif
                #ifndef CONFIG_GVSOC_ISS_SNITCH
                saved_arg->u.reg.value_64 = iss->regfile.get_freg(insn_arg->u.reg.index);
                #endif
            }
            else
            {
                saved_arg->u.reg.value = iss->regfile.get_reg_untimed(insn_arg->u.reg.index);
                #ifdef CONFIG_GVSOC_ISS_SNITCH
                if (iss->snitch & !iss->fp_ss & insn->is_fp_op)
                {
                    saved_arg->u.reg.value = insn->data_arga;
                }
                #endif
            }
        }
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
    {
        if (save_out)
            return;
        saved_arg->u.indirect_imm.reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_imm.reg_index);
        #ifdef CONFIG_GVSOC_ISS_SNITCH
        if (iss->snitch & !iss->fp_ss & insn->is_fp_op)
        {
            saved_arg->u.indirect_imm.reg_value = insn->data_arga;
        }
        #endif
    }
    // else if (arg->type == TRACE_TYPE_FLAG)
    //   {
    //     *savedValue = cpu->flag;
    //   }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
    {
        if (save_out)
            return;
        saved_arg->u.indirect_reg.base_reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_reg.base_reg_index);
        saved_arg->u.indirect_reg.offset_reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_reg.offset_reg_index);
        #ifdef CONFIG_GVSOC_ISS_SNITCH
        if (iss->snitch & !iss->fp_ss & insn->is_fp_op)
        {
            saved_arg->u.indirect_reg.base_reg_value = insn->data_arga;
        }
        #endif
    }
    // else
    //   {
    //     *savedValue = arg->val;
    //   }
}

static void iss_trace_save_args(Iss *iss, iss_insn_t *insn, iss_insn_arg_t saved_args[], bool save_out)
{
    for (int i = 0; i < insn->decoder_item->u.insn.nb_args; i++)
    {
        iss_decoder_arg_t *arg = &insn->decoder_item->u.insn.args[i];
        iss_trace_save_arg(iss, insn, &insn->args[i], arg, &saved_args[i], save_out);
    }
}

void iss_trace_dump_in(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_trace_save_args(iss, insn, iss->trace.saved_args, false);
}

void iss_trace_dump(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (!insn->is_macro_op || iss->top.traces.get_trace_engine()->get_format() == TRACE_FORMAT_LONG)
    {
        char buffer[1024];

        iss_trace_save_args(iss, insn, iss->trace.saved_args, true);

        iss_trace_dump_insn(iss, insn, pc, buffer, 1024, iss->trace.saved_args,
            iss->top.traces.get_trace_engine()->get_format() == TRACE_FORMAT_LONG, iss->trace.priv_mode, 0);

        iss->trace.insn_trace.msg(buffer);
    }
}

void iss_event_dump(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    char buffer[1024];

    iss_trace_dump_insn(iss, insn, pc, buffer, 1024, iss->trace.saved_args, false, iss->trace.priv_mode, 1);

    char *current = buffer;
    while (*current)
    {
        if (*current == ' ')
            *current = '_';

        current++;
    }

    iss->timing.insn_trace_event.event_string(buffer);
}

iss_reg_t iss_exec_insn_with_trace(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t next_insn;

    if (iss->timing.insn_trace_event.get_event_active())
    {
        iss_event_dump(iss, insn, pc);
    }

    if (iss->trace.insn_trace.get_active())
    {
        iss->trace.priv_mode = iss->core.mode_get();

        iss_trace_save_args(iss, insn, iss->trace.saved_args, false);

        next_insn = insn->saved_handler(iss, insn, pc);

        if (!iss->exec.is_stalled() && iss->trace.dump_trace_enabled)
            iss_trace_dump(iss, insn, pc);
    }
    else
    {
        next_insn = insn->saved_handler(iss, insn, pc);
    }

    return next_insn;
}

void iss_trace_init(Iss *iss)
{
    if (!pc_infos_is_init)
    {
        pc_infos_is_init = true;
        memset(pc_infos, 0, sizeof(pc_infos));
    }
}

void Trace::dump_debug_traces()
{
    const char *func, *inline_func, *file;
    int line;

    if (!iss_trace_pc_info(this->iss.exec.current_insn, &func, &inline_func, &file, &line))
    {
        this->iss.timing.func_trace_event.event_string(func);
        this->iss.timing.inline_trace_event.event_string(inline_func);
        this->iss.timing.file_trace_event.event_string(file);
        this->iss.timing.line_trace_event.event((uint8_t *)&line);
    }
}


void Trace::insn_trace_callback()
{
    // This is called when the state of the instruction trace has changed, we need
    // to flush the ISS instruction cache, as it keeps the state of the trace
    this->iss.insn_cache.flush();
}
