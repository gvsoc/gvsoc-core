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

#include "types.hpp"


class Regfile
{
public:

    Regfile(Iss &iss);

    void reset(bool active);

    iss_reg_t regs[ISS_NB_REGS + ISS_NB_FREGS];

    inline iss_reg_t *reg_ref(int reg);
    inline iss_reg_t *reg_store_ref(int reg);
    inline void set_reg(int reg, iss_reg_t value);
    inline iss_reg_t get_reg(int reg);
    inline iss_reg64_t get_reg64(int reg);
    inline void set_reg64(int reg, iss_reg64_t value);

private:
    Iss &iss;

};