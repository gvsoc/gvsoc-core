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

Csr::Csr(Iss &iss)
    : iss(iss)
{
    // Unprivileged Counter/Timers
    this->declare_csr(&this->cycle,  "cycle",   0xC00); this->cycle.write_illegal = true;
    this->cycle.register_callback(std::bind(&Csr::mcycle_access, this, std::placeholders::_1, std::placeholders::_2));
    this->declare_csr(&this->time,     "time",      0xC01);
    this->time.register_callback(std::bind(&Csr::time_access, this, std::placeholders::_1, std::placeholders::_2));
    this->declare_csr(&this->instret,  "instret",   0xC02);

    // Supervisor trap setup
    this->declare_csr(&this->sstatus,    "sstatus",    0x100);
    this->declare_csr(&this->sie,        "sie",        0x104);
    this->declare_csr(&this->stvec,      "stvec",      0x105);
    this->declare_csr(&this->scounteren, "scounteren", 0x106);

    // Supervisor trap handling
    this->declare_csr(&this->sscratch, "sscratch",  0x140);
    this->declare_csr(&this->sepc,     "sepc",      0x141);
    this->declare_csr(&this->scause,   "scause",    0x142);
    this->declare_csr(&this->stval,    "stval",    0x143);
    this->declare_csr(&this->sip,      "sip",    0x144);

    // Supervisor protection and translation
#if defined(CONFIG_GVSOC_ISS_MMU)
    this->declare_csr(&this->satp,     "satp",      0x180);
#endif

    // Machine trap setup
    this->declare_csr(&this->mstatus,    "mstatus",    0x300);
    this->declare_csr(&this->misa,       "misa",       0x301, 0, 0);
    this->declare_csr(&this->medeleg,    "medeleg",    0x302);
    this->declare_csr(&this->mideleg,    "mideleg",    0x303);
    this->declare_csr(&this->mie,        "mie",        0x304);
    this->declare_csr(&this->mtvec,      "mtvec",      0x305);
    this->declare_csr(&this->mcounteren, "mcounteren", 0x306);

    // Machine trap handling
    this->declare_csr(&this->mscratch, "mscratch", 0x340);
    this->declare_csr(&this->mepc,     "mepc",     0x341, 0, (iss_reg_t)~1ULL);
    this->declare_csr(&this->mcause,   "mcause",   0x342);
    this->declare_csr(&this->mtval,    "mtval",    0x343);
    this->declare_csr(&this->mip,      "mip",      0x344);

    // Debug/Trace registers
    this->declare_csr(&this->tselect,  "tselect",      0x7A0);
    this->declare_csr(&this->tdata1,   "tdata1",       0x7A1);
    this->declare_csr(&this->tdata2,   "tdata2",       0x7A2);
    this->declare_csr(&this->tdata3,   "tdata3",       0x7A3);

    // Machine Non-Maskable Interrupt Handling
    this->declare_csr(&this->mnscratch,   "mnscratch",    0x740);
    this->declare_csr(&this->mnepc,       "mnepc",        0x741);
    this->declare_csr(&this->mncause,     "mncause",      0x742);
    this->declare_csr(&this->mnstatus,    "mnstatus",     0x744);

    // Machine timers and counters
    this->declare_csr(&this->mcycle,   "mcycle",    0xB00);
    this->mcycle.register_callback(std::bind(&Csr::mcycle_access, this, std::placeholders::_1, std::placeholders::_2));
    // Machine counter / timers
    for (int i=0; i<29; i++)
    {
        this->declare_csr(&this->mhpmcounter[i], "mhpmcounter" + std::to_string(i+ 3), 0xB03 + i, 0, 0);
#if ISS_REG_WIDTH == 32
        this->declare_csr(&this->mhpmcounterh[i], "mhpmcounterh" + std::to_string(i+ 3), 0xB83 + i, 0, 0);
#endif
    }

    // Machine counter setup
    this->declare_csr(&this->mcountinhibit, "mcountinhibit", 0x320, 0, 0);


    // Machine information registers
    this->declare_csr(&this->mvendorid,  "mvendorid",  0xF11);
    this->declare_csr(&this->marchid,    "marchid",    0xF12);

    this->declare_csr(&this->vstart, "vstart", 0x008, 0);
    this->declare_csr(&this->vxstat, "vxstat", 0x009);
    this->declare_csr(&this->vxrm,   "vxrm",   0x00A);
    this->declare_csr(&this->vcsr,   "vcsr",   0x00F);
    this->declare_csr(&this->vl,     "vl",     0xC20);
    this->declare_csr(&this->vtype,  "vtype",  0xC21);
    this->declare_csr(&this->vlenb,  "vlenb",  0xC22, 32);
#if defined(CONFIG_GVSOC_ISS_PMP)
    // Machine protection and translation
    for (int i=0; i<16; i++)
    {
        this->declare_csr(&this->pmpcfg[i],  "pmpcfg" + std::to_string(i),  0x3A0 + i);
    }
    for (int i=0; i<64; i++)
    {
        this->declare_csr(&this->pmpaddr[i],  "pmpaddr" + std::to_string(i),  0x3B0 + i);
    }
#endif
}

void Csr::reset(bool active)
{
    if (active)
    {
#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
        memset(this->hwloop_regs, 0, sizeof(this->hwloop_regs));
#endif
    #if defined(ISS_HAS_PERF_COUNTERS)
        this->pcmr = 0;
        this->pcer = 3;
    #endif
    #if defined(CONFIG_GVSOC_ISS_STACK_CHECKER)
        this->stack_conf = 0;
    #endif
        this->dcsr = 4 << 28;
        this->fcsr.raw = 0;

        for (auto reg: this->regs)
        {
            if (reg.second)
            {
                reg.second->reset(active);
            }
        }

        this->misa.value = this->iss.top.get_js_config()->get_int("misa");

#if ISS_REG_WIDTH == 64
        this->misa.value |= 2ULL << 62;
#else
        this->misa.value |= 1 << 30;
#endif
    }

}


void Csr::declare_pcer(int index, std::string name, std::string help)
{
    this->iss.syscalls.pcer_info[index].name = strdup(name.c_str());
    this->iss.syscalls.pcer_info[index].help = help;
}

void Csr::build()
{
    iss.top.traces.new_trace("csr", &this->trace, vp::DEBUG);

    this->declare_pcer(CSR_PCER_CYCLES, "Cycles", "Count the number of cycles the core was running");
    this->declare_pcer(CSR_PCER_INSTR, "instr", "Count the number of instructions executed");
    this->declare_pcer(CSR_PCER_LD_STALL, "ld_stall", "Number of load use hazards");
    this->declare_pcer(CSR_PCER_JMP_STALL, "jmp_stall", "Number of jump register hazards");
    this->declare_pcer(CSR_PCER_IMISS, "imiss", "Cycles waiting for instruction fetches. i.e. the number of instructions wasted due to non-ideal caches");
    this->declare_pcer(CSR_PCER_LD, "ld", "Number of memory loads executed. Misaligned accesses are counted twice");
    this->declare_pcer(CSR_PCER_ST, "st", "Number of memory stores executed. Misaligned accesses are counted twice");
    this->declare_pcer(CSR_PCER_JUMP, "jump", "Number of jump instructions seen, i.e. j, jr, jal, jalr");
    this->declare_pcer(CSR_PCER_BRANCH, "branch", "Number of branch instructions seen, i.e. bf, bnf");
    this->declare_pcer(CSR_PCER_TAKEN_BRANCH, "taken_branch", "Number of taken branch instructions seen, i.e. bf, bnf");
    this->declare_pcer(CSR_PCER_RVC, "rvc", "Number of compressed instructions");
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
    this->declare_pcer(CSR_PCER_LD_EXT, "ld_ext", "Number of memory loads to EXT executed. Misaligned accesses are counted twice. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_ST_EXT, "st_ext", "Number of memory stores to EXT executed. Misaligned accesses are counted twice. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_LD_EXT_CYC, "ld_ext_cycles", "Cycles used for memory loads to EXT. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_ST_EXT_CYC, "st_ext_cycles", "Cycles used for memory stores to EXT. Every non-TCDM access is considered external");
    this->declare_pcer(CSR_PCER_TCDM_CONT, "tcdm_cont", "Cycles wasted due to TCDM/log-interconnect contention");
#endif

    this->iss.top.traces.new_trace_event("pcer_cycles", &this->iss.timing.pcer_trace_event[0], 1);
    this->iss.top.traces.new_trace_event("pcer_instr", &this->iss.timing.pcer_trace_event[1], 1);
    this->iss.top.traces.new_trace_event("pcer_ld_stall", &this->iss.timing.pcer_trace_event[2], 1);
    this->iss.top.traces.new_trace_event("pcer_jmp_stall", &this->iss.timing.pcer_trace_event[3], 1);
    this->iss.top.traces.new_trace_event("pcer_imiss", &this->iss.timing.pcer_trace_event[4], 1);
    this->iss.top.traces.new_trace_event("pcer_ld", &this->iss.timing.pcer_trace_event[5], 1);
    this->iss.top.traces.new_trace_event("pcer_st", &this->iss.timing.pcer_trace_event[6], 1);
    this->iss.top.traces.new_trace_event("pcer_jump", &this->iss.timing.pcer_trace_event[7], 1);
    this->iss.top.traces.new_trace_event("pcer_branch", &this->iss.timing.pcer_trace_event[8], 1);
    this->iss.top.traces.new_trace_event("pcer_taken_branch", &this->iss.timing.pcer_trace_event[9], 1);
    this->iss.top.traces.new_trace_event("pcer_rvc", &this->iss.timing.pcer_trace_event[10], 1);
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
    this->iss.top.traces.new_trace_event("pcer_ld_ext", &this->iss.timing.pcer_trace_event[11], 1);
    this->iss.top.traces.new_trace_event("pcer_st_ext", &this->iss.timing.pcer_trace_event[12], 1);
    this->iss.top.traces.new_trace_event("pcer_ld_ext_cycles", &this->iss.timing.pcer_trace_event[13], 1);
    this->iss.top.traces.new_trace_event("pcer_st_ext_cycles", &this->iss.timing.pcer_trace_event[14], 1);
    this->iss.top.traces.new_trace_event("pcer_tcdm_cont", &this->iss.timing.pcer_trace_event[15], 1);
#endif

    this->iss.top.new_master_port("time", &this->time_itf, (vp::Block *)this);

    this->mhartid = (this->iss.top.get_js_config()->get_child_int("cluster_id") << 5) | this->iss.top.get_js_config()->get_child_int("core_id");

    this->tselect.register_callback(std::bind(&Csr::tselect_access, this, std::placeholders::_1, std::placeholders::_2));

#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
    this->hwloop_regs[PULPV2_HWLOOP_LPCOUNT(0)] = 0;
    this->hwloop_regs[PULPV2_HWLOOP_LPCOUNT(1)] = 0;
#endif
}

bool Csr::tselect_access(bool is_write, iss_reg_t &value)
{
    if (!is_write)
    {
        value = -1;
    }
    return false;
}

bool Csr::time_access(bool is_write, iss_reg_t &value)
{
    uint64_t time;
    this->time_itf.sync_back(&time);
    value = time;
    return false;
}

bool Csr::mcycle_access(bool is_write, iss_reg_t &value)
{
    if (!is_write)
    {
        value = this->iss.top.clock.get_cycles();
    }
    return false;
}


#if defined(ISS_HAS_PERF_COUNTERS)
void check_perf_config_change(Iss *iss, unsigned int pcer, unsigned int pcmr);
#endif

/*
 *   USER CSRS
 */

static bool ustatus_read(Iss *iss, iss_reg_t *value)
{
    *value = 0;
    return false;
}

static bool ustatus_write(Iss *iss, unsigned int value)
{
    return false;
}

static bool uie_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: uie\n");
    return false;
}

static bool uie_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: uie\n");
    return false;
}

static bool utvec_read(Iss *iss, iss_reg_t *value)
{
    *value = 0;
    return false;
}

static bool utvec_write(Iss *iss, unsigned int value)
{
    return false;
}

static bool uscratch_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: uscratch\n");
    return false;
}

static bool uscratch_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: uscratch\n");
    return false;
}

static bool uepc_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: uepc\n");
    return false;
}

static bool uepc_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: uepc\n");
    return false;
}

static bool ucause_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: ucause\n");
    return false;
}

static bool ucause_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: ucause\n");
    return false;
}

static bool ubadaddr_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: ubadaddr\n");
    return false;
}

static bool ubadaddr_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: ubadaddr\n");
    return false;
}

static bool uip_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR READ: uip\n");
    return false;
}

static bool uip_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR WRITE: uip\n");
    return false;
}

static bool fflags_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.fcsr.fflags;
    return false;
}

static bool fflags_write(Iss *iss, unsigned int value)
{
    iss->csr.fcsr.fflags = value;
    return false;
}

static bool frm_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.fcsr.frm;
    return false;
}

static bool frm_write(Iss *iss, unsigned int value)
{
    iss->csr.fcsr.frm = value;
    return false;
}

static bool fcsr_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.fcsr.raw;
    return false;
}

static bool fcsr_write(Iss *iss, unsigned int value)
{
    iss->csr.fcsr.raw = value & 0xff;
    return false;
}

static bool hpmcounter_read(Iss *iss, iss_reg_t *value, int id)
{
    printf("WARNING UNIMPLEMENTED CSR: hpmcounter\n");
    return false;
}

static bool cycleh_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: cycleh\n");
    return false;
}

static bool timeh_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: timeh\n");
    return false;
}

static bool instreth_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: instreth\n");
    return false;
}

static bool hpmcounterh_read(Iss *iss, iss_reg_t *value, int id)
{
    printf("WARNING UNIMPLEMENTED CSR: hpmcounterh\n");
    return false;
}

/*
 *   SUPERVISOR CSRS
 */

static bool scounteren_read(Iss *iss, iss_reg_t *value)
{
    //*value = iss->tvec[GVSIM_MODE_SUPERVISOR];
    return false;
}

static bool scounteren_write(Iss *iss, unsigned int value)
{
    // iss->tvec[GVSIM_MODE_SUPERVISOR] = value;
    return false;
}

/*
 *   MACHINE CSRS
 */

static bool mimpid_read(Iss *iss, iss_reg_t *value)
{
    *value = 0;
    return false;
}

static bool mhartid_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.mhartid;
    return false;
}

static bool mcounteren_read(Iss *iss, iss_reg_t *value)
{
    return false;
}

static bool mcounteren_write(Iss *iss, unsigned int value)
{
    return false;
}


static bool mbadaddr_read(Iss *iss, iss_reg_t *value)
{
    //*value = iss->badaddr[GVSIM_MODE_MACHINE];
    return false;
}

static bool mbadaddr_write(Iss *iss, unsigned int value)
{
    // iss->badaddr[GVSIM_MODE_MACHINE] = value;
    return false;
}

static bool minstret_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: minstret\n");
    return false;
}

static bool minstret_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: minstret\n");
    return false;
}

static bool mcycleh_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: mcycleh\n");
    return false;
}

static bool mcycleh_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: mcycleh\n");
    return false;
}

static bool minstreth_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: \n");
    return false;
}

static bool minstreth_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: \n");
    return false;
}

static bool mucounteren_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: mucounteren\n");
    return false;
}

static bool mucounteren_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: mucounteren\n");
    return false;
}

static bool mscounteren_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: mscounteren\n");
    return false;
}

static bool mscounteren_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: mscounteren\n");
    return false;
}

static bool mhcounteren_read(Iss *iss, iss_reg_t *value)
{
    printf("WARNING UNIMPLEMENTED CSR: mhcounteren\n");
    return false;
}

static bool mhcounteren_write(Iss *iss, unsigned int value)
{
    printf("WARNING UNIMPLEMENTED CSR: mhcounteren\n");
    return false;
}

static bool mhpmevent_read(Iss *iss, iss_reg_t *value, int id)
{
    printf("WARNING UNIMPLEMENTED CSR: mhpmevent\n");
    return false;
}

static bool mhpmevent_write(Iss *iss, unsigned int value, int id)
{
    printf("WARNING UNIMPLEMENTED CSR: mhpmevent\n");
    return false;
}

/*
 *   PULP CSRS
 */

#ifdef CONFIG_GVSOC_ISS_STACK_CHECKER

static bool stack_conf_write(Iss *iss, iss_reg_t value)
{
    iss->csr.stack_conf = value;

    if (iss->csr.stack_conf)
        iss->csr.trace.msg("Activating stack checking (start: 0x%x, end: 0x%x)\n", iss->csr.stack_start, iss->csr.stack_end);
    else
        iss->csr.trace.msg("Deactivating stack checking\n");

    return false;
}

static bool stack_conf_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.stack_conf;
    return false;
}

static bool stack_start_write(Iss *iss, iss_reg_t value)
{
    iss->csr.stack_start = value;
    return false;
}

static bool stack_start_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.stack_start;
    return false;
}

static bool stack_end_write(Iss *iss, iss_reg_t value)
{
    iss->csr.stack_end = value;
    return false;
}

static bool stack_end_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.stack_end;
    return false;
}

#endif

static bool umode_read(Iss *iss, iss_reg_t *value)
{
    *value = 3;
    //*value = iss->state.mode;
    return false;
}

static bool dcsr_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.dcsr;
    return false;
}

static bool dcsr_write(Iss *iss, iss_reg_t value)
{
    iss->csr.dcsr = value;
    iss->exec.step_mode.set((value >> 2) & 1);
    return false;
}

static bool depc_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.depc;
    return false;
}

static bool depc_write(Iss *iss, iss_reg_t value)
{
    iss->csr.depc = value;
    return false;
}

static bool dscratch_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.scratch0;
    return false;
}

static bool dscratch_write(Iss *iss, unsigned int value)
{
    iss->csr.scratch0 = value;
    return false;
}

static bool scratch0_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.scratch0;
    return false;
}

static bool scratch0_write(Iss *iss, iss_reg_t value)
{
    iss->csr.scratch0 = value;
    return false;
}

static bool scratch1_read(Iss *iss, iss_reg_t *value)
{
    *value = iss->csr.scratch1;
    return false;
}

static bool scratch1_write(Iss *iss, iss_reg_t value)
{
    iss->csr.scratch1 = value;
    return false;
}

static bool pcer_write(Iss *iss, unsigned int prev_val, unsigned int value)
{
    iss->csr.pcer = value & 0x7fffffff;
    check_perf_config_change(iss, prev_val, iss->csr.pcmr);
    return false;
}

static bool pcmr_write(Iss *iss, unsigned int prev_val, unsigned int value)
{
    iss->csr.pcmr = value;

    check_perf_config_change(iss, iss->csr.pcer, prev_val);
    return false;
}

#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
static bool hwloop_read(Iss *iss, int reg, iss_reg_t *value)
{
    *value = iss->csr.hwloop_regs[reg];
    return false;
}

static bool hwloop_write(Iss *iss, int reg, unsigned int value)
{
    iss->csr.hwloop_regs[reg] = value;

    // Since the HW loop is using decode instruction for the HW loop start to jump faster
    // we need to recompute it when it is modified.
    if (reg == 0)
    {
        iss->exec.hwloop_start_insn[0] = value;
    }
    else if (reg == 4)
    {
        iss->exec.hwloop_start_insn[1] = value;
    }

    return false;
}
#endif

#if defined(ISS_HAS_PERF_COUNTERS)

#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
static inline void iss_csr_ext_counter_set(Iss *iss, int id, unsigned int value)
{
    if (iss->timing.ext_counter[id].is_bound())
    {
        iss->timing.ext_counter[id].sync(value);
    }
}

static inline void iss_csr_ext_counter_get(Iss *iss, int id, unsigned int *value)
{
    if (iss->timing.ext_counter[id].is_bound())
    {
        iss->timing.ext_counter[id].sync_back(value);
    }
}

void update_external_pccr(Iss *iss, int id, unsigned int pcer, unsigned int pcmr)
{
    unsigned int incr = 0;
    // Only update if the counter is active as the external signal may report
    // a different value whereas the counter must remain the same
    if (((pcer & CSR_PCER_EVENT_MASK(id)) && (pcmr & CSR_PCMR_ACTIVE)) || iss->timing.event_trace_is_active(id))
    {
        iss_csr_ext_counter_get(iss, id, &incr);
        iss->csr.pccr[id] += incr;
        iss->timing.event_trace_account(id, incr);
    }
    else
    {
        // Nothing to do if the counter is inactive, it will get reset so that
        // we get read events from now if it becomes active
    }

    // Reset the counter
    if (iss->timing.ext_counter[id].is_bound())
        iss_csr_ext_counter_set(iss, id, 0);

    // if (cpu->traceEvent) sim_trace_event_incr(cpu, id, incr);
}

void flushExternalCounters(Iss *iss)
{
    int i;
    for (int i = CSR_PCER_FIRST_EXTERNAL_EVENTS; i < CSR_PCER_FIRST_EXTERNAL_EVENTS + CSR_PCER_NB_EXTERNAL_EVENTS; i++)
    {
        update_external_pccr(iss, i, iss->csr.pcer, iss->csr.pcmr);
    }
}

#endif

void check_perf_config_change(Iss *iss, unsigned int pcer, unsigned int pcmr)
{
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
    // In case PCER or PCMR is modified, there is a special care about external signals as they
    // are still counting whatever the event active flag is. Reset them to start again from a
    // clean state
    {
        int i;
        // Check every external signal separatly
        for (int i = CSR_PCER_FIRST_EXTERNAL_EVENTS; i < CSR_PCER_FIRST_EXTERNAL_EVENTS + CSR_PCER_NB_EXTERNAL_EVENTS; i++)
        {
            // This will update our internal counter in case it is active or just reset it
            update_external_pccr(iss, i, pcer, pcmr);
        }
    }
#endif
}

static bool perfCounters_read(Iss *iss, int reg, iss_reg_t *value)
{
    if (reg == CSR_PCER)
    {
        *value = iss->csr.pcer;
    }
    else if (reg == CSR_PCMR)
    {
        *value = iss->csr.pcmr;
    }
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
    // In case of counters connected to external signals, we need to synchronize first
    if (reg >= CSR_PCCR(CSR_PCER_FIRST_EXTERNAL_EVENTS) && reg < CSR_PCCR(CSR_PCER_FIRST_EXTERNAL_EVENTS + CSR_PCER_NB_EXTERNAL_EVENTS))
    {
        update_external_pccr(iss, reg - CSR_PCCR(0), iss->csr.pcer, iss->csr.pcmr);
        *value = iss->csr.pccr[reg - CSR_PCCR(0)];
        iss->csr.trace.msg("Reading PCCR (index: %d, value: 0x%x)\n", reg - CSR_PCCR(0), *value);
    }
#endif
    else
    {
        *value = iss->csr.pccr[reg - CSR_PCCR(0)];
        iss->csr.trace.msg("Reading PCCR (index: %d, value: 0x%x)\n", reg - CSR_PCCR(0), *value);
    }

    return false;
}

static bool perfCounters_write(Iss *iss, int reg, unsigned int value)
{
    if (reg == CSR_PCER)
    {
        iss->csr.trace.msg("Setting PCER (value: 0x%x)\n", value);
        return pcer_write(iss, iss->csr.pcer, value);
    }
    else if (reg == CSR_PCMR)
    {
        iss->csr.trace.msg("Setting PCMR (value: 0x%x)\n", value);
        return pcmr_write(iss, iss->csr.pcmr, value);
    }
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
    // In case of counters connected to external signals, we need to synchronize the external one
    // with our
    if (reg >= CSR_PCCR(CSR_PCER_FIRST_EXTERNAL_EVENTS) && reg < CSR_PCCR(CSR_PCER_FIRST_EXTERNAL_EVENTS + CSR_PCER_NB_EXTERNAL_EVENTS))
    {
        // This will update out counter, which will be overwritten afterwards by the new value and
        // also set the external counter to 0 which makes sure they are synchroninez
        update_external_pccr(iss, reg - CSR_PCCR(0), iss->csr.pcer, iss->csr.pcmr);
    }
#endif
    else if (reg == CSR_PCCR(CSR_NB_PCCR))
    {
        iss->csr.trace.msg("Setting value to all PCCR (value: 0x%x)\n", value);

        int i;
        for (i = 0; i < 31; i++)
        {
            iss->csr.pccr[i] = value;
#if defined(CONFIG_GVSOC_ISS_EXTERNAL_PCCR)
            if (i >= CSR_PCER_FIRST_EXTERNAL_EVENTS && i < CSR_PCER_FIRST_EXTERNAL_EVENTS + CSR_PCER_NB_EXTERNAL_EVENTS)
            {
                update_external_pccr(iss, i, 0, 0);
            }
#endif
        }
    }
    else
    {
        iss->csr.trace.msg("Setting PCCR value (pccr: %d, value: 0x%x)\n", reg - CSR_PCCR(0), value);
        iss->csr.pccr[reg - CSR_PCCR(0)] = value;
    }
    return false;
}
#endif

static bool checkCsrAccess(Iss *iss, int reg, bool isWrite)
{
    // bool isRw = (reg >> 10) & 0x3;
    // bool priv = (reg >> 8) & 0x3;
    // if ((isWrite && !isRw) || (priv > iss->state.mode)) {
    //   triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
    //   return true;
    // }
    return false;
}

bool iss_csr_read(Iss *iss, iss_reg_t reg, iss_reg_t *value)
{
    bool status = true;

    iss->csr.trace.msg("Reading CSR (reg: 0x%x, name: %s)\n",
        reg, iss_csr_name(iss, reg));

#if 0
  // First check permissions
  if (checkCsrAccess(iss, reg, 0)) return true;


  if (!getOption(iss, __priv_pulp)) {
    if (reg >= 0xC03 && reg <= 0xC1F) return hpmcounter_read(iss, reg - 0xC03, value);
    if (reg >= 0xC83 && reg <= 0xC9F) return hpmcounterh_read(iss, reg - 0xC83, value);

    if (reg >= 0x323 && reg <= 0x33F) return mhpmevent_read(iss, reg - 0x323, value);
  }
#endif

    // New generic way of handling CSR access, all CSR should be accessed there
    if (!iss->csr.access(false, reg, *value))
    {
        return false;
    }

    // And dispatch
    switch (reg)
    {

    // User trap setup
    case 0x000:
        status = ustatus_read(iss, value);
        break;
    case 0x004:
        status = uie_read(iss, value);
        break;
    case 0x005:
        status = utvec_read(iss, value);
        break;

    // User trap handling
    case 0x040:
        status = uscratch_read(iss, value);
        break;
    case 0x041:
        status = uepc_read(iss, value);
        break;
    case 0x042:
        status = ucause_read(iss, value);
        break;
    case 0x043:
        status = ubadaddr_read(iss, value);
        break;
    case 0x044:
        status = uip_read(iss, value);
        break;

    // User floating-point CSRs
    case 0x001:
        status = fflags_read(iss, value);
        break;
    case 0x002:
        status = frm_read(iss, value);
        break;
    case 0x003:
        status = fcsr_read(iss, value);
        break;

    // User counter / timers
    case 0xC10:
        status = umode_read(iss, value);
        break;
    // case 0xC10: status = hpmcounter_read (iss, value, 16); break;
    case 0xC11:
        status = hpmcounter_read(iss, value, 17);
        break;
    case 0xC12:
        status = hpmcounter_read(iss, value, 18);
        break;
    case 0xC13:
        status = hpmcounter_read(iss, value, 19);
        break;
    case 0xC14:
        status = hpmcounter_read(iss, value, 20);
        break;
    case 0xC15:
        status = hpmcounter_read(iss, value, 21);
        break;
    case 0xC16:
        status = hpmcounter_read(iss, value, 22);
        break;
    case 0xC17:
        status = hpmcounter_read(iss, value, 23);
        break;
    case 0xC18:
        status = hpmcounter_read(iss, value, 24);
        break;
    case 0xC19:
        status = hpmcounter_read(iss, value, 25);
        break;
    case 0xC1A:
        status = hpmcounter_read(iss, value, 26);
        break;
    case 0xC1B:
        status = hpmcounter_read(iss, value, 27);
        break;
    case 0xC1C:
        status = hpmcounter_read(iss, value, 28);
        break;
    case 0xC1D:
        status = hpmcounter_read(iss, value, 29);
        break;
    case 0xC1E:
        status = hpmcounter_read(iss, value, 30);
        break;
    case 0xC1F:
        status = hpmcounter_read(iss, value, 31);
        break;
    case 0xC80:
        status = cycleh_read(iss, value);
        break;
    case 0xC82:
        status = instreth_read(iss, value);
        break;
    case 0xC83:
        status = hpmcounterh_read(iss, value, 3);
        break;
    case 0xC84:
        status = hpmcounterh_read(iss, value, 4);
        break;
    case 0xC85:
        status = hpmcounterh_read(iss, value, 5);
        break;
    case 0xC86:
        status = hpmcounterh_read(iss, value, 6);
        break;
    case 0xC87:
        status = hpmcounterh_read(iss, value, 7);
        break;
    case 0xC88:
        status = hpmcounterh_read(iss, value, 8);
        break;
    case 0xC89:
        status = hpmcounterh_read(iss, value, 9);
        break;
    case 0xC8A:
        status = hpmcounterh_read(iss, value, 10);
        break;
    case 0xC8B:
        status = hpmcounterh_read(iss, value, 11);
        break;
    case 0xC8C:
        status = hpmcounterh_read(iss, value, 12);
        break;
    case 0xC8D:
        status = hpmcounterh_read(iss, value, 13);
        break;
    case 0xC8E:
        status = hpmcounterh_read(iss, value, 14);
        break;
    case 0xC8F:
        status = hpmcounterh_read(iss, value, 15);
        break;
    case 0xC90:
        status = hpmcounterh_read(iss, value, 16);
        break;
    case 0xC91:
        status = hpmcounterh_read(iss, value, 17);
        break;
    case 0xC92:
        status = hpmcounterh_read(iss, value, 18);
        break;
    case 0xC93:
        status = hpmcounterh_read(iss, value, 19);
        break;
    case 0xC94:
        status = hpmcounterh_read(iss, value, 20);
        break;
    case 0xC95:
        status = hpmcounterh_read(iss, value, 21);
        break;
    case 0xC96:
        status = hpmcounterh_read(iss, value, 22);
        break;
    case 0xC97:
        status = hpmcounterh_read(iss, value, 23);
        break;
    case 0xC98:
        status = hpmcounterh_read(iss, value, 24);
        break;
    case 0xC99:
        status = hpmcounterh_read(iss, value, 25);
        break;
    case 0xC9A:
        status = hpmcounterh_read(iss, value, 26);
        break;
    case 0xC9B:
        status = hpmcounterh_read(iss, value, 27);
        break;
    case 0xC9C:
        status = hpmcounterh_read(iss, value, 28);
        break;
    case 0xC9D:
        status = hpmcounterh_read(iss, value, 29);
        break;
    case 0xC9E:
        status = hpmcounterh_read(iss, value, 30);
        break;
    case 0xC9F:
        status = hpmcounterh_read(iss, value, 31);
        break;

    case 0x106:
        status = scounteren_read(iss, value);
        break;

    // Machine information registers
    case 0xF13:
        status = mimpid_read(iss, value);
        break;
    case 0xF14:
        status = mhartid_read(iss, value);
        break;

    case 0x306:
        status = mcounteren_read(iss, value);
        break;


    // Machine timers and counters
    case 0xB02:
        status = minstret_read(iss, value);
        break;
    case 0xB80:
        status = mcycleh_read(iss, value);
        break;
    case 0xB82:
        status = minstreth_read(iss, value);
        break;
    // Machine counter setup
    case 0x323:
        status = mhpmevent_read(iss, value, 3);
        break;
    case 0x324:
        status = mhpmevent_read(iss, value, 4);
        break;
    case 0x325:
        status = mhpmevent_read(iss, value, 5);
        break;
    case 0x326:
        status = mhpmevent_read(iss, value, 6);
        break;
    case 0x327:
        status = mhpmevent_read(iss, value, 7);
        break;
    case 0x328:
        status = mhpmevent_read(iss, value, 8);
        break;
    case 0x329:
        status = mhpmevent_read(iss, value, 9);
        break;
    case 0x32A:
        status = mhpmevent_read(iss, value, 10);
        break;
    case 0x32B:
        status = mhpmevent_read(iss, value, 11);
        break;
    case 0x32C:
        status = mhpmevent_read(iss, value, 12);
        break;
    case 0x32D:
        status = mhpmevent_read(iss, value, 13);
        break;
    case 0x32E:
        status = mhpmevent_read(iss, value, 14);
        break;
    case 0x32F:
        status = mhpmevent_read(iss, value, 15);
        break;
    case 0x330:
        status = mhpmevent_read(iss, value, 16);
        break;
    case 0x331:
        status = mhpmevent_read(iss, value, 17);
        break;
    case 0x332:
        status = mhpmevent_read(iss, value, 18);
        break;
    case 0x333:
        status = mhpmevent_read(iss, value, 19);
        break;
    case 0x334:
        status = mhpmevent_read(iss, value, 20);
        break;
    case 0x335:
        status = mhpmevent_read(iss, value, 21);
        break;
    case 0x336:
        status = mhpmevent_read(iss, value, 22);
        break;
    case 0x337:
        status = mhpmevent_read(iss, value, 23);
        break;
    case 0x338:
        status = mhpmevent_read(iss, value, 24);
        break;
    case 0x339:
        status = mhpmevent_read(iss, value, 25);
        break;
    case 0x33A:
        status = mhpmevent_read(iss, value, 26);
        break;
    case 0x33B:
        status = mhpmevent_read(iss, value, 27);
        break;
    case 0x33C:
        status = mhpmevent_read(iss, value, 28);
        break;
    case 0x33D:
        status = mhpmevent_read(iss, value, 29);
        break;
    case 0x33E:
        status = mhpmevent_read(iss, value, 30);
        break;
    case 0x33F:
        status = mhpmevent_read(iss, value, 31);
        break;

    // Debug mode registers
    case 0x7B0:
        status = dcsr_read(iss, value);
        break;
    case 0x7B1:
        status = depc_read(iss, value);
        break;
    case 0x7B2:
        status = dscratch_read(iss, value);
        break;

    // PULP extensions
    case 0x014:
        status = mhartid_read(iss, value);
        break;

#if CONFIG_GVSOC_ISS_RI5KY != 0x7b0
    case 0x7b3:
        status = scratch1_read(iss, value);
        break;
#endif

#ifdef CONFIG_GVSOC_ISS_STACK_CHECKER
    case CSR_STACK_CONF:
        status = stack_conf_read(iss, value);
        break;
    case CSR_STACK_START:
        status = stack_start_read(iss, value);
        break;
    case CSR_STACK_END:
        status = stack_end_read(iss, value);
        break;
#endif

#if defined(CONFIG_GVSOC_ISS_SNITCH)
    case 0x7d0:
    case 0x7d1:
    case 0x7d2:
        status = false;
        break;
#endif

    default:

#if defined(ISS_HAS_PERF_COUNTERS)
        if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR)
        {
            status = perfCounters_read(iss, reg, value);
        }
#endif

#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
        if (reg >= CSR_HWLOOP0_START && reg <= CSR_HWLOOP1_COUNTER)
        {
            status = hwloop_read(iss, reg - CSR_HWLOOP0_START, value);
        }
#endif

        if (status)
        {
            iss->csr.trace.force_warning("Accessing unsupported CSR (id: 0x%x, name: %s)\n", reg, iss_csr_name(iss, reg));
#if 0
      triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
#endif
            return true;
        }
    }

    iss->csr.trace.msg("Read CSR (reg: 0x%x, value: 0x%x)\n", reg, value);

    return status;
}

bool iss_csr_write(Iss *iss, iss_reg_t reg, iss_reg_t value)
{
    iss->csr.trace.msg("Writing CSR (reg: 0x%x, name: %s, value: 0x%x)\n",
        reg, iss_csr_name(iss, reg), value);

    // If there is any write to a CSR, switch to full check instruction handler
    // in case something special happened (like HW counting become active)
    iss->exec.switch_to_full_mode();

#if 0
  // First check permissions
  if (checkCsrAccess(iss, reg, 0)) return true;

  if (!getOption(iss, __priv_pulp)) {
    if (reg >= 0x323 && reg <= 0x33F) return mhpmevent_write(iss, reg - 0x323, value);
  }
#endif

    // New generic way of handling CSR access, all CSR should be accessed there
    if (!iss->csr.access(true, reg, value))
    {
        return false;
    }

    // And dispatch
    switch (reg)
    {

    // User trap setup
    case 0x000:
        return ustatus_write(iss, value);
    case 0x004:
        return uie_write(iss, value);
    case 0x005:
        return utvec_write(iss, value);

    // User trap handling
    case 0x040:
        return uscratch_write(iss, value);
    case 0x041:
        return uepc_write(iss, value);
    case 0x042:
        return ucause_write(iss, value);
    case 0x043:
        return ubadaddr_write(iss, value);
    case 0x044:
        return uip_write(iss, value);

    // User floating-point CSRs
    case 0x001:
        return fflags_write(iss, value);
    case 0x002:
        return frm_write(iss, value);
    case 0x003:
        return fcsr_write(iss, value);

    case 0x343:
        return mbadaddr_write(iss, value);

    // Machine timers and counters
    case 0xB02:
        return minstret_write(iss, value);
    case 0xB82:
        return minstreth_write(iss, value);

    // Machine counter setup
    case 0x310:
        return mucounteren_write(iss, value);
    case 0x311:
        return mscounteren_write(iss, value);
    case 0x312:
        return mhcounteren_write(iss, value);

#if CONFIG_GVSOC_ISS_RI5KY != 0x7b0
    case 0x7b0:
        return dcsr_write(iss, value);
    case 0x7b1:
        return depc_write(iss, value);
    case 0x7b2:
        return scratch0_write(iss, value);
    case 0x7b3:
        return scratch1_write(iss, value);
#endif

    case 0xF13:
    case 0xF14:
        return false;
#ifdef CONFIG_GVSOC_ISS_STACK_CHECKER
    case CSR_STACK_CONF:
        return stack_conf_write(iss, value);
        break;
    case CSR_STACK_START:
        return stack_start_write(iss, value);
        break;
    case CSR_STACK_END:
        return stack_end_write(iss, value);
        break;
#else
    case 0x7d0:
    case 0x7d1:
    case 0x7d2:
        return false;
        break;
#endif
    }

#if defined(ISS_HAS_PERF_COUNTERS)
    if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR)
        return perfCounters_write(iss, reg, value);
#endif

#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
    if (reg >= CSR_HWLOOP0_START && reg <= CSR_HWLOOP1_COUNTER)
        return hwloop_write(iss, reg - CSR_HWLOOP0_START, value);
#endif

    iss->csr.trace.force_warning("Accessing unsupported CSR (id: 0x%x, name: %s)\n", reg, iss_csr_name(iss, reg));
#if 0
  triggerException_cause(iss, iss->currentPc, EXCEPTION_ILLEGAL_INSTR, ECAUSE_ILL_INSTR);
#endif

    return true;
}

const char *iss_csr_name(Iss *iss, iss_reg_t reg)
{
    CsrAbtractReg *csr = iss->csr.get_csr(reg);
    if (csr != NULL)
    {
        return csr->name;
    }

    switch (reg)
    {

    // User trap setup
    case 0x000:
        return "ustatus";
    case 0x004:
        return "uie";
    case 0x005:
        return "utvec";

    // User trap handling
    case 0x040:
        return "uscratch";
    case 0x041:
        return "uepc";
    case 0x042:
        return "ucause";
    case 0x043:
        return "ubadaddr";
    case 0x044:
        return "uip";

    // User floating-point CSRs
    case 0x001:
        return "fflags";
    case 0x002:
        return "frm";
    case 0x003:
        return "fcsr";

    // User counter / timers
    case 0xC01:
        return "time";
    case 0xC03:
        return "hpmcounter";
    case 0xC04:
        return "hpmcounter";
    case 0xC05:
        return "hpmcounter";
    case 0xC06:
        return "hpmcounter";
    case 0xC07:
        return "hpmcounter";
    case 0xC08:
        return "hpmcounter";
    case 0xC09:
        return "hpmcounter";
    case 0xC0A:
        return "hpmcounter";
    case 0xC0B:
        return "hpmcounter";
    case 0xC0C:
        return "hpmcounter";
    case 0xC0D:
        return "hpmcounter";
    case 0xC0E:
        return "hpmcounter";
    case 0xC0F:
        return "hpmcounter";
    case 0xC10:
        return "hpmcounter";
    case 0xC11:
        return "hpmcounter";
    case 0xC12:
        return "hpmcounter";
    case 0xC13:
        return "hpmcounter";
    case 0xC14:
        return "hpmcounter";
    case 0xC15:
        return "hpmcounter";
    case 0xC16:
        return "hpmcounter";
    case 0xC17:
        return "hpmcounter";
    case 0xC18:
        return "hpmcounter";
    case 0xC19:
        return "hpmcounter";
    case 0xC1A:
        return "hpmcounter";
    case 0xC1B:
        return "hpmcounter";
    case 0xC1C:
        return "hpmcounter";
    case 0xC1D:
        return "hpmcounter";
    case 0xC1E:
        return "hpmcounter";
    case 0xC1F:
        return "hpmcounter";
    case 0xC80:
        return "cycleh";
    case 0xC81:
        return "timeh";
    case 0xC82:
        return "instreth";
    case 0xC83:
        return "hpmcounterh3";
    case 0xC84:
        return "hpmcounterh4";
    case 0xC85:
        return "hpmcounterh5";
    case 0xC86:
        return "hpmcounterh6";
    case 0xC87:
        return "hpmcounterh7";
    case 0xC88:
        return "hpmcounterh8";
    case 0xC89:
        return "hpmcounterh9";
    case 0xC8A:
        return "hpmcounterh10";
    case 0xC8B:
        return "hpmcounterh11";
    case 0xC8C:
        return "hpmcounterh12";
    case 0xC8D:
        return "hpmcounterh13";
    case 0xC8E:
        return "hpmcounterh14";
    case 0xC8F:
        return "hpmcounterh15";
    case 0xC90:
        return "hpmcounterh16";
    case 0xC91:
        return "hpmcounterh17";
    case 0xC92:
        return "hpmcounterh18";
    case 0xC93:
        return "hpmcounterh19";
    case 0xC94:
        return "hpmcounterh20";
    case 0xC95:
        return "hpmcounterh21";
    case 0xC96:
        return "hpmcounterh22";
    case 0xC97:
        return "hpmcounterh23";
    case 0xC98:
        return "hpmcounterh24";
    case 0xC99:
        return "hpmcounterh25";
    case 0xC9A:
        return "hpmcounterh26";
    case 0xC9B:
        return "hpmcounterh27";
    case 0xC9C:
        return "hpmcounterh28";
    case 0xC9D:
        return "hpmcounterh29";
    case 0xC9E:
        return "hpmcounterh30";
    case 0xC9F:
        return "hpmcounterh31";

    case 0x106:
        return "scounteren";

    // Machine information registers
    case 0xF13:
        return "mimpid";
    case 0xF14:
        return "mhartid";

    case 0x306:
        return "mcounteren";

    // Machine timers and counters
    case 0xB02:
        return "minstret";
    case 0xB80:
        return "mcycleh";
    case 0xB82:
        return "minstreth";

    // Machine counter setup
    case 0x323:
        return "mhpmevent3";
    case 0x324:
        return "mhpmevent4";
    case 0x325:
        return "mhpmevent5";
    case 0x326:
        return "mhpmevent6";
    case 0x327:
        return "mhpmevent7";
    case 0x328:
        return "mhpmevent8";
    case 0x329:
        return "mhpmevent9";
    case 0x32A:
        return "mhpmevent10";
    case 0x32B:
        return "mhpmevent11";
    case 0x32C:
        return "mhpmevent12";
    case 0x32D:
        return "mhpmevent13";
    case 0x32E:
        return "mhpmevent14";
    case 0x32F:
        return "mhpmevent15";
    case 0x330:
        return "mhpmevent16";
    case 0x331:
        return "mhpmevent17";
    case 0x332:
        return "mhpmevent18";
    case 0x333:
        return "mhpmevent19";
    case 0x334:
        return "mhpmevent20";
    case 0x335:
        return "mhpmevent21";
    case 0x336:
        return "mhpmevent22";
    case 0x337:
        return "mhpmevent23";
    case 0x338:
        return "mhpmevent24";
    case 0x339:
        return "mhpmevent25";
    case 0x33A:
        return "mhpmevent26";
    case 0x33B:
        return "mhpmevent27";
    case 0x33C:
        return "mhpmevent28";
    case 0x33D:
        return "mhpmevent29";
    case 0x33E:
        return "mhpmevent30";
    case 0x33F:
        return "mhpmevent31";

    // Debug mode registers
    case 0x7B0:
        return "dcsr";
    case 0x7B1:
        return "depc";
    case 0x7B2:
        return "dscratch";

    // PULP extensions
    case 0x014:
        return "mhartid";

    case 0x7b3:
        return "scratch1";

#ifdef CONFIG_GVSOC_ISS_STACK_CHECKER
    case CSR_STACK_CONF:
        return "stack_conf";
    case CSR_STACK_START:
        return "stack_start";
    case CSR_STACK_END:
        return "stack_end";
#endif
    }

#if defined(ISS_HAS_PERF_COUNTERS)
    if ((reg >= CSR_PCCR(0) && reg <= CSR_PCCR(CSR_NB_PCCR)) || reg == CSR_PCER || reg == CSR_PCMR)
    {
      return "pccr";
    }
    if (reg == CSR_PCER)
    {
      return "pcer";
    }
    if (reg == CSR_PCMR)
    {
      return "pcmr";
    }
#endif

#if defined(CONFIG_GVSOC_ISS_RI5KY) || defined(CONFIG_GVSOC_ISS_HWLOOP)
    if (reg == CSR_HWLOOP0_START)
    {
      return "hwloop0_start";
    }
    if (reg == CSR_HWLOOP0_END)
    {
      return "hwloop0_end";
    }
    if (reg == CSR_HWLOOP0_COUNTER)
    {
      return "hwloop0_counter";
    }
    if (reg == CSR_HWLOOP1_START)
    {
      return "hwloop1_start";
    }
    if (reg == CSR_HWLOOP1_END)
    {
      return "hwloop1_end";
    }
    if (reg == CSR_HWLOOP1_COUNTER)
    {
      return "hwloop1_counter";
    }
#endif

    return "unknown";
}

CsrAbtractReg::CsrAbtractReg(iss_reg_t *value)
{
    if (value)
    {
        this->value_p = value;
    }
    else
    {
        this->value_p = &this->default_value;
    }
}

bool CsrAbtractReg::check_access(Iss *iss, bool write, bool read)
{
    if (write && this->write_illegal)
    {
        iss->exception.raise(iss->exec.current_insn, ISS_EXCEPT_ILLEGAL);
        return false;
    }

    return true;
}

void CsrAbtractReg::reset(bool active)
{
    if (active)
    {
        *this->value_p = this->reset_val;
    }
}

iss_reg_t CsrAbtractReg::handle(Iss *iss, iss_insn_t *insn, iss_reg_t pc, iss_reg_t reg_value)
{
    iss_reg_t value;

    CsrAbtractReg *csr = iss->csr.get_csr(UIM_GET(0));
    if (csr && !csr->check_access(iss, true, true))
    {
        return pc;
    }

    if (iss_csr_read(iss, UIM_GET(0), &value) == 0)
    {
        if (insn->out_regs[0] != 0)
        {
            // For now we don't have any mechanism to track validity of CSR, so set output
            // register as valid
            iss->regfile.memcheck_set_valid(REG_OUT(0), true);
            REG_SET(0, value);
        }
    }

    iss_csr_write(iss, UIM_GET(0), reg_value);

    return iss_insn_next(iss, insn, pc);
}

bool CsrAbtractReg::access(bool is_write, iss_reg_t &value)
{
    bool update = true;
    for (auto callback: this->callbacks)
    {
        update &= callback(is_write, value);
    }

    if (update)
    {
        if (is_write)
        {
            *this->value_p = ((*this->value_p) & ~this->write_mask) | (value & this->write_mask);
        }
        else
        {
            value = *this->value_p;
        }
    }
    return false;
}

void CsrAbtractReg::register_callback(std::function<bool(bool, iss_reg_t &)> callback)
{
    this->callbacks.push_back(callback);
}

void Csr::declare_csr(CsrAbtractReg *reg, std::string name, iss_reg_t address, iss_reg_t reset_val,
    iss_reg_t write_mask)
{
    if (this->regs.find(address) != this->regs.end())
    {
        this->trace.force_warning("Registering CSR at already occupied address (name: %s, address: 0x%x)\n",
            name.c_str(), address);
        return;
    }

    this->regs[address] = reg;
    reg->name = strdup(name.c_str());
    reg->write_mask = write_mask;
    reg->reset_val = reset_val;
}

CsrAbtractReg *Csr::get_csr(iss_reg_t address)
{
    return this->regs[address];
}

bool Csr::access(bool is_write, iss_reg_t address, iss_reg_t &value)
{
    CsrAbtractReg *csr = this->get_csr(address);
    if (csr != NULL)
    {
        return csr->access(is_write, value);
    }

    return true;
}


bool Mstatus::check_access(Iss *iss, bool write, bool read)
{
    if (read && iss->core.mode_get() == PRIV_U)
    {
        iss->exception.raise(iss->exec.current_insn, ISS_EXCEPT_ILLEGAL);
        return false;
    }

    return true;
}

bool Cycle::check_access(Iss *iss, bool write, bool read)
{
    if (write && iss->core.mode_get() != PRIV_M)
    {
        iss->exception.raise(iss->exec.current_insn, ISS_EXCEPT_ILLEGAL);
        return false;
    }

    return true;
}
