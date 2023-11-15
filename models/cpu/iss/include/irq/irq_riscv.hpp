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
#include <cpu/iss/include/types.hpp>

#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_VS_SOFT  2
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_VS_TIMER 6
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_VS_EXT   10
#define IRQ_M_EXT    11
#define IRQ_S_GEXT   12
#define IRQ_COP      12
#define IRQ_LCOF     13

class Irq
{
public:
    Irq(Iss &iss);

    void build();

    bool mideleg_access(bool is_write, iss_reg_t &value);
    bool mip_access(bool is_write, iss_reg_t &value);
    bool mie_access(bool is_write, iss_reg_t &value);
    bool sip_access(bool is_write, iss_reg_t &value);
    bool sie_access(bool is_write, iss_reg_t &value);
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
    void check_interrupts();
    static void msi_sync(void *__this, bool value);
    static void mti_sync(void *__this, bool value);
    static void mei_sync(void *__this, bool value);
    static void sei_sync(void *__this, bool value);

    Iss &iss;

    vp::reg_1 irq_enable;
    int debug_saved_irq_enable;
    int req_irq;
    bool req_debug;
    iss_reg_t debug_handler;
    vp::Trace trace;
    vp::WireSlave<bool> msi_itf;
    vp::WireSlave<bool> mti_itf;
    vp::WireSlave<bool> mei_itf;
    vp::WireSlave<bool> sei_itf;
};
