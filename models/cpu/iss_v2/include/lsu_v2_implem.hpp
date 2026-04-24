// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#pragma once

template<typename T>
inline bool LsuV2::load(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, true, reg);
}

template<typename T>
inline bool LsuV2::store(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::WRITE, false, reg);
}

template<typename T>
inline bool LsuV2::load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    {
        return true;
    }
    this->iss.timing.event_load_account(1);
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, true, reg);
}

template<typename T>
inline bool LsuV2::store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    {
        return true;
    }
    this->iss.timing.event_store_account(1);
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::WRITE, false, reg);
}

inline LsuReqEntry *LsuV2::get_req_entry()
{
    if (this->req_entry_first == NULL)
    {
        return NULL;
    }

    this->nb_pending_accesses++;
    LsuReqEntry *req = this->req_entry_first;
    this->req_entry_first = req->next;
    return req;
}

template<typename T>
inline bool LsuV2::load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::WRITE, false, reg);
}

template<typename T>
inline bool LsuV2::load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::READ, false, reg);
}

template<typename T>
inline bool LsuV2::store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, vp::IoReqOpcode::WRITE, false, reg);
}

inline void LsuV2::free_req_entry(LsuReqEntry *entry)
{
    this->nb_pending_accesses--;
    entry->next = this->req_entry_first;
    this->req_entry_first = entry;
}
