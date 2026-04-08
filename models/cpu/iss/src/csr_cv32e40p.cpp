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
 * CV32E40P-specific CSR subclass implementation.
 *
 * This file is compiled for every ISS target but all code is guarded by
 * CONFIG_GVSOC_ISS_CV32E40P so it compiles to nothing for other cores.
 *
 * Authors: Marco Paci, Chips-it (marco.paci@chips.it)
 */

#ifdef CONFIG_GVSOC_ISS_CV32E40P

#include "cpu/iss/include/iss.hpp"

// Helper: read config int with fallback default.
// Uses get() to distinguish "key missing" from "key=0".
static inline int cfg_int_or(js::Config *cfg, const char *key, int dflt)
{
    js::Config *child = cfg->get(key);
    return child ? child->get_int() : dflt;
}

Cv32e40pCsr::Cv32e40pCsr(Iss &iss)
    : Csr(iss)
{
    // All CSR customization moved to build() — constructor runs too early
    // during Iss member initialization (iss.top not yet set).
}

void Cv32e40pCsr::build()
{
    Csr::build();  // Base class CSR setup (regs map, trace, etc.)
    this->build_cv32e40p();  // CV32E40P-specific customization
}

void Cv32e40pCsr::build_cv32e40p()
{
    /* reset() runs AFTER build() in GVSOC — reset_val must be correct */

    // Cache FPU/ZFINX config for hot-path use
    this->m_fpu_in_isa = this->iss.top.get_js_config()->get_child_bool("fpu_in_isa");
    this->m_zfinx = this->iss.top.get_js_config()->get_child_bool("zfinx");

    // CV32E40P is M-mode only: remove CSRs that don't exist
    uint32_t nonexistent[] = {
        0x100, 0x104, 0x105, 0x106,           // sstatus, sie, stvec, scounteren
        0x140, 0x141, 0x142, 0x143, 0x144,     // sscratch, sepc, scause, stval, sip
        0x302, 0x303,                           // medeleg, mideleg (no delegation)
        0x306,                                  // mcounteren (no U-mode)
        0x740, 0x741, 0x742, 0x744,             // NMI: mnscratch, mnepc, mncause, mnstatus
        0x008, 0x009, 0x00A, 0x00F,             // Vector: vstart, vxstat, vxrm, vcsr
        0xC20, 0xC21, 0xC22,                    // Vector: vl, vtype, vlenb
    };
    for (uint32_t addr : nonexistent)
    {
        this->undeclare_csr(addr);
    }

    // mhpmevent CSRs — base Csr skips these for CV32E40P, declare here in build_cv32e40p()
    for (int i = 0; i < 29; i++)
    {
        this->declare_csr(&this->mhpmevent[i],
            "mhpmevent" + std::to_string(i + 3), 0x323 + i);
    }

    // D29: tinfo register — CV32E40P trigger info (read-only, value=0x4)
    this->undeclare_csr(0x7A4);  // Remove if base declared it
    this->declare_csr(&this->tinfo, "tinfo", 0x7A4);

    auto *cfg = this->iss.top.get_js_config();

    // D54: PULP custom CSRs (0xCD0-0xCD2) — cv32e40p_cs_registers.sv
    // These exist when COREV_PULP=1 (all PULP configs). Read-only via CSR instructions.
    bool pulpv2 = cfg->get_child_bool("pulpv2");
    if (pulpv2)
    {
        // D54: PULP custom CSRs — reset_val MUST be passed to declare_csr()
        // because Csr::reset() runs AFTER build() and overwrites .value with
        // reset_val (default 0). Values set only via .value are clobbered.

        // 0xCD0 UHARTID — duplicate of mhartid, user-mode readable
        iss_reg_t uhartid_val = cfg_int_or(cfg, "mhartid_value", 0);
        this->declare_csr(&this->uhartid, "uhartid", 0xCD0, uhartid_val);
        this->uhartid.set_write_mask(0);  // read-only

        // 0xCD1 PRIVLV — current privilege level (M-mode only → always 3)
        this->declare_csr(&this->privlv, "privlv", 0xCD1, 3);
        this->privlv.set_write_mask(0);  // read-only

        // 0xCD2 ZFINX — returns 1 when FPU=1 && ZFINX=1, else 0
        iss_reg_t zfinx_val = (this->m_zfinx) ? 1 : 0;
        this->declare_csr(&this->zfinx_csr, "zfinx", 0xCD2, zfinx_val);
        this->zfinx_csr.set_write_mask(0);  // read-only
    }

    // Read-only info registers
    this->mvendorid.write_illegal = true;
    this->mimpid.write_illegal = true;
    this->marchid.write_illegal = true;
    this->mhartid.write_illegal = true;

    // ----------------------------------------------------------------
    // CSR write masks — values from CV32E40P RTL (cv32e40p_cs_registers.sv)
    // Config Python values are used if present; fallback to RTL-derived
    // constants if config key returns 0 (missing).
    // ----------------------------------------------------------------

    // D62: mstatus effective write mask — matches RTL always_ff forcing.
    // CV32E40P PULP_SECURE=0: always_ff forces MPP=M, MPRV=0, UIE=0, UPIE=0.
    // Only MIE(3) + MPIE(7) are truly writable. With FPU: add FS(14:13).
    // (cv32e40p_cs_registers.sv:1222-1230)
    // Reset is ALWAYS 0x1800 (FS=Off) — RTL sets mstatus_fs_q=FS_OFF at reset
    // even when FPU=1 (cv32e40p_cs_registers.sv:1178). FS transitions to Dirty
    // only when FPU registers are written, not at reset.
    iss_reg_t mstatus_wmask_dflt = this->m_fpu_in_isa ? 0x6088 : 0x0088;
    iss_reg_t mstatus_reset = 0x00001800;  // MPP=M, FS=Off for ALL configs
    this->mstatus.set_write_mask(cfg_int_or(cfg, "mstatus_write_mask", (int)mstatus_wmask_dflt));
    this->mstatus.reset_val = mstatus_reset;
    this->mstatus.value     = mstatus_reset;

    // D27: mie — only IRQ_MASK bits writable (cv32e40p_pkg.sv:725)
    this->mie.set_write_mask(cfg_int_or(cfg, "mie_write_mask", 0xFFFF0888));

    // mip — read-only in M-mode
    this->mip.set_write_mask(0);

    // D26: mtvec — bits[7:1] hardwired to 0
    this->mtvec.set_write_mask(cfg_int_or(cfg, "mtvec_write_mask", 0xFFFFFF01));
    this->mtvec.reset_val = cfg_int_or(cfg, "mtvec_reset", 0x1);
    this->mtvec.value     = this->mtvec.reset_val;  // build() runs AFTER reset()

    // D58: mtval — CV32E40P hardwires mtval to 0 (not writable)
    // RTL: cv32e40p_cs_registers.sv never assigns mtval_n on CSR write
    this->mtval.set_write_mask(cfg_int_or(cfg, "mtval_write_mask", 0x00000000));

    // mcause — bit[31] (interrupt) + bits[4:0] (exception code)
    this->mcause.set_write_mask(cfg_int_or(cfg, "mcause_mask", 0x8000001F));

    // ----------------------------------------------------------------
    // Trigger module CSRs
    // ----------------------------------------------------------------
    // D34: tselect — CV32E40P has exactly 1 trigger, tselect hardwired to 0
    this->tselect.reset_val = 0;
    this->tselect.value     = 0;
    this->tselect.set_write_mask(0);  // read-only: writes ignored, always reads 0

    // D25: tdata1 — type=2 (mcontrol), dmode=1, action=1, match=0, m=1
    this->tdata1.reset_val = cfg_int_or(cfg, "tdata1_reset", 0x28001040);
    this->tdata1.value     = this->tdata1.reset_val;
    // D58: tdata1 is writable ONLY from Debug Mode (cv32e40p_cs_registers.sv:1262:
    // tmatch_control_we = csr_we_int & debug_mode_i). In M-mode, all writes
    // are silently ignored. Since GVSOC doesn't implement Debug Mode, mask=0.
    this->tdata1.set_write_mask(cfg_int_or(cfg, "tdata1_write_mask", 0x00000000));

    // D58: tdata2 writable ONLY from Debug Mode (cv32e40p_cs_registers.sv:1263)
    this->tdata2.set_write_mask(cfg_int_or(cfg, "tdata2_write_mask", 0x00000000));
    this->tdata3.set_write_mask(cfg_int_or(cfg, "tdata3_write_mask", 0x00000000));

    // D58: mcontext/scontext — writable only from Debug Mode
    this->mcontext.set_write_mask(cfg_int_or(cfg, "mcontext_write_mask", 0x00000000));
    this->scontext.set_write_mask(cfg_int_or(cfg, "scontext_write_mask", 0x00000000));

    // D29: tinfo — read-only, bit[2]=1 (type 2 = mcontrol supported)
    this->tinfo.reset_val = cfg_int_or(cfg, "tinfo_reset", 0x4);
    this->tinfo.value     = this->tinfo.reset_val;
    this->tinfo.set_write_mask(0);  // read-only

    // ----------------------------------------------------------------
    // Read-only info registers — initialization
    // ----------------------------------------------------------------
    // D32: mvendorid — OpenHW JEDEC bank 13, RISC-V compliant
    this->mvendorid.reset_val = cfg_int_or(cfg, "mvendorid_value", 0x00000602);
    this->mvendorid.value     = this->mvendorid.reset_val;

    // D32: marchid — CV32E40P architecture ID (cv32e40p_pkg.sv: MARCHID = 32'h4)
    this->marchid.reset_val = cfg_int_or(cfg, "marchid_value", 0x00000004);
    this->marchid.value     = this->marchid.reset_val;

    // D32: mhartid — hart ID (default 0, parameterizable per config)
    this->mhartid.reset_val = cfg_int_or(cfg, "mhartid_value", 0x00000000);
    this->mhartid.value     = this->mhartid.reset_val;

    // BUG-29b: mimpid from JSON config (RTL: FPU||COREV_PULP||COREV_CLUSTER ? 1 : 0)
    this->mimpid.value = (iss_reg_t)cfg_int_or(cfg, "mimpid", 0);
    this->mimpid.reset_val = this->mimpid.value;

    // ----------------------------------------------------------------
    // Counter CSRs
    // ----------------------------------------------------------------

    // D18b: mcountinhibit — RTL resets to write_mask value (all implemented bits set).
    // With N=1 HPM counter: mask=0x0d (CY,IR,HPM3). With N=29: mask=0xfffffffd.
    iss_reg_t mcountinhibit_mask = (iss_reg_t)cfg_int_or(cfg, "mcountinhibit_mask", 0x0d);
    this->mcountinhibit.set_write_mask(mcountinhibit_mask);
    this->mcountinhibit.reset_val = mcountinhibit_mask;
    this->mcountinhibit.value     = mcountinhibit_mask;

    // HPM counters: only first num_mhpmcounters are implemented
    int num_hpm = cfg_int_or(cfg, "num_mhpmcounters", 1);
    for (int i = 0; i < 29; i++)
    {
        if (i < num_hpm)
        {
            this->mhpmcounter[i].set_write_mask((iss_reg_t)-1);
#if ISS_REG_WIDTH == 32
            this->mhpmcounterh[i].set_write_mask((iss_reg_t)-1);
#endif
        }
        else
        {
            // CV32E40P: unimplemented counters are WARL (writes ignored, no exception)
            this->mhpmcounter[i].set_write_mask(0);
#if ISS_REG_WIDTH == 32
            this->mhpmcounterh[i].set_write_mask(0);
#endif
        }
    }

    // HPM event selectors: only mhpmevent3 has bits[15:0]
    int num_hpm_events = cfg_int_or(cfg, "num_hpm_events", 16);
    iss_reg_t evt_mask = (num_hpm_events > 0) ?
        ((1U << num_hpm_events) - 1) : 0;
    for (int i = 0; i < 29; i++)
    {
        this->mhpmevent[i].set_write_mask((i < num_hpm) ? evt_mask : 0);
    }

    // D28: dcsr — build() runs after reset(), so apply dcsr M-mode here too
    this->dcsr = (4 << 28) | 0x3;
}

void Cv32e40pCsr::reset(bool active)
{
    Csr::reset(active);

    // D28: dcsr reset — xdebugver=4 in [31:28], prv=M-mode in [1:0]
    // Base Csr::reset() sets dcsr = 4 << 28 (prv=0). CV32E40P is M-mode only.
    this->dcsr = (4 << 28) | 0x3;

    this->mcycle_offset = 0;

    // Re-apply config-driven values after Csr::reset() which may clobber them
    // with base-class defaults (e.g. marchid=0x14 from GVSOC base instead of
    // 0x04 from CV32E40P RTL). Uses reset_val set by build_cv32e40p().
    if (active)
    {
        this->mvendorid.value = this->mvendorid.reset_val;
        this->marchid.value   = this->marchid.reset_val;
        this->mimpid.value    = this->mimpid.reset_val;

        // D45: mstatus — Core::build() contaminates reset_val with FS=Dirty.
        // CV32E40P: reset is ALWAYS 0x1800 (FS=Off, MPP=M) for ALL configs
        // including FPU (RTL: mstatus_fs_q <= FS_OFF at reset).
        iss_reg_t mstatus_rst = 0x00001800;
        this->mstatus.reset_val = mstatus_rst;
        this->mstatus.value     = mstatus_rst;

        // D45b: tdata1 — re-apply value from build_cv32e40p() after Csr::reset()
        // may have used a contaminated reset_val.  build_cv32e40p() reads
        // tdata1_reset from JSON config (default 0x28001040: m=1, u=0).
        this->tdata1.value = this->tdata1.reset_val;
    }
}

bool Cv32e40pCsr::mstatus_access(bool is_write, iss_reg_t &value)
{
    // Return true → let CsrAbtractReg::access() handle the write_mask.
    // For reads, force MPP=M so the value read is correct.
    if (!is_write)
    {
        this->mstatus.mpp = PRIV_M;
        value = this->mstatus.value;
        return false;
    }
    return true;
}

bool Cv32e40pCsr::minstret_access(bool is_write, iss_reg_t &value)
{
    // BUG-26 OPEN: minstret doesn't auto-increment like RTL.
    // Marked volatile in StepNCompare, so not compared.
    return true;  // let default read/write proceed
}

bool Cv32e40pCsr::mcountinhibit_access(bool is_write, iss_reg_t &value)
{
    return true;  // placeholder
}

bool Cv32e40pCsr::mcycle_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        this->mcycle.value = value;
        this->mcycle_offset = (int64_t)value
            - (int64_t)this->iss.top.clock.get_cycles();
    }
    else
    {
        if (this->mcountinhibit.value & 0x1)
        {
            value = this->mcycle.value;
        }
        else
        {
            value = (iss_reg_t)((int64_t)this->iss.top.clock.get_cycles()
                + this->mcycle_offset);
        }
    }
    return false;
}

void Cv32e40pCsr::mstatus_read_fixup(iss_reg_t &value)
{
    /* CV32E40P: set mstatus.SD (bit 31) when FS[14:13]==3 or XS[16:15]==3.
     * This matches the RTL read-back behavior for mstatus. */
    if (this->m_fpu_in_isa && (((value >> 13) & 3) == 3 || ((value >> 15) & 3) == 3))
    {
        value |= (1ULL << 31);
    }
}

#endif /* CONFIG_GVSOC_ISS_CV32E40P */
