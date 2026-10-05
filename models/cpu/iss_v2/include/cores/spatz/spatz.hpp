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
#include <cpu/iss_v2/include/cores/vector_unit/vector_unit.hpp>

class Iss;

/**
 * Snitch core with the Spatz vector unit.
 *
 * With CONFIG_GVSOC_ISS_SNITCH_BARRIER_CSR, the core also has the Snitch
 * barrier CSR (0x7C2): reading it notifies the cluster barrier unit through
 * the barrier_req port and stalls the core until the unit answers on
 * barrier_ack, once every core of the cluster has arrived. Clusters using a
 * memory-mapped barrier instead leave it out.
 */
class Spatz
{
public:
    Spatz(Iss &iss);

    void start();
    void stop() {}
    void reset(bool active);

    Vu vu;

private:

    Iss &iss;

#ifdef CONFIG_GVSOC_ISS_SNITCH_BARRIER_CSR
    bool barrier_update(iss_insn_t *insn, bool is_write, iss_reg_t &value);
    static void barrier_sync(vp::Block *__this, bool value);

    CsrReg barrier;
    vp::WireMaster<bool> barrier_req_itf;
    vp::WireSlave<bool> barrier_ack_itf;
    // True between the barrier CSR read and the barrier_ack
    bool barrier_waiting;
    // True while the core is retained (stalled) on the barrier
    bool barrier_stalled;
    vp::Trace barrier_trace;
#endif
};
