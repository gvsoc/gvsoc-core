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

#ifndef __CPU_IssYPES_HPP
#define __CPU_IssYPES_HPP


#include <vp/vp.hpp>

class Iss;

#include <stdint.h>
#define __STDC_FORMAT_MACROS // This is needed for some old gcc versions
#include <inttypes.h>
#include <vector>
#include <string>

#ifndef ISA_NB_TAGS
#define ISA_NB_TAGS 0
#endif

#if defined(RISCY)
#define ISS_HAS_PERF_COUNTERS 1
#if defined(ISS_HAS_PERF_COUNTERS)
#include "archi/riscv/pcer_v2.h"
#include "archi/riscv/priv_1_10.h"
#endif
#else
#error Unknown core version
#endif

typedef uint64_t iss_reg64_t;

#define PRIxFULLREG64 "16.16" PRIx64

typedef uint64_t iss_freg_t;

#if defined(ISS_WORD_64)

#define ISS_OPCODE_MAX_SIZE 4
#define ISS_REG_WIDTH 64
#define ISS_REG_WIDTH_LOG2 6

typedef uint64_t iss_reg_t;
typedef uint64_t iss_uim_t;
typedef int64_t iss_sim_t;
typedef __int128 iss_lsim_t;
typedef __int128 iss_luim_t;
typedef uint64_t iss_addr_t;
typedef uint32_t iss_opcode_t;

#define PRIxREG PRIx64
#define PRIxFULLREG "16.16" PRIx64
#define PRIdREG PRIx64

#else

#define ISS_OPCODE_MAX_SIZE 4
#define ISS_REG_WIDTH 32
#define ISS_REG_WIDTH_LOG2 5

typedef uint32_t iss_reg_t;
typedef uint32_t iss_uim_t;
typedef int32_t iss_sim_t;
typedef int64_t iss_lsim_t;
typedef uint64_t iss_luim_t;
typedef uint32_t iss_addr_t;
typedef uint32_t iss_opcode_t;

#define PRIxREG PRIx32
#define PRIxFULLREG "8.8" PRIx32
#define PRIdREG PRId32

#endif

class iss;
class Lsu;

#if CONFIG_GVSOC_ISS_FP_WIDTH == 64
#define CONFIG_GVSOC_ISS_FP_EXP   11
#define CONFIG_GVSOC_ISS_FP_MANT  52
#else
#define CONFIG_GVSOC_ISS_FP_EXP   8
#define CONFIG_GVSOC_ISS_FP_MANT  23
#endif

#define ISS_NB_REGS 32
#define ISS_NB_FREGS 32

#ifndef CONFIG_GVSOC_ISS_PREFETCHER_SIZE
#define ISS_PREFETCHER_SIZE (ISS_OPCODE_MAX_SIZE * 4)
#else
#define ISS_PREFETCHER_SIZE CONFIG_GVSOC_ISS_PREFETCHER_SIZE
#endif

#define ISS_MAX_DECODE_RANGES 8
#define ISS_MAX_DECODE_ARGS 8
#define ISS_MAX_IMMEDIATES 4
#define ISS_MAX_NB_OUT_REGS 3
#define ISS_MAX_NB_IN_REGS 3
#define ISS_MAX_DATA 1

#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
#define ISS_EXCEPT_INSN_MISALIGNED  0
#define ISS_EXCEPT_INSN_FAULT       1
#define ISS_EXCEPT_ILLEGAL          2
#define ISS_EXCEPT_BREAKPOINT       3
#define ISS_EXCEPT_LOAD_MISALIGNED  4
#define ISS_EXCEPT_LOAD_FAULT       5
#define ISS_EXCEPT_STORE_MISALIGNED 6
#define ISS_EXCEPT_STORE_FAULT      7
#define ISS_EXCEPT_ENV_CALL_U_MODE  8
#define ISS_EXCEPT_ENV_CALL_S_MODE  9
#define ISS_EXCEPT_ENV_CALL_M_MODE  11
#define ISS_EXCEPT_INSN_PAGE_FAULT  12
#define ISS_EXCEPT_LOAD_PAGE_FAULT  13
#define ISS_EXCEPT_STORE_PAGE_FAULT 15

// TODO for compatibility, should be cleaned-up
#define ISS_EXCEPT_DEBUG   0
#else
#define ISS_EXCEPT_RESET   0
#define ISS_EXCEPT_ILLEGAL 1
#define ISS_EXCEPT_ENV_CALL_U_MODE   2
#define ISS_EXCEPT_DEBUG   3

#define ISS_EXCEPT_INSN_PAGE_FAULT  0
#define ISS_EXCEPT_LOAD_PAGE_FAULT  0
#define ISS_EXCEPT_STORE_PAGE_FAULT 0
#define ISS_EXCEPT_BREAKPOINT       0
#define ISS_EXCEPT_INSN_FAULT       0
#define ISS_EXCEPT_LOAD_FAULT       0
#define ISS_EXCEPT_STORE_FAULT      0
#endif

typedef struct iss_cpu_s iss_cpu_t;
typedef struct iss_insn_s iss_insn_t;
typedef struct iss_insn_cache_s iss_insn_cache_t;
typedef struct iss_decoder_item_s iss_decoder_item_t;

// Structure describing an instance of a resource.
// This is used to account timing on shared resources.
// Each instance can accept accesses concurently.
typedef struct
{
    int64_t cycles; // Indicate the time where the next access to this resource is possible
} iss_resource_instance_t;

typedef enum
{
    ISS_DECODER_ARG_TYPE_NONE,
    ISS_DECODER_ARG_TYPE_OUT_REG,
    ISS_DECODER_ARG_TYPE_IN_REG,
    ISS_DECODER_ARG_TYPE_UIMM,
    ISS_DECODER_ARG_TYPE_SIMM,
    ISS_DECODER_ARG_TYPE_INDIRECT_IMM,
    ISS_DECODER_ARG_TYPE_INDIRECT_REG,
    ISS_DECODER_ARG_TYPE_FLAG,
} iss_decoder_arg_type_e;

typedef enum
{
    ISS_DECODER_ARG_FLAG_NONE       = (1 << 0),
    ISS_DECODER_ARG_FLAG_POSTINC    = (1 << 1),
    ISS_DECODER_ARG_FLAG_PREINC     = (1 << 2),
    ISS_DECODER_ARG_FLAG_COMPRESSED = (1 << 3),
    ISS_DECODER_ARG_FLAG_FREG       = (1 << 4),
    ISS_DECODER_ARG_FLAG_REG64      = (1 << 5),
    ISS_DECODER_ARG_FLAG_DUMP_NAME  = (1 << 6),
    ISS_DECODER_ARG_FLAG_VREG       = (1 << 7),
    ISS_DECODER_ARG_FLAG_ELEM_32    = (1 << 8),
    ISS_DECODER_ARG_FLAG_ELEM_16    = (1 << 9),
    ISS_DECODER_ARG_FLAG_ELEM_16A   = (1 << 10),
    ISS_DECODER_ARG_FLAG_ELEM_8     = (1 << 11),
    ISS_DECODER_ARG_FLAG_ELEM_8A    = (1 << 12),
    ISS_DECODER_ARG_FLAG_ELEM_64    = (1 << 13),
    ISS_DECODER_ARG_FLAG_VEC        = (1 << 14),
    ISS_DECODER_ARG_FLAG_ELEM_SEW   = (1 << 15),
} iss_decoder_arg_flag_e;

typedef struct iss_insn_arg_s
{
    iss_decoder_arg_type_e type;
    iss_decoder_arg_flag_e flags;
    const char *name;

    union
    {
        struct
        {
            int index;
            iss_reg_t value;
            iss_reg64_t value_64;
            iss_reg_t memcheck_value;
            iss_reg64_t memcheck_value_64;
        } reg;
        struct
        {
            iss_sim_t value;
        } sim;
        struct
        {
            iss_uim_t value;
        } uim;
        struct
        {
            int reg_index;
            iss_sim_t imm;
            iss_reg_t reg_value;
            iss_reg_t memcheck_reg_value;
        } indirect_imm;
        struct
        {
            int base_reg_index;
            iss_reg_t base_reg_value;
            iss_reg_t memcheck_base_reg_value;
            int offset_reg_index;
            iss_reg_t offset_reg_value;
            iss_reg_t memcheck_offset_reg_value;
        } indirect_reg;
    } u;
} iss_insn_arg_t;

typedef struct iss_decoder_range_s
{
    int bit;
    int width;
    int shift;
} iss_decoder_range_t;

typedef struct iss_decoder_range_set_s
{
    int nb_ranges;
    iss_decoder_range_t ranges[ISS_MAX_DECODE_RANGES];
} iss_decoder_range_set_t;

typedef enum
{
    ISS_DECODER_VALUE_TYPE_UIM = 0,
    ISS_DECODER_VALUE_TYPE_SIM = 1,
    ISS_DECODER_VALUE_TYPE_RANGE = 2,
} iss_decoder_arg_info_type_e;

typedef struct iss_decoder_arg_value_s
{
    iss_decoder_arg_info_type_e type;
    union
    {
        iss_uim_t uim;
        iss_sim_t sim;
        iss_decoder_range_set_t range_set;
    } u;
} iss_decoder_arg_info_t;

typedef struct iss_decoder_arg_s
{
    iss_decoder_arg_type_e type;
    iss_decoder_arg_flag_e flags;
    union
    {
        struct
        {
            bool is_signed;
            int id;
            iss_decoder_arg_info_t info;
        } uimm;
        struct
        {
            bool is_signed;
            int id;
            iss_decoder_arg_info_t info;
        } simm;
        struct
        {
            int id;
            iss_decoder_arg_flag_e flags;
            bool dump_name;
            int latency;
            iss_decoder_arg_info_t info;
        } reg;
        struct
        {
            struct
            {
                int id;
                iss_decoder_arg_flag_e flags;
                bool dump_name;
                int latency;
                iss_decoder_arg_info_t info;
            } reg;
            struct
            {
                bool is_signed;
                int id;
                iss_decoder_arg_info_t info;
            } imm;
        } indirect_imm;
        struct
        {
            struct
            {
                int id;
                iss_decoder_arg_flag_e flags;
                bool dump_name;
                int latency;
                iss_decoder_arg_info_t info;
            } base_reg;
            struct
            {
                int id;
                iss_decoder_arg_flag_e flags;
                bool dump_name;
                int latency;
                iss_decoder_arg_info_t info;
            } offset_reg;
        } indirect_reg;
    } u;
} iss_decoder_arg_t;

typedef struct iss_decoder_insn_s
{
    iss_reg_t (*handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*fast_handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*stub_handler)(Iss *, iss_insn_t *, iss_reg_t);
    void (*decode)(Iss *, iss_insn_t *, iss_reg_t pc);
    char *label;
    int size;
    int nb_args;
    int latency;
    iss_decoder_arg_t args[ISS_MAX_DECODE_ARGS];
    int resource_id;
    int resource_latency;   // Time required to get the result when accessing the resource
    int resource_bandwidth; // Time required to accept the next access when accessing the resource
    int block_id;
    void *block_handler;
    int power_group;
    int is_macro_op;
    uint64_t flags;
    bool tags[ISA_NB_TAGS];
    uint8_t args_order[ISS_MAX_DECODE_ARGS];
} iss_decoder_insn_t;

typedef struct iss_decoder_item_s
{

    bool is_insn;
    bool is_active;

    bool opcode_others;
    iss_opcode_t opcode;

    union
    {
        iss_decoder_insn_t insn;

        struct
        {
            int bit;
            int width;
            int nb_groups;
            iss_decoder_item_t **groups;
        } group;
    } u;

} iss_decoder_item_t;

typedef struct iss_insn_s
{
    iss_reg_t (*fast_handler)(Iss *, iss_insn_t *, iss_reg_t);
    unsigned char out_regs[ISS_MAX_NB_OUT_REGS];
    bool out_regs_fp[ISS_MAX_NB_OUT_REGS];
    unsigned char in_regs[ISS_MAX_NB_IN_REGS];
    bool in_regs_fp[ISS_MAX_NB_IN_REGS];
    void *out_regs_ref[ISS_MAX_NB_OUT_REGS];
    void *in_regs_ref[ISS_MAX_NB_IN_REGS];
    iss_uim_t uim[ISS_MAX_IMMEDIATES];
    iss_sim_t sim[ISS_MAX_IMMEDIATES];
    iss_addr_t addr;
    iss_reg_t opcode;
    iss_reg_t (*handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*resource_handler)(Iss *, iss_insn_t *, iss_reg_t); // Handler called when an instruction with an associated resource is executed. The handler will take care of simulating the timing of the resource.
#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
    iss_reg_t (*hwloop_handler)(Iss *, iss_insn_t *, iss_reg_t);
#endif
    iss_reg_t (*stub_handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*stall_handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*stall_fast_handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*breakpoint_saved_handler)(Iss *, iss_insn_t *, iss_reg_t);
    iss_reg_t (*breakpoint_saved_fast_handler)(Iss *, iss_insn_t *, iss_reg_t);
    int size;
    int nb_out_reg;
    int nb_in_reg;
    iss_insn_arg_t args[ISS_MAX_DECODE_ARGS];
    iss_decoder_item_t *decoder_item;
    int resource_id;        // Identifier of the resource associated to this instruction
    int resource_latency;   // Time required to get the result when accessing the resource
    int resource_bandwidth; // Time required to accept the next access when accessing the resource

    iss_reg_t (*saved_handler)(Iss *, iss_insn_t *, iss_reg_t);

    int in_spregs[6];

    int latency;
    std::vector<iss_reg_t>  breakpoints;

    iss_insn_t *expand_table;
    bool is_macro_op;
    uint64_t flags;

    void *data[ISS_MAX_DATA];

#ifdef CONFIG_GVSOC_ISS_SNITCH
    bool is_outer;
    iss_reg_t max_rpt;

    iss_reg_t* reg_addr;
    iss_freg_t* freg_addr;
    int64_t* scoreboard_reg_timestamp_addr;
    // unsigned int* fflags_addr;

    iss_reg_t data_arga;
    iss_reg_t data_argb;
    iss_reg_t data_argc;

    // TODO this have been put here since fp ss is taking handlers from main core while it
    // should not
    unsigned int fmode;

#endif

    iss_decoder_insn_t *desc;

} iss_insn_t;




#define HWLOOP_NB_REGS 7

typedef struct iss_pulp_nn_s
{
    int qnt_step;
    iss_reg_t qnt_regs[4];
    iss_addr_t addr_reg; // need to be extended with address reg
    iss_reg_t qnt_reg_out;
    iss_reg_t spr_ml[6];
    iss_insn_t *ml_insn;
} iss_pulp_nn_t;

typedef struct iss_rnnext_s
{
    iss_insn_t *sdot_insn;
    iss_reg_t sdot_prefetch_0;
    iss_reg_t sdot_prefetch_1;
} iss_rnnext_t;

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include "vp/gdbserver/gdbserver_engine.hpp"

#define HALT_CAUSE_EBREAK 1
#define HALT_CAUSE_TRIGGER 2
#define HALT_CAUSE_HALT 3
#define HALT_CAUSE_STEP 4
#define HALT_CAUSE_RESET_HALT 5

typedef struct
{
    std::string name;
    std::string help;
} Iss_pcer_info_t;


#define  __ISS_CORE_INC(x) #x
#define  _ISS_CORE_INC(x, y) __ISS_CORE_INC(cpu/iss/include/cores/x/y)
#define  ISS_CORE_INC(x) _ISS_CORE_INC(CONFIG_ISS_CORE, x)

class PendingInsn
{
public:
    iss_insn_t *insn;
    uint64_t timestamp;
    iss_reg_t pc;
    uint64_t reg;
    uint64_t reg_2;
    bool done;
    int id;
};


#endif
