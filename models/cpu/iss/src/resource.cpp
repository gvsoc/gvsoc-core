/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

#include "cpu/iss/include/iss.hpp"

void iss_resource_init(Iss *iss)
{
    // Initialize all the resources and assign the first instance by default
    iss->exec.resources.resize(iss_get_isa_set()->nb_resources);

    for (int i = 0; i < iss_get_isa_set()->nb_resources; i++)
    {
        iss_resource_t *resource = &iss_get_isa_set()->resources[i];
        for (int j = 0; j < resource->nb_instances; j++)
        {
            iss_resource_instance_t *instance = new iss_resource_instance_t;
            instance->cycles = 0;
            resource->instances.push_back(instance);
        }

        iss->exec.resources[i] = resource->instances[0];
    }
}

// Called when an instruction with an associated resource is scheduled
iss_reg_t iss_resource_offload(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // First get the instance associated to this core for the resource associated to this instruction
    iss_resource_instance_t *instance = iss->exec.resources[insn->resource_id];
    int64_t cycles = 0;

    // Check if the instance is ready to accept an access
    if (iss->top.clock.get_cycles() < instance->cycles)
    {
        // If not, account the number of cycles until the instance becomes available
        cycles = instance->cycles - iss->top.clock.get_cycles();
        iss->timing.event_insn_contention_account(cycles);

        // And account the access on the instance. The time taken by the access is indicated by the instruction bandwidth
        instance->cycles += insn->resource_bandwidth;
    }
    else
    {
        // The instance is available, just account the time taken by the access, indicated by the instruction bandwidth
        instance->cycles = iss->top.clock.get_cycles() + insn->resource_bandwidth;
    }

    // Account the latency of the resource on the core, as the result is available after the instruction latency
    iss->timing.stall_insn_account(cycles + insn->resource_latency - 1);

    // Now that timing is modeled, execute the instruction
    return insn->resource_handler(iss, insn, pc);
}