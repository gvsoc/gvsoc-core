// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Out-of-line bodies for Regfile methods that need a complete Iss type
// (because they call into iss.timing or similar members). Included
// after iss.hpp via the ISA implem-include list.

#pragma once

#include <cpu/iss_v2/include/regfile.hpp>
#include <vp/memcheck.hpp>

inline bool Regfile::memcheck_reg(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->regs_memcheck[reg] != (iss_reg_t)-1)
    {
        if (this->iss.traces.get_trace_engine()->is_memcheck_enabled())
        {
            return true;
        }
    }
#endif

    return false;
}

inline void Regfile::memcheck_branch_reg(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_reg(reg))
    {
        this->memcheck_reg_fault = true;
        this->memcheck_reg_fault_id = reg;
        this->memcheck_reg_fault_kind = "uninit-branch";
        this->memcheck_reg_fault_message = "Conditional jump depends on uninitialised register";
    }
#endif
}

inline void Regfile::memcheck_access_reg(int reg)
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_reg(reg))
    {
        this->memcheck_reg_fault = true;
        this->memcheck_reg_fault_id = reg;
        this->memcheck_reg_fault_kind = "uninit-address";
        this->memcheck_reg_fault_message = "Access address depends on uninitialised register";
    }

    // Latch the provenance of the base register so that the LSU attaches it to the
    // request it is about to send
    this->iss.lsu.pending_addr_buffer_id = this->regs_memcheck_id[reg];
#endif
}

inline void Regfile::memcheck_fault()
{
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_reg_fault)
    {
        this->memcheck_reg_fault = false;

        std::string message = this->memcheck_reg_fault_message
            + " (reg: " + std::to_string(this->memcheck_reg_fault_id) + ")";

        // Structured fault record for front-ends
        vp::MemCheck *mc = this->iss.get_memcheck();
        mc->fault.valid = true;
        mc->fault.kind = this->memcheck_reg_fault_kind;
        mc->fault.addr = 0;
        mc->fault.size = 0;
        mc->fault.is_write = false;
        mc->fault.buffer_id = 0;
        mc->fault.message = message;
        mc->fault.time = this->iss.time.get_time();
        mc->fault.core = this->iss.get_path();
        mc->fault.pc = this->iss.exec.current_insn;

        if (this->iss.gdbserver.is_enabled())
        {
            // When GDB is connected, throw a message without exiting and notify gdb
            // since this will do a break, so that user can continue
            this->trace.force_warning_no_error("%s\n", message.c_str());

            if (!this->iss.exec.halted.get())
            {
                this->iss.exec.retain_inc();
                this->iss.exec.halted.set(true);
            }
            this->iss.gdbserver.gdbserver->signal(&this->iss.gdbserver,
                vp::Gdbserver_engine::SIGNAL_BUS);
        }
        else if (mc->fault_stop())
        {
            // A front-end is attached (GUI or console), the simulation pauses on
            // the fault like on a watchpoint hit
            this->trace.force_warning_no_error("%s\n", message.c_str());
        }
        else
        {
            // Batch mode: report and apply the werror policy
            this->trace.force_warning("%s\n", message.c_str());
        }
    }
#endif
}

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
inline bool Regfile::scoreboard_insn_check(iss_insn_t *insn)
{
    uint64_t blocking = insn->sb_reg_mask & this->sb_reg_invalid;
    if (blocking == 0) return false;
    if (this->iss.cfg.scoreboard_waw_after_writeback)
    {
        // Only written registers whose loaded value is already there: no
        // hazard, the instruction takes them over from the load
        uint64_t written_back = blocking & ~insn->sb_in_reg_mask &
            this->iss.exec.delayed_release_mask();
        if (written_back == blocking)
        {
            this->iss.exec.delayed_release_drop(written_back);
            return false;
        }
    }
    // Hand the per-core events class the opaque reason byte that was
    // stored when the first blocking register was invalidated, so it
    // can fire the right producer-specific stall counter. RTL counts
    // each hazard exactly once (PCCR_in[2..3] are gated by id_valid_q,
    // which is 0 on the retry cycles of a held insn), so we fire only
    // on the FIRST cycle of each new hazard. A "new" hazard is any
    // stall whose (PC, cycle) pair is not the immediate continuation
    // of the previous one.
    int64_t cur_cycle = this->iss.clock.get_cycles();
    if (insn->addr != this->sb_last_stall_pc ||
        cur_cycle  != this->sb_last_stall_cycle + 1)
    {
        this->iss.timing.event_scoreboard_stall(
            this->sb_reason[__builtin_ctzll(blocking)]);
    }
    this->sb_last_stall_pc    = insn->addr;
    this->sb_last_stall_cycle = cur_cycle;
    this->trace.msg(vp::Trace::LEVEL_TRACE,
        "Scoreboard dependency (insn_mask: 0x%lx, core_mask: 0x%lx)\n",
        insn->sb_reg_mask, this->sb_reg_invalid);
    return true;
}
#endif
