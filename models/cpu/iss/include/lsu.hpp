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

#include <types.hpp>

#define ADDR_MASK (~(ISS_REG_WIDTH / 8 - 1))

class Lsu
{
public:
    Lsu(Iss &iss);

    void build();

    int data_req(iss_addr_t addr, uint8_t *data, int size, bool is_write);
    int data_req_aligned(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write);
    int data_misaligned_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write);

    static void exec_misaligned(void *__this, vp::clock_event *event);
    static void data_grant(void *_this, vp::io_req *req);
    static void data_response(void *_this, vp::io_req *req);

    inline void store(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void store_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    inline void load(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void load_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void load_signed(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void load_signed_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    inline void elw(iss_insn_t *insn, iss_addr_t addr, int size, int reg);
    inline void elw_perf(iss_insn_t *insn, iss_addr_t addr, int size, int reg);

    inline void stack_access_check(int reg, iss_addr_t addr);

    Iss &iss;

    vp::trace trace;

private:
    static void store_resume(void *_this);
    static void store_resume(Lsu *lsu);
    static void load_resume(Lsu *lsu);
    static void elw_resume(Lsu *lsu);
    static void load_signed_resume(Lsu *lsu);
};
