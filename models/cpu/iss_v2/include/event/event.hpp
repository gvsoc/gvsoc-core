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

#pragma once

#include <vp/vp.hpp>

class Events
{
public:

    Events(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    inline void stall_fetch_account(int count){}
    inline void stall_taken_branch_account(){}
    inline void stall_insn_account(int cycles){}
    inline void stall_insn_dependency_account(int latency){}
    inline void stall_load_dependency_account(int latency){}
    inline void stall_jump_account(){}
    inline void stall_misaligned_account(){}
    inline void stall_load_account(int cycles){}
    inline void insn_account(){}
    inline void insn_stall_account(){}
    inline void cycle_account(){}
    inline void insn_stall_start(){}
    inline void insn_stall_stop(){}

    virtual inline void event_cycle_enable();
    virtual inline void event_cycle_disable();
    virtual inline void event_instr_account();
    virtual inline void event_fetch_account();
    virtual inline void event_imiss_account(int incr);
    virtual inline void event_imiss_start();
    virtual inline void event_imiss_stop();
    virtual inline void event_load_account(int incr);
    virtual inline void event_rvc_account(int incr);
    virtual inline void event_store_account(int incr);
    virtual inline void event_branch_account();
    virtual inline void event_taken_branch_account();
    virtual inline void event_jump_account();
    // Called once per executed JALR with the rs1 register index. Lets a
    // core (e.g. Ri5kyEvents) charge the taken-jump pipeline flush and
    // detect the RI5CY-style jr_stall (rs1 produced by a recent
    // instruction). Default: no-op.
    virtual inline void event_jalr_account(int rs1) {}
    // Called when an instruction retires, with the retiring insn. Lets a
    // core track the previous instruction's destination register so the
    // jalr-with-producer hazard can be detected by event_jalr_account.
    // Default: no-op.
    virtual inline void event_retire_account(iss_insn_t *insn) {}
    // Called when a retiring insn carries a non-zero per-instruction
    // latency (set by the decoder, typically via a setup pass that walks
    // get_insns_from_tag(...) and assigns u.insn.latency = N). The hook
    // lets each core pick a stall policy that matches its pipeline:
    // unconditional structural stall (e.g. RI5CY MULH FSM blocking the
    // ID->EX port), scoreboard-timestamp only (dependency-aware), resource
    // contention, etc. Default: no-op, so cores that don't tag any insn
    // and don't override this pay nothing beyond the latency != 0 check.
    virtual inline void event_insn_latency_account(iss_insn_t *insn, int latency) {}
    // Called by the shared rv32m.hpp div/divu/rem/remu handlers with the
    // live operand values, so per-core events classes can model
    // operand-dependent latency (e.g. the RI5CY divider's serial
    // bit-iteration loop whose cycle count depends on the divisor's
    // leading-zero count). Default: no-op.
    virtual inline void event_div_account(iss_reg_t dividend, iss_reg_t divisor,
                                          bool is_signed, bool is_rem) {}
    virtual inline void event_misaligned_account(int incr);
    virtual inline void event_apu_contention_account(int incr){}
    virtual inline void event_load_load_account(int incr){}

    inline void event_trace_account(unsigned int event, int cycles){}
    inline void event_trace_set(unsigned int event){}
    inline void event_trace_reset(unsigned int event){}
    inline int event_trace_is_active(unsigned int event){ return 0;}

    inline void stall_cycles_account(int incr){}

    inline void event_account(unsigned int event, int incr){}
    inline void handle_pending_events(){}

    vp::Event state_event;
    vp::Event pc_trace_event;


    vp::Event active_pc_trace_event;
    std::vector<vp::PowerSource> insn_groups_power;
    vp::PowerSource power_stall_first;
    vp::PowerSource power_stall_next;
    vp::PowerSource background_power;

protected:

    Iss &iss;

    vp::Event event_cycles;
    vp::Event event_instr;
    vp::Event event_fetch;
    vp::Event event_ld_stall;
    vp::Event event_jmp_stall;
    vp::Event event_imiss;
    vp::Event event_ld;
    vp::Event event_st;
    vp::Event event_jump;
    vp::Event event_branch;
    vp::Event event_taken_branch;
    vp::Event event_rvc;
    vp::Event event_misaligned;
};
