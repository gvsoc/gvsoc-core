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

#include "iss.hpp"


#if defined(ISS_HAS_PERF_COUNTERS)
void check_perf_config_change(Iss *iss, unsigned int pcer, unsigned int pcmr);
#endif

/*
 *   USER CSRS
 */

static bool ustatus_read(Iss *iss, iss_reg_t *value) {
  *value = 0;
  return false;
}

static bool ustatus_write(Iss *iss, unsigned int value) {
  return false;
}



static bool uie_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: uie\n");
  return false;
}

static bool uie_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: uie\n");
  return false;
}




static bool utvec_read(Iss *iss, iss_reg_t *value) {
  *value = 0;
  return false;
}

static bool utvec_write(Iss *iss, unsigned int value) {
  return false;
}



static bool uscratch_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: uscratch\n");
  return false;
}

static bool uscratch_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: uscratch\n");
  return false;
}



static bool uepc_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: uepc\n");
  return false;
}

static bool uepc_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: uepc\n");
  return false;
}



static bool ucause_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: ucause\n");
  return false;
}

static bool ucause_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: ucause\n");
  return false;
}



static bool ubadaddr_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: ubadaddr\n");
  return false;
}

static bool ubadaddr_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: ubadaddr\n");
  return false;
}



static bool uip_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR READ: uip\n");
  return false;
}

static bool uip_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR WRITE: uip\n");
  return false;
}



static bool fflags_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.state.fcsr.fflags.raw;
  return false;
}

static bool fflags_write(Iss *iss, unsigned int value) {
  iss->cpu.state.fcsr.fflags.raw = value;
  return false;
}



static bool frm_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.state.fcsr.frm;
  return false;
}

static bool frm_write(Iss *iss, unsigned int value) {
  iss->cpu.state.fcsr.frm = value;
  return false;
}



static bool fcsr_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.state.fcsr.raw;
  return false;
}

static bool fcsr_write(Iss *iss, unsigned int value) {
  iss->cpu.state.fcsr.raw = value & 0xff;
  return false;
}




static bool cycle_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: cycle\n");
  return false;
}

static bool time_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: time\n");
  return false;
}

static bool instret_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: instret\n");
  return false;
}

static bool hpmcounter_read(Iss *iss, iss_reg_t *value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: hpmcounter\n");
  return false;
}

static bool cycleh_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: cycleh\n");
  return false;
}

static bool timeh_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: timeh\n");
  return false;
}

static bool instreth_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: instreth\n");
  return false;
}

static bool hpmcounterh_read(Iss *iss, iss_reg_t *value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: hpmcounterh\n");
  return false;
}







/*
 *   SUPERVISOR CSRS
 */

static bool sstatus_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->status & 0x133;
  return false;
}

static bool sstatus_write(Iss *iss, unsigned int value) {
  //iss->status = (iss->status & ~0x133) | (value & 0x133);
  //checkInterrupts(iss, 1);
  return false;
}



static bool sedeleg_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: sedeleg\n");
  return false;
}

static bool sedeleg_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: sedeleg\n");
  return false;
}



static bool sideleg_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: sideleg\n");
  return false;
}

static bool sideleg_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: sideleg\n");
  return false;
}




static bool sie_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->ie[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool sie_write(Iss *iss, unsigned int value) {
  //iss->ie[GVSIM_MODE_SUPERVISOR] = value;
  //checkInterrupts(iss, 1);
  return false;
}



static bool stvec_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->tvec[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool stvec_write(Iss *iss, unsigned int value) {
  //iss->tvec[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}


static bool scounteren_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->tvec[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool scounteren_write(Iss *iss, unsigned int value) {
  //iss->tvec[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}


static bool sscratch_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->scratch[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool sscratch_write(Iss *iss, unsigned int value) {
  //iss->scratch[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}



static bool sepc_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->epc[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool sepc_write(Iss *iss, unsigned int value) {
  //iss->epc[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}



static bool scause_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->cause[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool scause_write(Iss *iss, unsigned int value) {
  //iss->cause[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}



static bool stval_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->badaddr[GVSIM_MODE_SUPERVISOR];
  return false;
}

static bool stval_write(Iss *iss, unsigned int value) {
  //iss->badaddr[GVSIM_MODE_SUPERVISOR] = value;
  return false;
}



static bool sip_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: sip\n");
  return false;
}

static bool sip_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: sip\n");
  return false;
}



static bool satp_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->sptbr;
  return false;
}

static bool satp_write(Iss *iss, unsigned int value) {
  //iss->sptbr = value;
  //sim_setPgtab(iss, value);
  return false;
}







/*
 *   MACHINE CSRS
 */

static bool misa_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.config.misa;
  return false;
}

static bool misa_write(Iss *iss, unsigned int value) {
  return false;
}

static bool mvendorid_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mvendorid\n");
  return false;
}

static bool marchid_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: marchid\n");
  return false;
}

static bool mimpid_read(Iss *iss, iss_reg_t *value) {
  *value = 0;
  return false;
}

static bool mhartid_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.config.mhartid;
  return false;
}


static bool mstatus_read(Iss *iss, iss_reg_t *value) {
  *value = (iss->cpu.csr.status & ~(1<<3)) | (iss->cpu.irq.irq_enable << 3) | (iss->cpu.irq.saved_irq_enable << 7);
  return false;
}

static bool mstatus_write(Iss *iss, unsigned int value)
{
  iss_perf_account_dependency_stall(iss, 4);

#if defined(SECURE)
  unsigned int mask = 0x21899;
  unsigned int or_mask = 0x0;
#else
  unsigned int mask = 0x88;
  unsigned int or_mask = 0x1800;
#endif
  iss->cpu.csr.status = (value & mask) | or_mask;
  iss_irq_enable(iss, (value >> 3) & 1);
  iss->cpu.irq.saved_irq_enable = (value >> 7) & 1;
  return false;
}



static bool medeleg_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->edeleg[GVSIM_MODE_MACHINE];
  return false;
}

static bool medeleg_write(Iss *iss, unsigned int value) {
  //iss->edeleg[GVSIM_MODE_MACHINE] = value;
  return false;
}



static bool mideleg_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->ideleg[GVSIM_MODE_MACHINE];
  return false;
}

static bool mideleg_write(Iss *iss, unsigned int value) {
  //iss->ideleg[GVSIM_MODE_MACHINE] = value;
  //checkInterrupts(iss, 1);
  return false;
}



static bool mie_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->ie[GVSIM_MODE_MACHINE];
  return false;
}

static bool mie_write(Iss *iss, unsigned int value) {
  //iss->ie[GVSIM_MODE_MACHINE] = value;
  //checkInterrupts(iss, 1);
  return false;
}



static bool mtvec_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->tvec[GVSIM_MODE_MACHINE];
  *value = iss->cpu.csr.mtvec;
  return false;
}

static bool mtvec_write(Iss *iss, unsigned int value) {
  iss->cpu.csr.mtvec = value;
  //iss->tvec[GVSIM_MODE_MACHINE] = value;
  iss_irq_set_vector_table(iss, value);
  return false;
}


static bool mcounteren_read(Iss *iss, iss_reg_t *value) {
  return false;
}

static bool mcounteren_write(Iss *iss, unsigned int value) {
  return false;
}



static bool mscratch_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.mscratch;
  return false;
}

static bool mscratch_write(Iss *iss, unsigned int value) {
  iss->cpu.csr.mscratch = value;
  return false;
}


static bool mepc_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.epc;
  return false;
}

static bool mepc_write(Iss *iss, unsigned int value) {
  iss->trace.msg("Setting MEPC (value: 0x%x)\n", value);
  iss->cpu.csr.epc = value & ~1;
  return false;
}



static bool mcause_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.mcause;
  return false;
}

static bool mcause_write(Iss *iss, unsigned int value) {
  iss->cpu.csr.mcause = value;
  return false;
}



static bool mtval_read(Iss *iss, iss_reg_t *value) {
  return false;
}

static bool mtval_write(Iss *iss, unsigned int value) {
  return false;
}



static bool mbadaddr_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->badaddr[GVSIM_MODE_MACHINE];
  return false;
}

static bool mbadaddr_write(Iss *iss, unsigned int value) {
 // iss->badaddr[GVSIM_MODE_MACHINE] = value;
  return false;
}



static bool mip_read(Iss *iss, iss_reg_t *value) {
  //*value = iss->ip[GVSIM_MODE_MACHINE];
  return false;
}

static bool mip_write(Iss *iss, unsigned int value) {
  //iss->ip[GVSIM_MODE_MACHINE] = value;
  //checkInterrupts(iss, 1);
  return false;
}


static bool pmpcfg_read(Iss *iss, iss_reg_t *value, int id) {
  return false;
}

static bool pmpcfg_write(Iss *iss, unsigned int value, int id) {
  return false;
}


static bool pmpaddr_read(Iss *iss, iss_reg_t *value, int id) {
  return false;
}

static bool pmpaddr_write(Iss *iss, unsigned int value, int id) {
  return false;
}


static bool mbase_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mbase\n");
  return false;
}

static bool mbase_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mbase\n");
  return false;
}



static bool mbound_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mbound\n");
  return false;
}

static bool mbound_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mbound\n");
  return false;
}



static bool mibase_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mibase\n");
  return false;
}

static bool mibase_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mibase\n");
  return false;
}



static bool mibound_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mibound\n");
  return false;
}

static bool mibound_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mibound\n");
  return false;
}



static bool mdbase_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mdbase\n");
  return false;
}

static bool mdbase_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mdbase\n");
  return false;
}



static bool mdbound_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mdbound\n");
  return false;
}

static bool mdbound_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mdbound\n");
  return false;
}



static bool mcycle_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mcycle\n");
  return false;
}

static bool mcycle_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mcycle\n");
  return false;
}

static bool minstret_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: minstret\n");
  return false;
}

static bool minstret_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: minstret\n");
  return false;
}

static bool mhpmcounter_read(Iss *iss, iss_reg_t *value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmcounter\n");
  return false;
}

static bool mhpmcounter_write(Iss *iss, unsigned int value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmcounter\n");
  return false;
}

static bool mcycleh_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mcycleh\n");
  return false;
}

static bool mcycleh_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mcycleh\n");
  return false;
}

static bool minstreth_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: \n");
  return false;
}

static bool minstreth_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: \n");
  return false;
}

static bool mhpmcounterh_read(Iss *iss, int id, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmcounterh\n");
  return false;
}

static bool mhpmcounterh_write(Iss *iss, int id, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmcounterh\n");
  return false;
}



static bool mucounteren_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mucounteren\n");
  return false;
}

static bool mucounteren_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mucounteren\n");
  return false;
}



static bool mscounteren_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mscounteren\n");
  return false;
}

static bool mscounteren_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mscounteren\n");
  return false;
}



static bool mhcounteren_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: mhcounteren\n");
  return false;
}

static bool mhcounteren_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: mhcounteren\n");
  return false;
}



static bool mhpmevent_read(Iss *iss, iss_reg_t *value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmevent\n");
  return false;
}

static bool mhpmevent_write(Iss *iss, unsigned int value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: mhpmevent\n");
  return false;
}



static bool tselect_read(Iss *iss, iss_reg_t *value) {
  printf("WARNING UNIMPLEMENTED CSR: tselect\n");
  return false;
}

static bool tselect_write(Iss *iss, unsigned int value) {
  printf("WARNING UNIMPLEMENTED CSR: tselect\n");
  return false;
}



static bool tdata_read(Iss *iss, iss_reg_t *value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: tdata\n");
  return false;
}

static bool tdata_write(Iss *iss, unsigned int value, int id) {
  printf("WARNING UNIMPLEMENTED CSR: tdata\n");
  return false;
}




/*
 *   PULP CSRS
 */

static bool stack_conf_write(Iss *iss, iss_reg_t value)
{
  iss->cpu.csr.stack_conf = value;

  if (iss->cpu.csr.stack_conf)
    iss->csr_trace.msg("Activating stack checking (start: 0x%x, end: 0x%x)\n", iss->cpu.csr.stack_start, iss->cpu.csr.stack_end);
  else
    iss->csr_trace.msg("Deactivating stack checking\n");

  return false;
}

static bool stack_conf_read(Iss *iss, iss_reg_t *value)
{
  *value = iss->cpu.csr.stack_conf;
  return false;
}

static bool stack_start_write(Iss *iss, iss_reg_t value)
{
  iss->cpu.csr.stack_start = value;
  return false;
}

static bool stack_start_read(Iss *iss, iss_reg_t *value)
{
  *value = iss->cpu.csr.stack_start;
  return false;
}

static bool stack_end_write(Iss *iss, iss_reg_t value)
{
  iss->cpu.csr.stack_end = value;
  return false;
}

static bool stack_end_read(Iss *iss, iss_reg_t *value)
{
  *value = iss->cpu.csr.stack_end;
  return false;
}

static bool umode_read(Iss *iss, iss_reg_t *value) {
  *value = 3;
  //*value = iss->state.mode;
  return false;
}

static bool dcsr_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.dcsr;
  return false;
}

static bool dcsr_write(Iss *iss, iss_reg_t value) {
  iss->cpu.csr.dcsr = value;
  iss->step_mode.set((value >> 2) & 1);
  return false;
}

static bool depc_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.depc;
  return false;
}

static bool depc_write(Iss *iss, iss_reg_t value) {
  iss->cpu.csr.depc = value;
  return false;
}



static bool dscratch_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.scratch0;
  return false;
}

static bool dscratch_write(Iss *iss, unsigned int value) {
  iss->cpu.csr.scratch0 = value;
  return false;
}

static bool scratch0_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.scratch0;
  return false;
}

static bool scratch0_write(Iss *iss, iss_reg_t value) {
  iss->cpu.csr.scratch0 = value;
  return false;
}

static bool scratch1_read(Iss *iss, iss_reg_t *value) {
  *value = iss->cpu.csr.scratch1;
  return false;
}

static bool scratch1_write(Iss *iss, iss_reg_t value) {
  iss->cpu.csr.scratch1 = value;
  return false;
}


static bool pcer_write(Iss *iss, unsigned int prev_val, unsigned int value) {
  iss->cpu.csr.pcer = value & 0x7fffffff;
  check_perf_config_change(iss, prev_val, iss->cpu.csr.pcmr);
  return false;
}

static bool pcmr_write(Iss *iss, unsigned int prev_val, unsigned int value) {
  iss->cpu.csr.pcmr = value;

  check_perf_config_change(iss, iss->cpu.csr.pcer, prev_val);
  return false;
}

#if defined(CSR_HWLOOP0_START)
static bool hwloop_read(Iss *iss, int reg, iss_reg_t *value) {
  *value = iss->cpu.pulpv2.hwloop_regs[reg];
  return false;
}

static bool hwloop_write(Iss *iss, int reg, unsigned int value) {
  iss->cpu.pulpv2.hwloop_regs[reg] = value;

  // Since the HW loop is using decode instruction for the HW loop start to jump faster
  // we need to recompute it when it is modified.
  if (reg == 0)
  {
      iss->cpu.state.hwloop_start_insn[0] = insn_cache_get(iss, value);
  }
  else if (reg == 4)
  {
      iss->cpu.state.hwloop_start_insn[1] = insn_cache_get(iss, value);
  }

  return false;
}
#endif

#if defined(ISS_HAS_PERF_COUNTERS)

static inline void iss_csr_ext_counter_set(Iss *iss, int id, unsigned int value)
{
  if (!iss->ext_counter[id].is_bound())
  {
    iss->trace.warning("Trying to access external counter through CSR while it is not connected (id: %d)\n", id);
  }
  else
  {
    iss->ext_counter[id].sync(value);
  }
}

static inline void iss_csr_ext_counter_get(Iss *iss, int id, unsigned int *value)
{
  if (!iss->ext_counter[id].is_bound())
  {
    iss->trace.force_warning("Trying to access external counter through CSR while it is not connected (id: %d)\n", id);
  }
  else
  {
    iss->ext_counter[id].sync_back(value);
  }
}

void update_external_pccr(Iss *iss, int id, unsigned int pcer, unsigned int pcmr) {
  unsigned int incr = 0;
  // Only update if the counter is active as the external signal may report 
  // a different value whereas the counter must remain the same
  if (((pcer & CSR_PCER_EVENT_MASK(id)) && (pcmr & CSR_PCMR_ACTIVE)) || iss->perf_event_is_active(id))
  {
    iss_csr_ext_counter_get(iss, id, &incr);
    iss->cpu.csr.pccr[id] += incr;
    iss->perf_event_incr(id, incr);
  }
  else {
    // Nothing to do if the counter is inactive, it will get reset so that
    // we get read events from now if it becomes active
  }

  // Reset the counter
  if (iss->ext_counter[id].is_bound())
    iss_csr_ext_counter_set(iss, id, 0);

  //if (cpu->traceEvent) sim_trace_event_incr(cpu, id, incr);
}

void check_perf_config_change(Iss *iss, unsigned int pcer, unsigned int pcmr)
{
  // In case PCER or PCMR is modified, there is a special care about external signals as they
  // are still counting whatever the event active flag is. Reset them to start again from a
  // clean state
  {
    int i;
    // Check every external signal separatly
    for (i=CSR_PCER_NB_INTERNAL_EVENTS; i<CSR_PCER_NB_EVENTS; i++) {
      // This will update our internal counter in case it is active or just reset it
      update_external_pccr(iss, i, pcer, pcmr);
    }
  }
}

void flushExternalCounters(Iss *iss)
{
  int i;
  for (i=CSR_PCER_NB_INTERNAL_EVENTS; i<CSR_PCER_NB_EVENTS; i++)
  {
    update_external_pccr(iss, i, iss->cpu.csr.pcer, iss->cpu.csr.pcmr);
  }
}

static bool perfCounters_read(Iss *iss, int reg, iss_reg_t *value) {
  // In case of counters connected to external signals, we need to synchronize first
  if (reg >= CSR_PCCR(CSR_PCER_NB_INTERNAL_EVENTS) && reg < CSR_PCCR(CSR_NB_PCCR))
  {
    update_external_pccr(iss, reg - CSR_PCCR(0), iss->cpu.csr.pcer, iss->cpu.csr.pcmr);
    *value = iss->cpu.csr.pccr[reg - CSR_PCCR(0)];
    iss->perf_counter_trace.msg("Reading PCCR (index: %d, value: 0x%x)\n", reg - CSR_PCCR(0), *value);
  }
  else if (reg == CSR_PCER)
  {
    *value = iss->cpu.csr.pcer;
  }
  else if (reg == CSR_PCMR)
  {
    *value = iss->cpu.csr.pcmr;
  }
  else
  {
    *value = iss->cpu.csr.pccr[reg - CSR_PCCR(0)];
    iss->perf_counter_trace.msg("Reading PCCR (index: %d, value: 0x%x)\n", reg - CSR_PCCR(0), *value);
  }


  return false;
}

static bool perfCounters_write(Iss *iss, int reg, unsigned int value)
{
  if (reg == CSR_PCER)
  {
    iss->perf_counter_trace.msg("Setting PCER (value: 0x%x)\n", value);
    return pcer_write(iss, iss->cpu.csr.pcer, value);
  }
  else if (reg == CSR_PCMR)
  {
    iss->perf_counter_trace.msg("Setting PCMR (value: 0x%x)\n", value);
    return pcmr_write(iss, iss->cpu.csr.pcmr, value);
  }
  // In case of counters connected to external signals, we need to synchronize the external one
  // with our
  else if (reg >= CSR_PCCR(CSR_PCER_NB_INTERNAL_EVENTS) && reg < CSR_PCCR(CSR_NB_PCCR))
  {
      // This will update out counter, which will be overwritten afterwards by the new value and
      // also set the external counter to 0 which makes sure they are synchroninez
    update_external_pccr(iss, reg - CSR_PCCR(0), iss->cpu.csr.pcer, iss->cpu.csr.pcmr);
  }
  else if (reg == CSR_PCCR(CSR_NB_PCCR)) 
  {
    iss->perf_counter_trace.msg("Setting value to all PCCR (value: 0x%x)\n", value);

    int i;
    for (i=0; i<31; i++)
    {
      iss->cpu.csr.pccr[i] = value;
      if (i >= CSR_PCER_NB_INTERNAL_EVENTS  && i < CSR_PCER_NB_EVENTS)
      {
        update_external_pccr(iss, i, 0, 0);
      }
    }  
  }
  else
  {
    iss->perf_counter_trace.msg("Setting PCCR value (pccr: %d, value: 0x%x)\n", reg - CSR_PCCR(0), value);
    iss->cpu.csr.pccr[reg - CSR_PCCR(0)] = value;
  }
  return false;
}
#endif



static bool checkCsrAccess(Iss *iss, int reg, bool isWrite) {
  //bool isRw = (reg >> 10) & 0x3;
  //bool priv = (reg >> 8) & 0x3;
  //if ((isWrite && !isRw) || (priv > iss->state.mode)) {
  //  triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
  //  return true;
  //}
  return false;
}

bool iss_csr_read(Iss *iss, iss_reg_t reg, iss_reg_t *value)
{
  bool status;

  iss->csr_trace.msg("Reading CSR (reg: 0x%x)\n", reg);

  #if 0
  // First check permissions
  if (checkCsrAccess(iss, reg, 0)) return true;


  if (!getOption(iss, __priv_pulp)) {
    if (reg >= 0xC03 && reg <= 0xC1F) return hpmcounter_read(iss, reg - 0xC03, value);
    if (reg >= 0xC83 && reg <= 0xC9F) return hpmcounterh_read(iss, reg - 0xC83, value);

    if (reg >= 0xB03 && reg <= 0xB1F) return mhpmcounter_read(iss, reg - 0xB03, value);
    if (reg >= 0xB83 && reg <= 0xB9F) return mhpmcounterh_read(iss, reg - 0xB83, value);

    if (reg >= 0x323 && reg <= 0x33F) return mhpmevent_read(iss, reg - 0x323, value);
  }
  #endif

  // And dispatch
  switch (reg) {

    // User trap setup
    case 0x000: status = ustatus_read   (iss, value); break;
    case 0x004: status = uie_read       (iss, value); break;
    case 0x005: status = utvec_read     (iss, value); break;

    // User trap handling
    case 0x040: status = uscratch_read  (iss, value); break;
    case 0x041: status = uepc_read      (iss, value); break;
    case 0x042: status = ucause_read    (iss, value); break;
    case 0x043: status = ubadaddr_read  (iss, value); break;
    case 0x044: status = uip_read       (iss, value); break;

    // User floating-point CSRs
    case 0x001: status = fflags_read    (iss, value); break;
    case 0x002: status = frm_read       (iss, value); break;
    case 0x003: status = fcsr_read      (iss, value); break;

    // User counter / timers
    case 0xC00: status = cycle_read      (iss, value); break;
    case 0xC01: status = time_read       (iss, value); break;
    case 0xC02: status = instret_read    (iss, value); break;
    case 0xC03: status = hpmcounter_read (iss, value, 3); break;
    case 0xC04: status = hpmcounter_read (iss, value, 4); break;
    case 0xC05: status = hpmcounter_read (iss, value, 5); break;
    case 0xC06: status = hpmcounter_read (iss, value, 6); break;
    case 0xC07: status = hpmcounter_read (iss, value, 7); break;
    case 0xC08: status = hpmcounter_read (iss, value, 8); break;
    case 0xC09: status = hpmcounter_read (iss, value, 9); break;
    case 0xC0A: status = hpmcounter_read (iss, value, 10); break;
    case 0xC0B: status = hpmcounter_read (iss, value, 11); break;
    case 0xC0C: status = hpmcounter_read (iss, value, 12); break;
    case 0xC0D: status = hpmcounter_read (iss, value, 13); break;
    case 0xC0E: status = hpmcounter_read (iss, value, 14); break;
    case 0xC0F: status = hpmcounter_read (iss, value, 15); break;
    case 0xC10: status = umode_read(iss, value); break;
    // case 0xC10: status = hpmcounter_read (iss, value, 16); break;
    case 0xC11: status = hpmcounter_read (iss, value, 17); break;
    case 0xC12: status = hpmcounter_read (iss, value, 18); break;
    case 0xC13: status = hpmcounter_read (iss, value, 19); break;
    case 0xC14: status = hpmcounter_read (iss, value, 20); break;
    case 0xC15: status = hpmcounter_read (iss, value, 21); break;
    case 0xC16: status = hpmcounter_read (iss, value, 22); break;
    case 0xC17: status = hpmcounter_read (iss, value, 23); break;
    case 0xC18: status = hpmcounter_read (iss, value, 24); break;
    case 0xC19: status = hpmcounter_read (iss, value, 25); break;
    case 0xC1A: status = hpmcounter_read (iss, value, 26); break;
    case 0xC1B: status = hpmcounter_read (iss, value, 27); break;
    case 0xC1C: status = hpmcounter_read (iss, value, 28); break;
    case 0xC1D: status = hpmcounter_read (iss, value, 29); break;
    case 0xC1E: status = hpmcounter_read (iss, value, 30); break;
    case 0xC1F: status = hpmcounter_read (iss, value, 31); break;
    case 0xC80: status = cycleh_read     (iss, value); break;
    case 0xC81: status = timeh_read      (iss, value); break;
    case 0xC82: status = instreth_read   (iss, value); break;
    case 0xC83: status = hpmcounterh_read (iss, value, 3); break;
    case 0xC84: status = hpmcounterh_read (iss, value, 4); break;
    case 0xC85: status = hpmcounterh_read (iss, value, 5); break;
    case 0xC86: status = hpmcounterh_read (iss, value, 6); break;
    case 0xC87: status = hpmcounterh_read (iss, value, 7); break;
    case 0xC88: status = hpmcounterh_read (iss, value, 8); break;
    case 0xC89: status = hpmcounterh_read (iss, value, 9); break;
    case 0xC8A: status = hpmcounterh_read (iss, value, 10); break;
    case 0xC8B: status = hpmcounterh_read (iss, value, 11); break;
    case 0xC8C: status = hpmcounterh_read (iss, value, 12); break;
    case 0xC8D: status = hpmcounterh_read (iss, value, 13); break;
    case 0xC8E: status = hpmcounterh_read (iss, value, 14); break;
    case 0xC8F: status = hpmcounterh_read (iss, value, 15); break;
    case 0xC90: status = hpmcounterh_read (iss, value, 16); break;
    case 0xC91: status = hpmcounterh_read (iss, value, 17); break;
    case 0xC92: status = hpmcounterh_read (iss, value, 18); break;
    case 0xC93: status = hpmcounterh_read (iss, value, 19); break;
    case 0xC94: status = hpmcounterh_read (iss, value, 20); break;
    case 0xC95: status = hpmcounterh_read (iss, value, 21); break;
    case 0xC96: status = hpmcounterh_read (iss, value, 22); break;
    case 0xC97: status = hpmcounterh_read (iss, value, 23); break;
    case 0xC98: status = hpmcounterh_read (iss, value, 24); break;
    case 0xC99: status = hpmcounterh_read (iss, value, 25); break;
    case 0xC9A: status = hpmcounterh_read (iss, value, 26); break;
    case 0xC9B: status = hpmcounterh_read (iss, value, 27); break;
    case 0xC9C: status = hpmcounterh_read (iss, value, 28); break;
    case 0xC9D: status = hpmcounterh_read (iss, value, 29); break;
    case 0xC9E: status = hpmcounterh_read (iss, value, 30); break;
    case 0xC9F: status = hpmcounterh_read (iss, value, 31); break;




    // Supervisor trap setup
    case 0x100: status = sstatus_read    (iss, value); break;
    case 0x102: status = sedeleg_read    (iss, value); break;
    case 0x103: status = sideleg_read    (iss, value); break;
    case 0x104: status = sie_read        (iss, value); break;
    case 0x105: status = stvec_read      (iss, value); break;
    case 0x106: status = scounteren_read (iss, value); break;

    // Supervisor trap handling
    case 0x140: status = sscratch_read  (iss, value); break;
    case 0x141: status = sepc_read      (iss, value); break;
    case 0x142: status = scause_read    (iss, value); break;
    case 0x143: status = stval_read  (iss, value); break;
    case 0x144: status = sip_read       (iss, value); break;

    // Supervisor protection and translation
    case 0x180: status = satp_read     (iss, value); break;




    // Machine information registers
    case 0xF11: status = mvendorid_read  (iss, value); break;
    case 0xF12: status = marchid_read    (iss, value); break;
    case 0xF13: status = mimpid_read     (iss, value); break;
    case 0xF14: status = mhartid_read    (iss, value); break;

    // Machine trap setup
    case 0x300: status = mstatus_read    (iss, value); break;
    case 0x301: status = misa_read       (iss, value); break;
    case 0x302: status = medeleg_read    (iss, value); break;
    case 0x303: status = mideleg_read    (iss, value); break;
    case 0x304: status = mie_read        (iss, value); break;
    case 0x305: status = mtvec_read      (iss, value); break;
    case 0x306: status = mcounteren_read (iss, value); break;

    // Machine trap handling
    case 0x340: status = mscratch_read   (iss, value); break;
    case 0x341: status = mepc_read       (iss, value); break;
    case 0x342: status = mcause_read     (iss, value); break;
    case 0x343: status = mtval_read       (iss, value); break;
    case 0x344: status = mip_read        (iss, value); break;

    // Machine protection and translation
    case 0x3A0: status = pmpcfg_read   (iss, value, 0); break;
    case 0x3A1: status = pmpcfg_read   (iss, value, 1); break;
    case 0x3A2: status = pmpcfg_read   (iss, value, 2); break;
    case 0x3A3: status = pmpcfg_read   (iss, value, 3); break;
    case 0x3B0: status = pmpaddr_read  (iss, value, 0); break;
    case 0x3B1: status = pmpaddr_read  (iss, value, 1); break;
    case 0x3B2: status = pmpaddr_read  (iss, value, 2); break;
    case 0x3B3: status = pmpaddr_read  (iss, value, 3); break;
    case 0x3B4: status = pmpaddr_read  (iss, value, 4); break;
    case 0x3B5: status = pmpaddr_read  (iss, value, 5); break;
    case 0x3B6: status = pmpaddr_read  (iss, value, 6); break;
    case 0x3B7: status = pmpaddr_read  (iss, value, 7); break;
    case 0x3B8: status = pmpaddr_read  (iss, value, 8); break;
    case 0x3B9: status = pmpaddr_read  (iss, value, 9); break;
    case 0x3BA: status = pmpaddr_read  (iss, value, 10); break;
    case 0x3BB: status = pmpaddr_read  (iss, value, 11); break;
    case 0x3BC: status = pmpaddr_read  (iss, value, 12); break;
    case 0x3BD: status = pmpaddr_read  (iss, value, 13); break;
    case 0x3BE: status = pmpaddr_read  (iss, value, 14); break;
    case 0x3BF: status = pmpaddr_read  (iss, value, 15); break;

    // Machine timers and counters
    case 0xB00: status = mcycle_read     (iss, value); break;
    case 0xB02: status = minstret_read   (iss, value); break;
    case 0xB03: status = mhpmcounter_read (iss, value, 3); break;
    case 0xB04: status = mhpmcounter_read (iss, value, 4); break;
    case 0xB05: status = mhpmcounter_read (iss, value, 5); break;
    case 0xB06: status = mhpmcounter_read (iss, value, 6); break;
    case 0xB07: status = mhpmcounter_read (iss, value, 7); break;
    case 0xB08: status = mhpmcounter_read (iss, value, 8); break;
    case 0xB09: status = mhpmcounter_read (iss, value, 9); break;
    case 0xB0A: status = mhpmcounter_read (iss, value, 10); break;
    case 0xB0B: status = mhpmcounter_read (iss, value, 11); break;
    case 0xB0C: status = mhpmcounter_read (iss, value, 12); break;
    case 0xB0D: status = mhpmcounter_read (iss, value, 13); break;
    case 0xB0E: status = mhpmcounter_read (iss, value, 14); break;
    case 0xB0F: status = mhpmcounter_read (iss, value, 15); break;
    case 0xB10: status = mhpmcounter_read (iss, value, 16); break;
    case 0xB11: status = mhpmcounter_read (iss, value, 17); break;
    case 0xB12: status = mhpmcounter_read (iss, value, 18); break;
    case 0xB13: status = mhpmcounter_read (iss, value, 19); break;
    case 0xB14: status = mhpmcounter_read (iss, value, 20); break;
    case 0xB15: status = mhpmcounter_read (iss, value, 21); break;
    case 0xB16: status = mhpmcounter_read (iss, value, 22); break;
    case 0xB17: status = mhpmcounter_read (iss, value, 23); break;
    case 0xB18: status = mhpmcounter_read (iss, value, 24); break;
    case 0xB19: status = mhpmcounter_read (iss, value, 25); break;
    case 0xB1A: status = mhpmcounter_read (iss, value, 26); break;
    case 0xB1B: status = mhpmcounter_read (iss, value, 27); break;
    case 0xB1C: status = mhpmcounter_read (iss, value, 28); break;
    case 0xB1D: status = mhpmcounter_read (iss, value, 29); break;
    case 0xB1E: status = mhpmcounter_read (iss, value, 30); break;
    case 0xB1F: status = mhpmcounter_read (iss, value, 31); break;
    case 0xB80: status = mcycleh_read     (iss, value); break;
    case 0xB82: status = minstreth_read   (iss, value); break;
    case 0xB83: status = mhpmcounter_read (iss, value, 3); break;
    case 0xB84: status = mhpmcounter_read (iss, value, 4); break;
    case 0xB85: status = mhpmcounter_read (iss, value, 5); break;
    case 0xB86: status = mhpmcounter_read (iss, value, 6); break;
    case 0xB87: status = mhpmcounter_read (iss, value, 7); break;
    case 0xB88: status = mhpmcounter_read (iss, value, 8); break;
    case 0xB89: status = mhpmcounter_read (iss, value, 9); break;
    case 0xB8A: status = mhpmcounter_read (iss, value, 10); break;
    case 0xB8B: status = mhpmcounter_read (iss, value, 11); break;
    case 0xB8C: status = mhpmcounter_read (iss, value, 12); break;
    case 0xB8D: status = mhpmcounter_read (iss, value, 13); break;
    case 0xB8E: status = mhpmcounter_read (iss, value, 14); break;
    case 0xB8F: status = mhpmcounter_read (iss, value, 15); break;
    case 0xB90: status = mhpmcounter_read (iss, value, 16); break;
    case 0xB91: status = mhpmcounter_read (iss, value, 17); break;
    case 0xB92: status = mhpmcounter_read (iss, value, 18); break;
    case 0xB93: status = mhpmcounter_read (iss, value, 19); break;
    case 0xB94: status = mhpmcounter_read (iss, value, 20); break;
    case 0xB95: status = mhpmcounter_read (iss, value, 21); break;
    case 0xB96: status = mhpmcounter_read (iss, value, 22); break;
    case 0xB97: status = mhpmcounter_read (iss, value, 23); break;
    case 0xB98: status = mhpmcounter_read (iss, value, 24); break;
    case 0xB99: status = mhpmcounter_read (iss, value, 25); break;
    case 0xB9A: status = mhpmcounter_read (iss, value, 26); break;
    case 0xB9B: status = mhpmcounter_read (iss, value, 27); break;
    case 0xB9C: status = mhpmcounter_read (iss, value, 28); break;
    case 0xB9D: status = mhpmcounter_read (iss, value, 29); break;
    case 0xB9E: status = mhpmcounter_read (iss, value, 30); break;
    case 0xB9F: status = mhpmcounter_read (iss, value, 31); break;

    // Machine counter setup
    case 0x323: status = mhpmevent_read (iss, value, 3); break;
    case 0x324: status = mhpmevent_read (iss, value, 4); break;
    case 0x325: status = mhpmevent_read (iss, value, 5); break;
    case 0x326: status = mhpmevent_read (iss, value, 6); break;
    case 0x327: status = mhpmevent_read (iss, value, 7); break;
    case 0x328: status = mhpmevent_read (iss, value, 8); break;
    case 0x329: status = mhpmevent_read (iss, value, 9); break;
    case 0x32A: status = mhpmevent_read (iss, value, 10); break;
    case 0x32B: status = mhpmevent_read (iss, value, 11); break;
    case 0x32C: status = mhpmevent_read (iss, value, 12); break;
    case 0x32D: status = mhpmevent_read (iss, value, 13); break;
    case 0x32E: status = mhpmevent_read (iss, value, 14); break;
    case 0x32F: status = mhpmevent_read (iss, value, 15); break;
    case 0x330: status = mhpmevent_read (iss, value, 16); break;
    case 0x331: status = mhpmevent_read (iss, value, 17); break;
    case 0x332: status = mhpmevent_read (iss, value, 18); break;
    case 0x333: status = mhpmevent_read (iss, value, 19); break;
    case 0x334: status = mhpmevent_read (iss, value, 20); break;
    case 0x335: status = mhpmevent_read (iss, value, 21); break;
    case 0x336: status = mhpmevent_read (iss, value, 22); break;
    case 0x337: status = mhpmevent_read (iss, value, 23); break;
    case 0x338: status = mhpmevent_read (iss, value, 24); break;
    case 0x339: status = mhpmevent_read (iss, value, 25); break;
    case 0x33A: status = mhpmevent_read (iss, value, 26); break;
    case 0x33B: status = mhpmevent_read (iss, value, 27); break;
    case 0x33C: status = mhpmevent_read (iss, value, 28); break;
    case 0x33D: status = mhpmevent_read (iss, value, 29); break;
    case 0x33E: status = mhpmevent_read (iss, value, 30); break;
    case 0x33F: status = mhpmevent_read (iss, value, 31); break;

    // Debug/Trace registers (shared with debug mode)
    case 0x7A0: status = tselect_read (iss, value); break;
    case 0x7A1: status = tdata_read (iss, value, 1); break;
    case 0x7A2: status = tdata_read (iss, value, 2); break;
    case 0x7A3: status = tdata_read (iss, value, 3); break;

    // Debug mode registers
    case 0x7B0: status = dcsr_read (iss, value); break;
    case 0x7B1: status = depc_read (iss, value); break;
    case 0x7B2: status = dscratch_read (iss, value); break;

    // PULP extensions
    case 0x014: status = mhartid_read(iss, value); break;

#if CSR_HWLOOP0_START != 0x7b0
    case 0x7b3: status = scratch1_read(iss, value); break;
#endif

#ifdef CSR_STACK_CONF
    case CSR_STACK_CONF:  status = stack_conf_read(iss, value); break;
    case CSR_STACK_START: status = stack_start_read(iss, value); break;
    case CSR_STACK_END:   status = stack_end_read(iss, value); break;
#endif

    default:

#if defined(ISS_HAS_PERF_COUNTERS)
    if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR)
    {
      status = perfCounters_read(iss, reg, value);
    }
#endif

#if defined(CSR_HWLOOP0_START)
    else if (iss->cpu.pulpv2.hwloop)
    {
      if (reg >= CSR_HWLOOP0_START && reg <= CSR_HWLOOP1_COUNTER)
      {
        status = hwloop_read(iss, reg - CSR_HWLOOP0_START, value);
      }
    }
#endif

    else
    {
#if 0
      triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
#endif
      return true;
    }
  }

  iss->csr_trace.msg("Read CSR (reg: 0x%x, value: 0x%x)\n", reg, value);

  return status;
}

bool iss_csr_write(Iss *iss, iss_reg_t reg, iss_reg_t value)
{
  iss->csr_trace.msg("Writing CSR (reg: 0x%x, value: 0x%x)\n", reg, value);

  // If there is any write to a CSR, switch to full check instruction handler
  // in case something special happened (like HW counting become active)
  iss->trigger_check_all();

#if 0
  // First check permissions
  if (checkCsrAccess(iss, reg, 0)) return true;

  if (!getOption(iss, __priv_pulp)) {
    if (reg >= 0xB03 && reg <= 0xB1F) return mhpmcounter_write(iss, reg - 0xB03, value);
    if (reg >= 0xB83 && reg <= 0xB9F) return mhpmcounterh_write(iss, reg - 0xB83, value);

    if (reg >= 0x323 && reg <= 0x33F) return mhpmevent_write(iss, reg - 0x323, value);
  }
#endif

  // And dispatch
  switch (reg) {

    // User trap setup
    case 0x000: return ustatus_write   (iss, value);
    case 0x004: return uie_write       (iss, value);
    case 0x005: return utvec_write     (iss, value);

    // User trap handling
    case 0x040: return uscratch_write  (iss, value);
    case 0x041: return uepc_write      (iss, value);
    case 0x042: return ucause_write    (iss, value);
    case 0x043: return ubadaddr_write  (iss, value);
    case 0x044: return uip_write       (iss, value);

    // User floating-point CSRs
    case 0x001: return fflags_write    (iss, value);
    case 0x002: return frm_write       (iss, value);
    case 0x003: return fcsr_write      (iss, value);




    // Supervisor trap setup
    case 0x100: return sstatus_write   (iss, value);
    case 0x102: return sedeleg_write   (iss, value);
    case 0x103: return sideleg_write   (iss, value);
    case 0x104: return sie_write       (iss, value);
    case 0x105: return stvec_write     (iss, value);

    // Supervisor trap handling
    case 0x140: return sscratch_write  (iss, value);
    case 0x141: return sepc_write      (iss, value);
    case 0x142: return scause_write    (iss, value);
    case 0x144: return sip_write       (iss, value);

    // Supervisor protection and translation





    // Machine trap setup
    case 0x300: return mstatus_write    (iss, value);
    case 0x301: return misa_write       (iss, value);
    case 0x302: return medeleg_write    (iss, value);
    case 0x303: return mideleg_write    (iss, value);
    case 0x304: return mie_write        (iss, value);
    case 0x305: return mtvec_write      (iss, value);

    // Machine trap handling
    case 0x340: return mscratch_write   (iss, value);
    case 0x341: return mepc_write       (iss, value);
    case 0x342: return mcause_write     (iss, value);
    case 0x343: return mbadaddr_write   (iss, value);
    case 0x344: return mip_write        (iss, value);

    // Machine protection and translation
    case 0x380: return mbase_write      (iss, value);
    case 0x381: return mbound_write     (iss, value);
    case 0x382: return mibase_write     (iss, value);
    case 0x383: return mibound_write    (iss, value);
    case 0x384: return mdbase_write     (iss, value);
    case 0x385: return mdbound_write    (iss, value);

    // Machine timers and counters
    case 0xB00: return mcycle_write     (iss, value);
    case 0xB02: return minstret_write   (iss, value);
    case 0xB80: return mcycleh_write    (iss, value);
    case 0xB82: return minstreth_write  (iss, value);

    // Machine counter setup
    case 0x310: return mucounteren_write(iss, value);
    case 0x311: return mscounteren_write(iss, value);
    case 0x312: return mhcounteren_write(iss, value);

#if CSR_HWLOOP0_START != 0x7b0
    case 0x7b0: return dcsr_write    (iss, value);
    case 0x7b1: return depc_write    (iss, value);
    case 0x7b2: return scratch0_write(iss, value);
    case 0x7b3: return scratch1_write(iss, value);
#endif

#ifdef CSR_STACK_CONF
    case CSR_STACK_CONF:  return stack_conf_write(iss, value); break;
    case CSR_STACK_START: return stack_start_write(iss, value); break;
    case CSR_STACK_END:   return stack_end_write(iss, value); break;
#endif

  }

#if defined(ISS_HAS_PERF_COUNTERS)
  if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR) return perfCounters_write(iss, reg, value);
#endif

#if defined(CSR_HWLOOP0_START)
  if (iss->cpu.pulpv2.hwloop)
  {
    if (reg >= CSR_HWLOOP0_START && reg <= CSR_HWLOOP1_COUNTER) return hwloop_write(iss, reg - CSR_HWLOOP0_START, value);
  }
#endif

#if 0
  triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
#endif

  return true;
}

void iss_csr_init(Iss *iss, int reset)
{
  iss->cpu.csr.status = 0x3 << 11;
  iss->cpu.csr.mcause = 0;
#if defined(ISS_HAS_PERF_COUNTERS)
  iss->cpu.csr.pcmr = 0;
  iss->cpu.csr.pcer = 3;
#endif
  iss->cpu.csr.stack_conf = 0;
  iss->cpu.csr.dcsr = 4 << 28;
}

const char *iss_csr_name(Iss *iss, iss_reg_t reg)
{
  switch (reg) {

    // User trap setup
    case 0x000: return "ustatus";
    case 0x004: return "uie";
    case 0x005: return "utvec";

    // User trap handling
    case 0x040: return "uscratch";
    case 0x041: return "uepc";
    case 0x042: return "ucause";
    case 0x043: return "ubadaddr";
    case 0x044: return "uip";

    // User floating-point CSRs
    case 0x001: return "fflags";
    case 0x002: return "frm";
    case 0x003: return "fcsr";

    // User counter / timers
    case 0xC00: return "cycle";
    case 0xC01: return "time";
    case 0xC02: return "instret";
    case 0xC03: return "hpmcounter";
    case 0xC04: return "hpmcounter";
    case 0xC05: return "hpmcounter";
    case 0xC06: return "hpmcounter";
    case 0xC07: return "hpmcounter";
    case 0xC08: return "hpmcounter";
    case 0xC09: return "hpmcounter";
    case 0xC0A: return "hpmcounter";
    case 0xC0B: return "hpmcounter";
    case 0xC0C: return "hpmcounter";
    case 0xC0D: return "hpmcounter";
    case 0xC0E: return "hpmcounter";
    case 0xC0F: return "hpmcounter";
    case 0xC10: return "hpmcounter";
    case 0xC11: return "hpmcounter";
    case 0xC12: return "hpmcounter";
    case 0xC13: return "hpmcounter";
    case 0xC14: return "hpmcounter";
    case 0xC15: return "hpmcounter";
    case 0xC16: return "hpmcounter";
    case 0xC17: return "hpmcounter";
    case 0xC18: return "hpmcounter";
    case 0xC19: return "hpmcounter";
    case 0xC1A: return "hpmcounter";
    case 0xC1B: return "hpmcounter";
    case 0xC1C: return "hpmcounter";
    case 0xC1D: return "hpmcounter";
    case 0xC1E: return "hpmcounter";
    case 0xC1F: return "hpmcounter";
    case 0xC80: return "cycleh";
    case 0xC81: return "timeh";
    case 0xC82: return "instreth";
    case 0xC83: return "hpmcounterh3";
    case 0xC84: return "hpmcounterh4";
    case 0xC85: return "hpmcounterh5";
    case 0xC86: return "hpmcounterh6";
    case 0xC87: return "hpmcounterh7";
    case 0xC88: return "hpmcounterh8";
    case 0xC89: return "hpmcounterh9";
    case 0xC8A: return "hpmcounterh10";
    case 0xC8B: return "hpmcounterh11";
    case 0xC8C: return "hpmcounterh12";
    case 0xC8D: return "hpmcounterh13";
    case 0xC8E: return "hpmcounterh14";
    case 0xC8F: return "hpmcounterh15";
    case 0xC90: return "hpmcounterh16";
    case 0xC91: return "hpmcounterh17";
    case 0xC92: return "hpmcounterh18";
    case 0xC93: return "hpmcounterh19";
    case 0xC94: return "hpmcounterh20";
    case 0xC95: return "hpmcounterh21";
    case 0xC96: return "hpmcounterh22";
    case 0xC97: return "hpmcounterh23";
    case 0xC98: return "hpmcounterh24";
    case 0xC99: return "hpmcounterh25";
    case 0xC9A: return "hpmcounterh26";
    case 0xC9B: return "hpmcounterh27";
    case 0xC9C: return "hpmcounterh28";
    case 0xC9D: return "hpmcounterh29";
    case 0xC9E: return "hpmcounterh30";
    case 0xC9F: return "hpmcounterh31";




    // Supervisor trap setup
    case 0x100: return "sstatus";
    case 0x102: return "sedeleg";
    case 0x103: return "sideleg";
    case 0x104: return "sie";
    case 0x105: return "stvec";
    case 0x106: return "scounteren";

    // Supervisor trap handling
    case 0x140: return "sscratch";
    case 0x141: return "sepc";
    case 0x142: return "scause";
    case 0x143: return "stval";
    case 0x144: return "sip";

    // Supervisor protection and translation
    case 0x180: return "satp";




    // Machine information registers
    case 0xF11: return "mvendorid";
    case 0xF12: return "marchid";
    case 0xF13: return "mimpid";
    case 0xF14: return "mhartid";

    // Machine trap setup
    case 0x300: return "mstatus";
    case 0x301: return "misa";
    case 0x302: return "medeleg";
    case 0x303: return "mideleg";
    case 0x304: return "mie";
    case 0x305: return "mtvec";
    case 0x306: return "mcounteren";

    // Machine trap handling
    case 0x340: return "mscratch";
    case 0x341: return "mepc";
    case 0x342: return "mcause";
    case 0x343: return "mtval";
    case 0x344: return "mip";

    // Machine protection and translation
    case 0x3A0: return "pmpcfg0";
    case 0x3A1: return "pmpcfg1";
    case 0x3A2: return "pmpcfg2";
    case 0x3A3: return "pmpcfg3";
    case 0x3B0: return "pmpaddr0";
    case 0x3B1: return "pmpaddr1";
    case 0x3B2: return "pmpaddr2";
    case 0x3B3: return "pmpaddr3";
    case 0x3B4: return "pmpaddr4";
    case 0x3B5: return "pmpaddr5";
    case 0x3B6: return "pmpaddr6";
    case 0x3B7: return "pmpaddr7";
    case 0x3B8: return "pmpaddr8";
    case 0x3B9: return "pmpaddr9";
    case 0x3BA: return "pmpaddr10";
    case 0x3BB: return "pmpaddr11";
    case 0x3BC: return "pmpaddr12";
    case 0x3BD: return "pmpaddr13";
    case 0x3BE: return "pmpaddr14";
    case 0x3BF: return "pmpaddr15";

    // Machine timers and counters
    case 0xB00: return "mcycle";
    case 0xB02: return "minstret";
    case 0xB03: return "mhpmcounter3";
    case 0xB04: return "mhpmcounter4";
    case 0xB05: return "mhpmcounter5";
    case 0xB06: return "mhpmcounter6";
    case 0xB07: return "mhpmcounter7";
    case 0xB08: return "mhpmcounter8";
    case 0xB09: return "mhpmcounter9";
    case 0xB0A: return "mhpmcounter10";
    case 0xB0B: return "mhpmcounter11";
    case 0xB0C: return "mhpmcounter12";
    case 0xB0D: return "mhpmcounter13";
    case 0xB0E: return "mhpmcounter14";
    case 0xB0F: return "mhpmcounter15";
    case 0xB10: return "mhpmcounter16";
    case 0xB11: return "mhpmcounter17";
    case 0xB12: return "mhpmcounter18";
    case 0xB13: return "mhpmcounter19";
    case 0xB14: return "mhpmcounter20";
    case 0xB15: return "mhpmcounter21";
    case 0xB16: return "mhpmcounter22";
    case 0xB17: return "mhpmcounter23";
    case 0xB18: return "mhpmcounter24";
    case 0xB19: return "mhpmcounter25";
    case 0xB1A: return "mhpmcounter26";
    case 0xB1B: return "mhpmcounter27";
    case 0xB1C: return "mhpmcounter28";
    case 0xB1D: return "mhpmcounter29";
    case 0xB1E: return "mhpmcounter30";
    case 0xB1F: return "mhpmcounter31";
    case 0xB80: return "mcycleh";
    case 0xB82: return "minstreth";
    case 0xB83: return "mhpmcounter3";
    case 0xB84: return "mhpmcounter4";
    case 0xB85: return "mhpmcounter5";
    case 0xB86: return "mhpmcounter6";
    case 0xB87: return "mhpmcounter7";
    case 0xB88: return "mhpmcounter8";
    case 0xB89: return "mhpmcounter9";
    case 0xB8A: return "mhpmcounter10";
    case 0xB8B: return "mhpmcounter11";
    case 0xB8C: return "mhpmcounter12";
    case 0xB8D: return "mhpmcounter13";
    case 0xB8E: return "mhpmcounter14";
    case 0xB8F: return "mhpmcounter15";
    case 0xB90: return "mhpmcounter16";
    case 0xB91: return "mhpmcounter17";
    case 0xB92: return "mhpmcounter18";
    case 0xB93: return "mhpmcounter19";
    case 0xB94: return "mhpmcounter20";
    case 0xB95: return "mhpmcounter21";
    case 0xB96: return "mhpmcounter22";
    case 0xB97: return "mhpmcounter23";
    case 0xB98: return "mhpmcounter24";
    case 0xB99: return "mhpmcounter25";
    case 0xB9A: return "mhpmcounter26";
    case 0xB9B: return "mhpmcounter27";
    case 0xB9C: return "mhpmcounter28";
    case 0xB9D: return "mhpmcounter29";
    case 0xB9E: return "mhpmcounter30";
    case 0xB9F: return "mhpmcounter31";

    // Machine counter setup
    case 0x323: return "mhpmevent3";
    case 0x324: return "mhpmevent4";
    case 0x325: return "mhpmevent5";
    case 0x326: return "mhpmevent6";
    case 0x327: return "mhpmevent7";
    case 0x328: return "mhpmevent8";
    case 0x329: return "mhpmevent9";
    case 0x32A: return "mhpmevent10";
    case 0x32B: return "mhpmevent11";
    case 0x32C: return "mhpmevent12";
    case 0x32D: return "mhpmevent13";
    case 0x32E: return "mhpmevent14";
    case 0x32F: return "mhpmevent15";
    case 0x330: return "mhpmevent16";
    case 0x331: return "mhpmevent17";
    case 0x332: return "mhpmevent18";
    case 0x333: return "mhpmevent19";
    case 0x334: return "mhpmevent20";
    case 0x335: return "mhpmevent21";
    case 0x336: return "mhpmevent22";
    case 0x337: return "mhpmevent23";
    case 0x338: return "mhpmevent24";
    case 0x339: return "mhpmevent25";
    case 0x33A: return "mhpmevent26";
    case 0x33B: return "mhpmevent27";
    case 0x33C: return "mhpmevent28";
    case 0x33D: return "mhpmevent29";
    case 0x33E: return "mhpmevent30";
    case 0x33F: return "mhpmevent31";

    // Debug/Trace registers (shared with debug mode)
    case 0x7A0: return "tselect";
    case 0x7A1: return "tdata0";
    case 0x7A2: return "tdata1";
    case 0x7A3: return "tdata2";

    // Debug mode registers
    case 0x7B0: return "dcsr";
    case 0x7B1: return "depc";
    case 0x7B2: return "dscratch";

    // PULP extensions
    case 0x014: return "mhartid";

    case 0x7b3: return "scratch1";

#ifdef CSR_STACK_CONF
    case CSR_STACK_CONF:  return "stack_conf";
    case CSR_STACK_START: return "stack_start";
    case CSR_STACK_END:   return "stack_end";
#endif

  }

#if 0
#if defined(ISS_HAS_PERF_COUNTERS)
    if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR)
    {
      status = perfCounters_read(iss, reg, value);
    }
#endif

#if defined(CSR_HWLOOP0_START)
    else if (iss->cpu.pulpv2.hwloop)
    {
      if (reg >= CSR_HWLOOP0_START && reg <= CSR_HWLOOP1_COUNTER)
      {
        status = hwloop_read(iss, reg - CSR_HWLOOP0_START, value);
      }
    }
#endif
#endif

  return "csr";

}