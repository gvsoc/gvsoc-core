# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)
#
# Ramulator 2.x configuration for the GVSoC memory.ramulator wrapper.
#
# Uses the "External" frontend so the DRAM model is driven by an external
# simulator (GVSoC) via receive_external_requests(). Export to the YAML the
# C++ model loads with:
#
#   python3 -m ramulator export external_config.py -o config.yaml
#
# This is a plain DDR4-2400 single-rank single-channel config, matching the
# Ramulator README example.

import ramulator

# External frontend: GVSoC injects the requests and ticks the memory system.
frontend = ramulator.frontend.External(clock_ratio=1)

dram = ramulator.dram.DDR4(
    org_preset="DDR4_8Gb_x8",
    timing_preset="DDR4_2400R",
    rank=1,
)

ctrl = ramulator.controller.GenericDDR(
    dram=dram,
    scheduler=ramulator.scheduler.FRFCFS(),
    refresh_manager=ramulator.refresh_manager.AllBank(),
    row_policy=ramulator.row_policy.Open(),
    addr_mapper=ramulator.addr_mapper.RoBaRaCoCh(),
)

mem = ramulator.memory_system.GenericDRAM(
    clock_ratio=3,
    controllers=[ctrl],
    channel_mapper=ramulator.channel_mapper.CacheLineInterleave(),
)

# Append the cycle period to the memory config
org, timing = dram.resolve()
mem = mem.to_config()
mem["tCK_ps"] = timing["tCK_ps"]

sim = ramulator.Simulation(frontend, mem)
