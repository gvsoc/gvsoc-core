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


from typing import Annotated, ClassVar

from config_tree import Config, cfg_field
from gvrun.runtime import Runtime
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


class RiscvVectorUnitConfig(Config):
    """Vector unit of the core, filled by pulp.ara.ara_v2.attach when the core
    has one."""

    _defer_parent_init: ClassVar[bool] = True

    nb_lanes: int = cfg_field(default=0, dump=True, desc=(
        "Number of vector lanes"
    ))
    nb_ipus: int = cfg_field(default=0, dump=True, desc=(
        "Number of integer units, 0 for one per lane"
    ))
    lane_width: int = cfg_field(default=0, dump=True, desc=(
        "Width in bytes of a lane"
    ))
    nb_ports: int = cfg_field(default=0, dump=True, desc=(
        "Number of memory ports of the Spatz vector LSU"
    ))
    nb_outstanding_reqs: int = cfg_field(default=0, dump=True, desc=(
        "Number of outstanding requests per memory port of the Spatz vector LSU"
    ))


class RiscvBinaryConfig(Config):
    """One binary simulated by the core."""

    file: str = cfg_field(default='', desc=(
        "Path of the ELF binary"
    ))


class RiscvArgConfig(Config):
    """One command-line argument given to the application through HTIF."""

    value: str = cfg_field(default='', desc=(
        "Argument"
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
    fetch_split_jump_stall: bool = cfg_field(default=False, dump=True, desc=(
        "True if an instruction split over two lines costs one more cycle when both lines are "
        "fetched for it, i.e. when it is reached by a jump (RI5CY: the second line is only asked "
        "for once the first one is in). Reached in sequence, the first line is already there."
    ))
    fetch_width: int = cfg_field(default=0, dump=True, desc=(
        "Bytes the core gets from its instruction cache per fetch, when narrower than the "
        "prefetch line. With fetch_split_jump_stall, a 32-bit instruction split over two "
        "such words also costs one more cycle when it is reached by a jump, even inside one "
        "line (RI5CY with a 32-bit instruction port: the second word is asked for once the "
        "first is in). 0 (default): the prefetch line size."
    ))
    scoreboard_waw_after_writeback: bool = cfg_field(default=False, dump=True, desc=(
        "True if an instruction which writes a register without reading it does not wait for a "
        "load whose value is already in that register and only waits out its load-use delay "
        "(RI5CY writes back in order: a write after a load is no hazard, only a read waits)."
    ))
    lsu_misaligned_store_stall: bool = cfg_field(default=False, dump=True, desc=(
        "True if a misaligned store keeps the execute stage one more cycle for its second "
        "half, so the next instruction starts one cycle later (RI5CY: the second access of a "
        "misaligned store is issued from EX while the next instruction waits in ID)."
    ))
    mtvec_writable: bool = cfg_field(default=True, dump=True, desc=(
        "False if software writes to mtvec are ignored, mtvec staying on the 256-byte page of "
        "the boot address (RI5CY built without PULP_SECURE)."
    ))
    lsu_elw_resume_cycles: int = cfg_field(default=-1, dump=True, desc=(
        "Cycles a p.elw event load costs after its response on top of a plain load, whether "
        "the core was clock-gated or not (RI5CY: the event load drops the busy signal and the "
        "pipeline is refilled once it is answered). -1 (default) keeps the older rule: only an "
        "elw which put the core to sleep pays, max(L + 1, 3) - L cycles for a sleep of L cycles."
    ))
    memory_start: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Start address of a memory whose array the core accesses directly, without going "
        "through its data port (io_v2 LSU only, the core meminfo port must be bound to the "
        "memory). Loads and stores to it are then untimed. Used with memory_size."
    ))
    memory_size: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Size of the memory accessed directly, see memory_start. 0 (default) disables the "
        "direct access."
    ))
    power_insn_groups: list[PowerSourceConfig] = cfg_field(default_factory=list, init=False, desc=(
        "Per-instruction-group dynamic energy tables; the list index matches the isa "
        "power group set with set_power_group. Empty = per-instruction power off."
    ))
    power: RiscvPowerConfig = cfg_field(default=None,
        init=False, desc=(
        "Named core power sources (stall_first, stall_next, background). None "
        "(all sources inert) without a power model."
    ))
    misa: int = cfg_field(default=0, init=False, fmt="hex", desc=(
        "Reset value of the misa CSR, set by the core generator from its ISA"
    ))
    has_double: bool = cfg_field(default=False, init=False, desc=(
        "True if the ISA has the D extension, set by the core generator from its ISA"
    ))
    cluster_id: int = cfg_field(default=0, init=False, desc=(
        "Cluster ID of the core, put in the upper bits of mhartid"
    ))
    debug_handler: int = cfg_field(default=0, init=False, fmt="hex", desc=(
        "Address of the debug handler"
    ))
    regfile_fi: bool = cfg_field(default=False, init=False, desc=(
        "True if the register files are fault-injected"
    ))
    prefetcher_fi: bool = cfg_field(default=False, init=False, desc=(
        "True if the prefetcher buffer is fault-injected"
    ))
    vu: RiscvVectorUnitConfig = cfg_field(default_factory=RiscvVectorUnitConfig,
        init=False, desc=(
        "Vector unit, when the core has one"
    ))
    binaries: Annotated[list[RiscvBinaryConfig], Runtime] = cfg_field(default_factory=list,
        init=False, desc=(
        "Binaries simulated by the core, for the debug symbols of the traces"
    ))
    args: Annotated[list[RiscvArgConfig], Runtime] = cfg_field(default_factory=list,
        init=False, desc=(
        "Command-line arguments given to the application through HTIF"
    ))
    htif_tohost: Annotated[int, Runtime] = cfg_field(default=0, init=False, fmt="hex", desc=(
        "Address of the HTIF tohost symbol of the binary, 0 if none"
    ))
    htif_fromhost: Annotated[int, Runtime] = cfg_field(default=0, init=False, fmt="hex", desc=(
        "Address of the HTIF fromhost symbol of the binary, 0 if none"
    ))
