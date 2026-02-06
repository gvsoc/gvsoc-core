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

template<typename T>
inline bool Lsu::load(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, false, false, reg);
}

template<typename T>
inline bool Lsu::load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, false, true, reg);
}

template<typename T>
inline bool Lsu::store(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, true, false, reg);
}

template<typename T>
inline bool Lsu::load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    // if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    // {
    //     return true;
    // }
    // this->iss.timing.event_load_account(1);
    return this->data_req_virtual(insn, addr, size, false, false, reg);
}

template<typename T>
inline bool Lsu::load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    // if (this->iss.gdbserver.watchpoint_check(false, addr, size))
    // {
    //     return true;
    // }
    // this->iss.timing.event_load_account(1);
    return this->data_req_virtual(insn, addr, size, false, true, reg);
}

template<typename T>
inline bool Lsu::store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    // if (this->iss.gdbserver.watchpoint_check(true, addr, size))
    // {
    //     return true;
    // }
    // this->iss.timing.event_store_account(1);
    return this->data_req_virtual(insn, addr, size, true, false, reg);
}

inline LsuReqEntry *Lsu::get_req_entry()
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
inline bool Lsu::load_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, false, false, reg);
}

template<typename T>
inline bool Lsu::store_float(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, true, false, reg);
}

template<typename T>
inline bool Lsu::load_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, false, false, reg);
}

template<typename T>
inline bool Lsu::store_float_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg)
{
    return this->data_req_virtual(insn, addr, size, true, false, reg);
}

inline void Lsu::free_req_entry(LsuReqEntry *entry)
{
    this->nb_pending_accesses--;
    entry->next = this->req_entry_first;
    this->req_entry_first = entry;
}
