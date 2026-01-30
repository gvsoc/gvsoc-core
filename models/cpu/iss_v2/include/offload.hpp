/*
 * Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

#include <stdint.h>


template<typename T>
class IssOffloadInsn
{
public:
    bool granted;
    uint64_t opcode;
    T arg_a;
    T arg_b;
    T arg_c;
    T arg_d;
    T result;
};

template<typename T>
class IssOffloadInsnGrant
{
public:
    T result;
};

class Offload
{
public:
    Offload(Iss &iss);
    void start() {}
    void stop() {}
    void reset(bool active);

    bool offload_insn(IssOffloadInsn<iss_reg_t> *insn);

private:
    static void offload_grant(vp::Block *__this, IssOffloadInsnGrant<iss_reg_t> *result);

    Iss &iss;
    bool denied;
    vp::WireMaster<IssOffloadInsn<iss_reg_t> *> offload_itf;
    vp::WireSlave<IssOffloadInsnGrant<iss_reg_t> *> offload_grant_itf;
};
