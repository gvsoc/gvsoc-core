#
# Copyright (C) 2020 ETH Zurich and University of Bologna
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


from typing import ClassVar

from config_tree import Config, cfg_field
from vp.power_config import PowerSourceConfig


class RiscvPowerConfig(Config):
    """Named core power sources, typically filled from the core power model
    YAML with vp.power_config.apply_power_yaml. Untouched sources stay
    inert."""

    _defer_parent_init: ClassVar[bool] = True

    stall_first: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Energy of the first stall cycle of an instruction"
    ))
    stall_next: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Energy of each following stall cycle"
    ))
    background: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Core background power (dynamic + leakage)"
    ))


class RiscvConfig(Config):
    isa: str = cfg_field(default='rv32imafdc', dump=True, desc=(
        "ISA string of the core"
    ))
    irq: str = cfg_field(default='riscv', dump=True, desc=(
        "Interrupt controller"
    ))
    fetch_enable: bool = cfg_field(default=False, dump=True, desc=(
        "True if the ISS should start executing instructins immediately, False if it will start "
        "after the fetch_enable signal starts it."
    ))
    boot_addr: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Address of the first instruction."
    ))
    hart_id: int = cfg_field(default=0, dump=True, desc=(
        "The core ID of the core simulated by the ISS."
    ))
    htif: bool = cfg_field(default=True, dump=True, desc=(
        "True if the ISS should start executing instructins immediately, False if it will start "
        "after the fetch_enable signal starts it."
    ))
    pmp: bool = cfg_field(default=False, dump=True, desc=(
        "True if the ISS should include the PMP."
    ))
    mmu: bool = cfg_field(default=False, dump=True, desc=(
        "True if the ISS should include the MMU."
    ))
    lsu_hold_on_async: bool = cfg_field(default=False, dump=True, desc=(
        "True if a data access answered asynchronously holds the core until its response "
        "(the response stands for the grant of a core which is only granted such an access "
        "when it completes, like a PULP core behind its demux for anything but the TCDM)."
    ))
    fetch_lookahead: bool = cfg_field(default=False, dump=True, desc=(
        "True if the line which follows the one being executed is fetched while its last "
        "instruction executes, so that a line which takes one cycle to come in costs "
        "nothing and a longer fetch one cycle less (RI5CY: the prefetch buffer asks for "
        "the next word before the decoder needs it; after a jump the target is only asked "
        "for when the jump is taken)."
    ))
    lsu_misaligned_store_stall: bool = cfg_field(default=False, dump=True, desc=(
        "True if a misaligned store keeps the execute stage one more cycle for its second "
        "half, so the next instruction starts one cycle later (RI5CY: the second access of a "
        "misaligned store is issued from EX while the next instruction waits in ID)."
    ))
    power_insn_groups: list[PowerSourceConfig] = cfg_field(default_factory=list, init=False, desc=(
        "Per-instruction-group dynamic energy tables; the list index matches the isa "
        "power group set with set_power_group. Empty = per-instruction power off."
    ))
    power: RiscvPowerConfig = cfg_field(default_factory=RiscvPowerConfig,
        init=False, desc=(
        "Named core power sources (stall_first, stall_next, background)."
    ))
