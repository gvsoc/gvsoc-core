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


static std::vector<std::vector<iss_resource_instance_t>> resources;


// Called when an instruction with an associated resource is scheduled
iss_reg_t iss_resource_offload(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // First get the instance associated to this core for the resource associated to this instruction
    iss_resource_instance_t *instance = iss->exec.resources[insn->resource_id];

    // Check if the instance is ready to accept an access
    if (iss->exec.get_cycles() < instance->cycles)
    {
        // If not, account the number of cycles until the instance becomes available
        int64_t bw_cycles = instance->cycles - iss->exec.get_cycles();
        // Bandwidth cycles are always accounted as stalls, since they are immediately blocking
        // the core
        iss->timing.event_apu_contention_account(bw_cycles);
        iss->timing.stall_insn_account(bw_cycles);

        // And account the access on the instance. The time taken by the access is indicated by the instruction bandwidth
        instance->cycles += insn->resource_bandwidth;
    }
    else
    {
        // The instance is available, just account the time taken by the access, indicated by the instruction bandwidth
        instance->cycles = iss->exec.get_cycles() + insn->resource_bandwidth;
    }

    // Now that timing is modeled, execute the instruction
    iss_reg_t retval = insn->resource_handler(iss, insn, pc);

    // Account the latency of the resource on the core, as the result is available after the instruction latency
    if (insn->resource_latency > 1)
    {
        int64_t latency_cycles = insn->resource_latency;

#if defined(CONFIG_GVSOC_ISS_SCOREBOARD)
        iss->regfile.scoreboard_reg_set_timestamp(insn->out_regs[0], latency_cycles, -1);
#endif

#if defined(PIPELINE_STALL_THRESHOLD)
        iss->timing.event_apu_contention_account(latency_cycles - 1);
        iss->timing.stall_insn_account(latency_cycles - 1);
#endif
    }

    return retval;
}

void iss_resource_attach_from_tag(Iss *iss, std::string tag, int id, int latency, int bandwidth)
{
    for (iss_decoder_item_t *insn: *iss->decode.get_insns_from_tag(tag))
    {
        insn->u.insn.resource_id = id;
        insn->u.insn.resource_latency = latency;
        insn->u.insn.resource_bandwidth = bandwidth;
    }
}

void iss_resource_declare(Iss *iss, int id, int nb_instances)
{
    resources.resize(id + 1);
    resources[id].resize(nb_instances);
    for (int i=0; i<nb_instances; i++)
    {
        resources[id][i].cycles = 0;
    }
}

void iss_resource_assign_instance(Iss *iss, int id, int instance_id)
{
    iss->exec.resources.resize(id + 1);
    iss->exec.resources[id] = &resources[id][instance_id];
}