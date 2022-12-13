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
#define __STDC_FORMAT_MACROS    // This is needed for some old gcc versions
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

#define PRIxFULLREG64  "16.16" PRIx64

#if defined(ISS_WORD_64)

#define ISS_OPCODE_MAX_SIZE 8
#define ISS_REG_WIDTH 64
#define ISS_REG_WIDTH_LOG2 6

typedef uint64_t iss_reg_t;
typedef uint64_t iss_uim_t;
typedef int64_t iss_sim_t;
typedef uint64_t iss_addr_t;
typedef uint64_t iss_opcode_t;

#define PRIxREG  PRIx64
#define PRIxFULLREG  "16.16" PRIx64
#define PRIdREG  PRIx64

#else

#define ISS_OPCODE_MAX_SIZE 4
#define ISS_REG_WIDTH 32
#define ISS_REG_WIDTH_LOG2 5

typedef uint32_t iss_reg_t;
typedef uint32_t iss_uim_t;
typedef int32_t iss_sim_t;
typedef uint32_t iss_addr_t;
typedef uint32_t iss_opcode_t;

#define PRIxREG  PRIx32
#define PRIxFULLREG  "8.8" PRIx32
#define PRIdREG  PRId32

#endif

class iss;

#define ISS_NB_REGS  32
#define ISS_NB_FREGS 32
#define ISS_NB_TOTAL_REGS (ISS_NB_REGS + ISS_NB_FREGS)

#define ISS_PREFETCHER_SIZE (ISS_OPCODE_MAX_SIZE*4)

#define ISS_MAX_DECODE_RANGES 8
#define ISS_MAX_DECODE_ARGS 5
#define ISS_MAX_IMMEDIATES 4
#define ISS_MAX_NB_OUT_REGS 3
#define ISS_MAX_NB_IN_REGS 3

#define ISS_INSN_BLOCK_SIZE_LOG2 8
#define ISS_INSN_BLOCK_SIZE (1<<ISS_INSN_BLOCK_SIZE_LOG2)
#define ISS_INSN_PC_BITS 1
#define ISS_INSN_BLOCK_ID_BITS 12
#define ISS_INSN_NB_BLOCKS (1<<ISS_INSN_BLOCK_ID_BITS)

#define ISS_EXCEPT_RESET    0
#define ISS_EXCEPT_ILLEGAL  1
#define ISS_EXCEPT_ECALL    2
#define ISS_EXCEPT_DEBUG    3

typedef struct iss_cpu_s iss_cpu_t;
typedef struct iss_insn_s iss_insn_t;
typedef struct iss_insn_block_s iss_insn_block_t;
typedef struct iss_insn_cache_s iss_insn_cache_t;
typedef struct iss_decoder_item_s iss_decoder_item_t;

typedef enum {
  ISS_DECODER_ARG_TYPE_NONE,
  ISS_DECODER_ARG_TYPE_OUT_REG,
  ISS_DECODER_ARG_TYPE_IN_REG,
  ISS_DECODER_ARG_TYPE_UIMM,
  ISS_DECODER_ARG_TYPE_SIMM,
  ISS_DECODER_ARG_TYPE_INDIRECT_IMM,
  ISS_DECODER_ARG_TYPE_INDIRECT_REG,
  ISS_DECODER_ARG_TYPE_FLAG,
} iss_decoder_arg_type_e;

typedef enum {
  ISS_DECODER_ARG_FLAG_NONE = 0,
  ISS_DECODER_ARG_FLAG_POSTINC = 1,
  ISS_DECODER_ARG_FLAG_PREINC = 2,
  ISS_DECODER_ARG_FLAG_COMPRESSED = 4,
  ISS_DECODER_ARG_FLAG_FREG = 8,
  ISS_DECODER_ARG_FLAG_REG64 = 16,
  ISS_DECODER_ARG_FLAG_DUMP_NAME = 32,
} iss_decoder_arg_flag_e;

typedef struct iss_insn_arg_s {
  iss_decoder_arg_type_e type;
  iss_decoder_arg_flag_e flags;
  const char *name;

  union {
    struct {
      int index;
      iss_reg_t value;
      iss_reg64_t value_64;
    } reg;
    struct {
      iss_sim_t value;
    } sim;
    struct {
      iss_uim_t value;
    } uim;
    struct {
      int reg_index;
      iss_sim_t imm;
      iss_reg_t reg_value;
    } indirect_imm;
    struct {
      int base_reg_index;
      iss_reg_t base_reg_value;
      int offset_reg_index;
      iss_reg_t offset_reg_value;
    } indirect_reg;
  } u;
} iss_insn_arg_t;

typedef struct iss_decoder_range_s {
  int bit;
  int width;
  int shift;
} iss_decoder_range_t;

typedef struct iss_decoder_range_set_s {
  int nb_ranges;
  iss_decoder_range_t ranges[ISS_MAX_DECODE_RANGES];
} iss_decoder_range_set_t;

typedef enum {
  ISS_DECODER_VALUE_TYPE_UIM = 0,
  ISS_DECODER_VALUE_TYPE_SIM = 1,
  ISS_DECODER_VALUE_TYPE_RANGE = 2,
} iss_decoder_arg_info_type_e;

typedef struct iss_decoder_arg_value_s {
  iss_decoder_arg_info_type_e type;
  union {
    iss_uim_t uim;
    iss_sim_t sim;
    iss_decoder_range_set_t range_set;
  } u;
} iss_decoder_arg_info_t;

typedef struct iss_decoder_arg_s {
  iss_decoder_arg_type_e type;
  iss_decoder_arg_flag_e flags;
  union {
    struct {
      bool is_signed;
      int id;
      iss_decoder_arg_info_t info;
    } uimm;
    struct {
      bool is_signed;
      int id;
      iss_decoder_arg_info_t info;
    } simm;
    struct {
      int id;
      iss_decoder_arg_flag_e flags;
      bool dump_name;
      int latency;
      iss_decoder_arg_info_t info;
    } reg;
    struct {
      struct {
        int id;
        iss_decoder_arg_flag_e flags;
        bool dump_name;
      int latency;
        iss_decoder_arg_info_t info;
      } reg;
      struct {
        bool is_signed;
        int id;
        iss_decoder_arg_info_t info;
      } imm;
    } indirect_imm;
    struct {
      struct {
        int id;
        iss_decoder_arg_flag_e flags;
        bool dump_name;
        int latency;
        iss_decoder_arg_info_t info;
      } base_reg;
      struct {
        int id;
        iss_decoder_arg_flag_e flags;
        bool dump_name;
        int latency;
        iss_decoder_arg_info_t info;
      } offset_reg;
    } indirect_reg;
  } u;
} iss_decoder_arg_t;

typedef struct iss_decoder_item_s {

  bool is_insn;
  bool is_active;

  bool opcode_others;
  iss_opcode_t opcode;

  union {
    struct {
      iss_insn_t *(*handler)(Iss *, iss_insn_t*);
      iss_insn_t *(*fast_handler)(Iss *, iss_insn_t*);
      void (*decode)(Iss *, iss_insn_t*);
      char *label;
      int size;
      int nb_args;
      int latency;
      iss_decoder_arg_t args[ISS_MAX_DECODE_ARGS];
      int resource_id;
      int resource_latency;          // Time required to get the result when accessing the resource
      int resource_bandwidth;        // Time required to accept the next access when accessing the resource
      int power_group;
    } insn;

    struct {
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
  int64_t cycles;    // Indicate the time where the next access to this resource is possible
} iss_resource_instance_t;


// Structure describing a resource.
typedef struct
{
  const char *name;     // Name of the resource
  int nb_instances;     // Number of instances of this resource. Each instance can accept accesses concurently
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
  iss_resource_t *resources;   // Resources associated to this ISA
} iss_isa_set_t;

typedef struct iss_isa_tag_s
{
  char *name;
  iss_decoder_item_t **insns;
} iss_isa_tag_t;

typedef struct {
  uint8_t data[ISS_PREFETCHER_SIZE];
  iss_addr_t addr;
} iss_prefetcher_t;

typedef struct iss_insn_s {
  iss_addr_t addr;
  iss_reg_t opcode;
  bool fetched;
  iss_insn_t *(*fast_handler)(Iss *, iss_insn_t*);
  iss_insn_t *(*handler)(Iss *, iss_insn_t*);
  iss_insn_t *(*resource_handler)(Iss *, iss_insn_t*);        // Handler called when an instruction with an associated resource is executed. The handler will take care of simulating the timing of the resource.
  iss_insn_t *(*hwloop_handler)(Iss *, iss_insn_t*);
  iss_insn_t *(*stall_handler)(Iss *, iss_insn_t*);
  iss_insn_t *(*stall_fast_handler)(Iss *, iss_insn_t*);
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
  int resource_id;   // Identifier of the resource associated to this instruction
  int resource_latency;          // Time required to get the result when accessing the resource
  int resource_bandwidth;        // Time required to accept the next access when accessing the resource

  int input_latency;
  int input_latency_reg;

  iss_insn_t *(*saved_handler)(Iss *, iss_insn_t*);
  iss_insn_t *branch;

  int in_spregs[6];

  int latency;

} iss_insn_t;

typedef struct iss_insn_block_s {
  iss_addr_t pc;
  iss_insn_t insns[ISS_INSN_BLOCK_SIZE];
  iss_insn_block_t *next;
  bool is_init;
} iss_insn_block_t;

typedef struct iss_insn_cache_s {
  iss_insn_block_t *blocks[ISS_INSN_NB_BLOCKS];
} iss_insn_cache_t;

typedef struct iss_regfile_s {
  iss_reg_t regs[ISS_NB_REGS + ISS_NB_FREGS];
} iss_regfile_t;

typedef struct
{
  union {
    struct {
      union {
        struct {
          unsigned int NX:1;
          unsigned int UF:1;
          unsigned int OF:1;
          unsigned int DZ:1;
          unsigned int NV:1;
        };
        unsigned int raw:5;
      } fflags;
      unsigned int frm:3;
    };
    iss_reg_t raw;
  };
} iss_fcsr_t;

typedef struct iss_cpu_state_s {
  iss_insn_t *hwloop_start_insn[2];
  iss_insn_t *hwloop_end_insn[2];

  iss_addr_t bootaddr;

  int insn_cycles;

  void (*stall_callback)(Iss *iss);
  void (*fetch_stall_callback)(Iss *iss);
  iss_opcode_t fetch_stall_opcode;
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

typedef struct iss_config_s {
  iss_reg_t mhartid;
  iss_reg_t misa;
  const char *isa;
  iss_addr_t debug_handler;
} iss_config_t;

typedef struct iss_irq_s {
  iss_insn_t *vectors[35];
  int irq_enable;
  int saved_irq_enable;
  int debug_saved_irq_enable;
  int req_irq;
  bool req_debug;
  uint32_t vector_base;
  iss_insn_t *debug_handler;
} iss_irq_t;

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
  iss_addr_t addr_reg;  // need to be extended with address reg
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


typedef struct iss_cpu_s {
  iss_prefetcher_t decode_prefetcher;
  iss_prefetcher_t prefetcher;
  iss_insn_cache_t insn_cache;
  iss_insn_t *current_insn;
  iss_insn_t *prev_insn;
  iss_insn_t *stall_insn;
  iss_insn_t *prefetch_insn;
  iss_regfile_t regfile;
  iss_cpu_state_t state;
  iss_config_t config;
  iss_irq_t irq;
  iss_csr_t csr;
  iss_pulpv2_t pulpv2;
  iss_pulp_nn_t pulp_nn;
  iss_rnnext_t rnnext;
  iss_corev_t corev;
  std::vector<iss_resource_instance_t *>resources;     // When accesses to the resources are scheduled statically, this gives the instance allocated to this core for each resource
} iss_cpu_t;
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include "vp/gdbserver/gdbserver_engine.hpp"



#define HALT_CAUSE_EBREAK      1
#define HALT_CAUSE_TRIGGER     2
#define HALT_CAUSE_HALT        3
#define HALT_CAUSE_STEP        4
#define HALT_CAUSE_RESET_HALT  5

typedef struct
{
    std::string name;
    std::string help;
} Iss_pcer_info_t;


class Iss : public vp::component, vp::Gdbserver_core
{

public:

  Iss(js::config *config);

  int build();
  void start();
  void pre_reset();
  void reset(bool active);

  virtual void target_open();

  static void data_grant(void *_this, vp::io_req *req);
  static void data_response(void *_this, vp::io_req *req);

  static void fetch_grant(void *_this, vp::io_req *req);
  static void fetch_response(void *_this, vp::io_req *req);

  static void refetch_handler(void *__this, vp::clock_event *event);
  static void exec_instr(void *__this, vp::clock_event *event);
  static void exec_first_instr(void *__this, vp::clock_event *event);
  void exec_first_instr(vp::clock_event *event);
  static void exec_instr_check_all(void *__this, vp::clock_event *event);
  static inline void exec_misaligned(void *__this, vp::clock_event *event);

  static void irq_req_sync(void *__this, int irq);
  void elw_irq_unstall();
  void debug_req();

  inline int data_req(iss_addr_t addr, uint8_t *data, int size, bool is_write);
  inline int data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write);
  int data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write);

  bool user_access(iss_addr_t addr, uint8_t *data, iss_addr_t size, bool is_write);
  std::string read_user_string(iss_addr_t addr, int len=-1);

  static vp::io_req_status_e dbg_unit_req(void *__this, vp::io_req *req);
  inline void iss_exec_insn_check_debug_step();

  void irq_check();
  void wait_for_interrupt();
  
  void set_halt_mode(bool halted, int cause);

  void handle_ebreak();
  void handle_riscv_ebreak();

  void dump_debug_traces();

  inline void trigger_check_all() { this->instr_event->meth_set(&Iss::exec_instr_check_all); }

  void insn_trace_callback();

  int gdbserver_get_id();
  std::string gdbserver_get_name();
  int gdbserver_reg_set(int reg, uint8_t *value);
  int gdbserver_reg_get(int reg, uint8_t *value);
  int gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value);
  int gdbserver_stop();
  int gdbserver_cont();
  int gdbserver_stepi();
  int gdbserver_state();

  void declare_pcer(int index, std::string name, std::string help);

  inline void stalled_inc();
  inline void stalled_dec();

  /*
   * Performance events
   */

  inline void perf_event_incr(unsigned int event, int incr);
  inline int perf_event_is_active(unsigned int event);

  vp::io_master data;
  vp::io_master fetch;
  vp::io_slave  dbg_unit;

  vp::wire_slave<int>      irq_req_itf;
  vp::wire_master<int>     irq_ack_itf;
  vp::wire_master<bool>     busy_itf;

  vp::wire_master<bool>    flush_cache_req_itf;
  vp::wire_slave<bool>     flush_cache_ack_itf;

  vp::wire_master<uint32_t> ext_counter[32];

  vp::io_req     io_req;
  vp::io_req     fetch_req;

  iss_cpu_t cpu;

  vp::trace     trace;
  vp::trace     gdbserver_trace;
  vp::trace     decode_trace;
  vp::trace     insn_trace;
  vp::trace     csr_trace;
  vp::trace     perf_counter_trace;

  vp::reg_32    bootaddr_reg;
  vp::reg_1     fetch_enable_reg;
  vp::reg_1     is_active_reg;
  vp::reg_8     stalled;
  vp::reg_1     wfi;
  vp::reg_1     misaligned_access;
  vp::reg_1     halted;
  vp::reg_1     step_mode;
  vp::reg_1     do_step;

  vp::reg_1 elw_stalled;

  std::vector<vp::power::power_source> insn_groups_power;
  vp::power::power_source background_power;

  vp::trace     state_event;
  vp::reg_1     busy;
  vp::trace     pc_trace_event;
  vp::trace     active_pc_trace_event;
  vp::trace     func_trace_event;
  vp::trace     inline_trace_event;
  vp::trace     line_trace_event;
  vp::trace     file_trace_event;
  vp::trace     binaries_trace_event;
  vp::trace     pcer_trace_event[32];
  vp::trace     insn_trace_event;

  Iss_pcer_info_t pcer_info[32];
  int64_t cycle_count_start;
  int64_t cycle_count;

  bool dump_trace_enabled;

  static void ipc_stat_handler(void *__this, vp::clock_event *event);
  void gen_ipc_stat(bool pulse=false);
  void trigger_ipc_stat();
  void stop_ipc_stat();
  int ipc_stat_nb_insn;
  vp::trace     ipc_stat_event;
  vp::clock_event *ipc_clock_event;
  int ipc_stat_delay;
  
#ifdef USE_TRDB
  trdb_ctx *trdb;
  struct list_head trdb_packet_list;
  uint8_t trdb_pending_word[16];
#endif

  vp::clock_event *instr_event;

private:

  int irq_req;
  int irq_req_value;

  bool iss_opened;
  int halt_cause;
  int64_t wakeup_latency;
  int bootaddr_offset;
  iss_reg_t hit_reg = 0;
  bool riscv_dbg_unit;

  iss_reg_t ppc;
  iss_reg_t npc;

  int        misaligned_size;
  uint8_t   *misaligned_data;
  iss_addr_t misaligned_addr;
  bool       misaligned_is_write;

  vp::wire_slave<uint32_t> bootaddr_itf;
  vp::wire_slave<bool>     clock_itf;
  vp::wire_slave<bool>     fetchen_itf;
  vp::wire_slave<bool>     flush_cache_itf;
  vp::wire_slave<bool>     halt_itf;
  vp::wire_master<bool>    halt_status_itf;

  vp::Gdbserver_engine *gdbserver;

  bool clock_active;

  static void clock_sync(void *_this, bool active);
  static void bootaddr_sync(void *_this, uint32_t value);
  static void fetchen_sync(void *_this, bool active);
  static void flush_cache_sync(void *_this, bool active);
  static void flush_cache_ack_sync(void *_this, bool active);
  static void halt_sync(void *_this, bool active);
  void halt_core();
};

#endif
