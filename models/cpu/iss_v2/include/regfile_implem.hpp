// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Out-of-line bodies for Regfile methods that need a complete Iss type
// (because they call into iss.timing or similar members). Included
// after iss.hpp via the ISA implem-include list.

#pragma once

#include <cpu/iss_v2/include/regfile.hpp>

#ifdef CONFIG_GVSOC_ISS_REGFILE_SCOREBOARD
inline bool Regfile::scoreboard_insn_check(iss_insn_t *insn)
{
    uint64_t blocking = insn->sb_reg_mask & this->sb_reg_invalid;
    if (blocking == 0) return false;
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
