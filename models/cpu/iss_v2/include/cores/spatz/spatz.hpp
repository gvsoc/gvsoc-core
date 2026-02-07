/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include <cpu/iss_v2/include/types.hpp>
#include <cpu/iss_v2/include/insn.hpp>
#include <cpu/iss_v2/include/csr.hpp>
#include <cpu/iss_v2/include/vector.hpp>
#include <cpu/iss_v2/include/cores/ara/ara.hpp>

class Iss;

class Spatz
{
public:
    Spatz(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    Ara ara;

    void insn_commit(PendingInsn *pending_insn);
    static iss_reg_t vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

private:

    Iss &iss;
};
