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

class Iss;

#include <stdint.h>
#define __STDC_FORMAT_MACROS // This is needed for some old gcc versions
#include <inttypes.h>
#include <vector>
#include <string>

#if defined(RISCY)
#define ISS_HAS_PERF_COUNTERS 1
#if defined(ISS_HAS_PERF_COUNTERS)
#ifdef PCER_VERSION_2
#include "archi/riscv/pcer_v2.h"
#else
#include "archi/riscv/pcer_v1.h"
#endif
#ifdef PRIV_1_10
#include "archi/riscv/priv_1_10.h"
#else
#include "archi/riscv/priv_1_9.h"
#endif
#endif
#else
#error Unknown core version
#endif

typedef uint64_t iss_reg64_t;

#define PRIxFULLREG64 "16.16" PRIx64

#if defined(ISS_WORD_64)

#define ISS_OPCODE_MAX_SIZE 8
#define ISS_REG_WIDTH 64
#define ISS_REG_WIDTH_LOG2 6

typedef uint64_t iss_reg_t;
typedef uint64_t iss_uim_t;
typedef int64_t iss_sim_t;
typedef uint64_t iss_addr_t;
typedef uint64_t iss_opcode_t;

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
typedef uint32_t iss_addr_t;
typedef uint32_t iss_opcode_t;

#define PRIxREG PRIx32
#define PRIxFULLREG "8.8" PRIx32
#define PRIdREG PRId32

#endif

class iss;
class Lsu;

#define ISS_NB_REGS 32
#define ISS_NB_FREGS 32
#define ISS_NB_TOTAL_REGS (ISS_NB_REGS + ISS_NB_FREGS)

#define ISS_PREFETCHER_SIZE (ISS_OPCODE_MAX_SIZE * 4)

#define ISS_MAX_DECODE_RANGES 8
#define ISS_MAX_DECODE_ARGS 5
#define ISS_MAX_IMMEDIATES 4
#define ISS_MAX_NB_OUT_REGS 3
#define ISS_MAX_NB_IN_REGS 3

#define ISS_INSN_BLOCK_SIZE_LOG2 8
#define ISS_INSN_BLOCK_SIZE (1 << ISS_INSN_BLOCK_SIZE_LOG2)
#define ISS_INSN_PC_BITS 1
#define ISS_INSN_BLOCK_ID_BITS 12
#define ISS_INSN_NB_BLOCKS (1 << ISS_INSN_BLOCK_ID_BITS)

#define ISS_EXCEPT_RESET 0
#define ISS_EXCEPT_ILLEGAL 1
#define ISS_EXCEPT_ECALL 2
#define ISS_EXCEPT_DEBUG 3

typedef struct iss_cpu_s iss_cpu_t;
typedef struct iss_insn_s iss_insn_t;
typedef struct iss_insn_block_s iss_insn_block_t;
typedef struct iss_insn_cache_s iss_insn_cache_t;
typedef struct iss_decoder_item_s iss_decoder_item_t;

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
    ISS_DECODER_ARG_FLAG_NONE = 0,
    ISS_DECODER_ARG_FLAG_POSTINC = 1,
    ISS_DECODER_ARG_FLAG_PREINC = 2,
    ISS_DECODER_ARG_FLAG_COMPRESSED = 4,
    ISS_DECODER_ARG_FLAG_FREG = 8,
    ISS_DECODER_ARG_FLAG_REG64 = 16,
    ISS_DECODER_ARG_FLAG_DUMP_NAME = 32,
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
        } indirect_imm;
        struct
        {
            int base_reg_index;
            iss_reg_t base_reg_value;
            int offset_reg_index;
            iss_reg_t offset_reg_value;
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

typedef struct iss_decoder_item_s
{

    bool is_insn;
    bool is_active;

    bool opcode_others;
    iss_opcode_t opcode;

    union
    {
        struct
        {
            iss_insn_t *(*handler)(Iss *, iss_insn_t *);
            iss_insn_t *(*fast_handler)(Iss *, iss_insn_t *);
            void (*decode)(Iss *, iss_insn_t *);
            char *label;
            int size;
            int nb_args;
            int latency;
            iss_decoder_arg_t args[ISS_MAX_DECODE_ARGS];
            int resource_id;
            int resource_latency;   // Time required to get the result when accessing the resource
            int resource_bandwidth; // Time required to accept the next access when accessing the resource
            int power_group;
        } insn;

        struct
        {
            int bit;
            int width;
            int nb_groups;
            iss_decoder_item_t **groups;
        } group;
    } u;

} iss_decoder_item_t;

// Structure describing an instance of a resource.
// This is used to account timing on shared resources.
// Each instance can accept accesses concurently.
typedef struct
{
    int64_t cycles; // Indicate the time where the next access to this resource is possible
} iss_resource_instance_t;

// Structure describing a resource.
typedef struct
{
    const char *name;                                 // Name of the resource
    int nb_instances;                                 // Number of instances of this resource. Each instance can accept accesses concurently
    std::vector<iss_resource_instance_t *> instances; // Instances of this resource
} iss_resource_t;

typedef struct iss_isa_s
{
    char *name;
    iss_decoder_item_t *tree;
} iss_isa_t;

typedef struct iss_isa_set_s
{
    int nb_isa;
    iss_isa_t *isa_set;
    int nb_resources;
    iss_resource_t *resources; // Resources associated to this ISA
} iss_isa_set_t;

typedef struct iss_isa_tag_s
{
    char *name;
    iss_decoder_item_t **insns;
} iss_isa_tag_t;

typedef struct iss_insn_s
{
    iss_addr_t addr;
    iss_reg_t opcode;
    bool fetched;
    iss_insn_t *(*fast_handler)(Iss *, iss_insn_t *);
    iss_insn_t *(*handler)(Iss *, iss_insn_t *);
    iss_insn_t *(*resource_handler)(Iss *, iss_insn_t *); // Handler called when an instruction with an associated resource is executed. The handler will take care of simulating the timing of the resource.
    iss_insn_t *(*hwloop_handler)(Iss *, iss_insn_t *);
    iss_insn_t *(*stall_handler)(Iss *, iss_insn_t *);
    iss_insn_t *(*stall_fast_handler)(Iss *, iss_insn_t *);
    int size;
    int nb_out_reg;
    int nb_in_reg;
    int out_regs[ISS_MAX_NB_OUT_REGS];
    int in_regs[ISS_MAX_NB_IN_REGS];
    iss_uim_t uim[ISS_MAX_IMMEDIATES];
    iss_sim_t sim[ISS_MAX_IMMEDIATES];
    iss_insn_arg_t args[ISS_MAX_DECODE_ARGS];
    iss_insn_t *next;
    iss_decoder_item_t *decoder_item;
    int resource_id;        // Identifier of the resource associated to this instruction
    int resource_latency;   // Time required to get the result when accessing the resource
    int resource_bandwidth; // Time required to accept the next access when accessing the resource

    int input_latency;
    int input_latency_reg;

    iss_insn_t *(*saved_handler)(Iss *, iss_insn_t *);
    iss_insn_t *branch;

    int in_spregs[6];

    int latency;

} iss_insn_t;

typedef struct iss_insn_block_s
{
    iss_addr_t pc;
    iss_insn_t insns[ISS_INSN_BLOCK_SIZE];
    iss_insn_block_t *next;
    bool is_init;
} iss_insn_block_t;

typedef struct iss_insn_cache_s
{
    iss_insn_block_t *blocks[ISS_INSN_NB_BLOCKS];
} iss_insn_cache_t;

typedef struct iss_regfile_s
{
    iss_reg_t regs[ISS_NB_REGS + ISS_NB_FREGS];
} iss_regfile_t;

typedef struct
{
    union
    {
        struct
        {
            union
            {
                struct
                {
                    unsigned int NX : 1;
                    unsigned int UF : 1;
                    unsigned int OF : 1;
                    unsigned int DZ : 1;
                    unsigned int NV : 1;
                };
                unsigned int raw : 5;
            } fflags;
            unsigned int frm : 3;
        };
        iss_reg_t raw;
    };
} iss_fcsr_t;

typedef struct iss_cpu_state_s
{
    iss_insn_t *hwloop_start_insn[2];
    iss_insn_t *hwloop_end_insn[2];

    iss_addr_t bootaddr;

    void (*stall_callback)(Lsu *lsu);
    int stall_reg;
    int stall_size;
    bool do_fetch;

    iss_insn_arg_t saved_args[ISS_MAX_DECODE_ARGS];

    iss_reg_t vf0;
    iss_reg_t vf1;

    iss_insn_t *elw_insn;
    bool cache_sync;
    // This is used by HW loop to know that we interrupted and replayed
    // a ELW instructin so that it is not accounted twice in the loop.
    int elw_interrupted;
    iss_insn_t *hwloop_next_insn;

    iss_fcsr_t fcsr;

    bool debug_mode;

} iss_cpu_state_t;

typedef struct iss_config_s
{
    iss_reg_t mhartid;
    iss_reg_t misa;
    const char *isa;
    iss_addr_t debug_handler;
} iss_config_t;

typedef struct iss_csr_s
{
    iss_reg_t status;
    iss_reg_t epc;
    iss_reg_t depc;
    iss_reg_t dcsr;
    iss_reg_t mtvec;
    iss_reg_t mcause;
#if defined(ISS_HAS_PERF_COUNTERS)
    iss_reg_t pccr[32];
    iss_reg_t pcer;
    iss_reg_t pcmr;
#endif
    iss_reg_t stack_conf;
    iss_reg_t stack_start;
    iss_reg_t stack_end;
    iss_reg_t scratch0;
    iss_reg_t scratch1;
    iss_reg_t mscratch;
} iss_csr_t;

#define PULPV2_HWLOOP_NB_REGS 7

typedef struct iss_pulpv2_s
{
    bool hwloop;
    iss_reg_t hwloop_regs[PULPV2_HWLOOP_NB_REGS];
} iss_pulpv2_t;

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

#define COREV_HWLOOP_NB_REGS 7

typedef struct iss_corev_s
{
    bool hwloop;
    iss_reg_t hwloop_regs[COREV_HWLOOP_NB_REGS];
} iss_corev_t;

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


class Timing
{
public:
    inline void stall_fetch_account(int count);
    inline void stall_taken_branch_account();
    inline void stall_insn_account(int cycles);
    inline void stall_insn_dependency_account(int latency);
    inline void stall_load_dependency_account(int latency);
    inline void stall_jump_account();
    inline void stall_misaligned_account();
    inline void stall_load_account(int cycles);
    inline void insn_account();

    inline void event_load_account(int incr);
    inline void event_rvc_account(int incr);
    inline void event_store_account(int incr);
    inline void event_branch_account(int incr);
    inline void event_taken_branch_account(int incr);
    inline void event_jump_account(int incr);
    inline void event_misaligned_account(int incr);
    inline void event_insn_contention_account(int incr);

    inline int stall_cycles_get();
    inline void stall_cycles_dec();

    inline void reset(bool active);

private:
    inline void event_account(unsigned int event, int incr);

    int stall_cycles;
};



class Decode
{
public:
    Decode(Iss &iss);
    iss_insn_t *decode_pc(iss_insn_t *pc);

private:
    int decode_opcode(iss_insn_t *insn, iss_opcode_t opcode);
    int decode_item(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_opcode_group(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    int decode_insn(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_item_t *item);
    uint64_t decode_ranges(iss_opcode_t opcode, iss_decoder_range_set_t *range_set, bool is_signed);
    int decode_info(iss_insn_t *insn, iss_opcode_t opcode, iss_decoder_arg_info_t *info, bool is_signed);

    // TODO to be removed
    Iss &iss;
    vp::trace trace;
};



#endif
