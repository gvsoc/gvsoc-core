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
#include <types.hpp>

class Irq
{
public:
    Irq(Iss &iss);

    void build();

    bool mtvec_access(bool is_write, iss_reg_t &value);
    bool stvec_access(bool is_write, iss_reg_t &value);

    bool mtvec_set(iss_addr_t base);
    bool stvec_set(iss_addr_t base);
    inline void global_enable(int enable);
    void cache_flush();
    void reset(bool active);
    int check();
    void wfi_handle();
    void elw_irq_unstall();
    static void irq_req_sync(void *__this, int irq);

    Iss &iss;
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
    iss_insn_t *mtvec_insn;
    iss_insn_t *stvec_insn;
#else
    iss_insn_t *vectors[35];
#endif
    vp::reg_1 irq_enable;
    int debug_saved_irq_enable;
    int req_irq;
    bool req_debug;
    iss_insn_t *debug_handler;
    vp::trace trace;

    int irq_req;
    int irq_req_value;
    vp::wire_master<int> irq_ack_itf;
    vp::wire_slave<int> irq_req_itf;

};
