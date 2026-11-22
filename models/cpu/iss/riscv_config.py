#
# Copyright (C) 2026 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Configuration of the v1 RISC-V ISS (cpu.iss.riscv.RiscvCommon).

Most fields are set by RiscvCommon from its constructor arguments. A core
may come with its own config class, which must derive from IssConfig: the
ISS copies the IssConfig prefix of the compiled struct.
"""

from __future__ import annotations

from typing import Annotated, ClassVar

from config_tree import Config, cfg_field
from gvrun.runtime import Runtime
from vp.power_config import PowerSourceConfig
from cpu.iss_v2.riscv_config import RiscvPowerConfig, RiscvBinaryConfig, RiscvArgConfig


class IssVuConfig(Config):
    """Vector unit (Ara / Spatz) attached to the v1 ISS, see pulp.ara.ara.attach."""

    _defer_parent_init: ClassVar[bool] = True

    nb_lanes: int = cfg_field(default=0, desc="Number of vector lanes")
    lsu_width: int = cfg_field(default=0, desc="Width in bytes of the vector LSU, per port")
    compute_width: int = cfg_field(default=0, desc="Width in bytes of the vector compute units")
    nb_ports: int = cfg_field(default=0, desc="Number of memory ports of the Spatz VLSU")
    nb_outstanding_reqs: int = cfg_field(default=0, desc=(
        "Number of outstanding requests per port of the Spatz VLSU"
    ))


class IssConfig(Config):
    """Configuration of the v1 RISC-V ISS."""

    isa: str = cfg_field(default='rv32imafdc', desc=(
        "ISA string of the core"
    ))
    misa: int = cfg_field(default=0, fmt="hex", desc=(
        "Reset value of the MISA CSR"
    ))
    has_double: bool = cfg_field(default=False, desc=(
        "True if the ISA has the D extension"
    ))
    riscv_dbg_unit: bool = cfg_field(default=False, desc=(
        "True if the core has the RISC-V debug unit"
    ))
    debug_handler: int = cfg_field(default=0, fmt="hex", desc=(
        "Address of the debug handler"
    ))
    cluster_id: int = cfg_field(default=0, desc=(
        "Cluster ID, in MHARTID bits 5 and above"
    ))
    core_id: int = cfg_field(default=0, desc=(
        "Core ID, in MHARTID bits 0 to 4"
    ))
    fetch_enable: bool = cfg_field(default=False, desc=(
        "True if the core starts fetching instructions after reset"
    ))
    boot_addr: int = cfg_field(default=0, fmt="hex", desc=(
        "Boot address"
    ))
    bootaddr_offset: int = cfg_field(default=0, fmt="hex", desc=(
        "Offset added to the boot address to get the first fetched instruction"
    ))
    has_memory: bool = cfg_field(default=False, desc=(
        "True if the core accesses a memory directly, see memory_start"
    ))
    memory_start: int = cfg_field(default=0, fmt="hex", desc=(
        "Base address of the memory accessed directly, when has_memory is True"
    ))
    memory_size: int = cfg_field(default=0, fmt="hex", desc=(
        "Size of the memory accessed directly, when has_memory is True"
    ))
    power_insn_groups: list[PowerSourceConfig] = cfg_field(default_factory=list, desc=(
        "Energy of the instructions, indexed by ISA power group"
    ))
    power: RiscvPowerConfig = cfg_field(default=None, desc=(
        "Named core power sources. None (all sources inert) without a power model"
    ))
    binaries: Annotated[list[RiscvBinaryConfig], Runtime] = cfg_field(default_factory=list,
        desc=(
        "ELF binaries whose symbols are used for traces and the debugger, known at run time"
    ))
    args: Annotated[list[RiscvArgConfig], Runtime] = cfg_field(default_factory=list, desc=(
        "Command-line arguments given to the binary through HTIF, known at run time"
    ))
    htif_tohost: Annotated[int, Runtime] = cfg_field(default=0, fmt="hex", desc=(
        "Address of the HTIF tohost symbol of the binary, known at run time"
    ))
    htif_fromhost: Annotated[int, Runtime] = cfg_field(default=0, fmt="hex", desc=(
        "Address of the HTIF fromhost symbol of the binary, known at run time"
    ))
    vu: IssVuConfig = cfg_field(default_factory=IssVuConfig, desc=(
        "Vector unit, when one is attached"
    ))
