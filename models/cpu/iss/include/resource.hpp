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

#ifndef __CPU_ISS_RESOURCE_HPP
#define __CPU_ISS_RESOURCE_HPP

#include "cpu/iss/include/types.hpp"

iss_reg_t iss_resource_offload(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
void iss_resource_attach_from_tag(Iss *iss, std::string tag, int id, int latency, int bandwidth);
void iss_resource_declare(Iss *iss, int id, int nb_instances);
void iss_resource_assign_instance(Iss *iss, int id, int instance_id);

#endif
