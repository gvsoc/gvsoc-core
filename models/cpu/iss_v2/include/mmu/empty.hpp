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

class MmuEmpty
{
public:

    MmuEmpty(Iss &iss) {}

    void start() {}
    void stop() {}
    void reset(bool active) {}

    inline bool insn_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr) { phys_addr = virt_addr; return false; }
    inline bool load_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr, bool &use_mem_array) { phys_addr = virt_addr; return false; }
    inline bool store_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr, bool &use_mem_array) { phys_addr = virt_addr; return false; }

    inline void flush(iss_addr_t address, iss_reg_t address_space) {}
};
