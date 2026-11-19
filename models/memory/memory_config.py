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

"""Memory configuration module.

This module provides configuration classes for memory components in the system,
including settings for size, atomic operations, and latency.
"""

from __future__ import annotations

from typing import Annotated, ClassVar

from config_tree import Config, cfg_field, HasSize
from gvrun.runtime import Runtime
from vp.power_config import PowerSourceConfig


class MemoryPowerConfig(Config):
    """Power tables of the memory: one per-access energy quantum per access
    kind, plus a background source (dynamic + leakage). Filled from the YAML
    file named by the ``power_model`` field of :class:`MemoryConfig`; untouched
    sources stay inert."""

    _defer_parent_init: ClassVar[bool] = True

    background: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Background power (dynamic + leakage), started at reset"))
    read_8: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 8-bit reads"))
    read_16: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 16-bit reads"))
    read_32: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 32-bit (and wider) reads"))
    write_8: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 8-bit writes"))
    write_16: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 16-bit writes"))
    write_32: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig, desc=(
        "Per-access energy of 32-bit (and wider) writes"))


class MemoryConfig(Config, HasSize):
    """Configuration for memory components.

    This class defines the configuration parameters for memory components in the system,
    like size and atomic operation support.

    Attributes
    ----------
    size: int
        The size of the memory in bytes.
    atomics: bool
        True if the memory should support riscv atomics. Since this is slowing down the model, it
        should be set to True only if needed.
    latency: int
        Specify extra latency which will be added to any incoming request.
    """

    size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Memory size in bytes"
    ))

    atomics: bool = cfg_field(default=False, dump=True, desc=(
        "True if the memory should support riscv atomics. "
        "Since this is slowing down the model, it should be "
        "set to True only if needed"
    ))

    # Memory latency is 1 by default to model the fact that the memory always reply in the next
    # cycle
    latency: int = cfg_field(default=1, dump=True, desc=(
        "Specify extra latency which will be added to any "
        "incoming request"
    ))

    truncate: int = cfg_field(default=True, dump=True, desc=(
        "If true, this truncates the global input address with the memory size to make it relative "
        "to the memory"
    ))

    width_bits: int = cfg_field(default=-1, dump=True, desc=(
        "Log2 of the bandwidth to the memory, in bytes per cycle. No bandwidth timing model "
        "if -1"
    ))

    align: int = cfg_field(default=0, dump=True, desc=(
        "Alignment in bytes requested from aligned_alloc for the backing buffer, 0 for none"
    ))

    check: bool = cfg_field(default=False, dump=True, desc=(
        "Allocate a bitmap for tracking uninitialised accesses"
    ))

    init: bool = cfg_field(default=True, dump=True, desc=(
        "True to fill the backing buffer with 0x57 bytes at construction"
    ))

    # Runtime: the resolved path differs between the build-time generation
    # (source tree) and the run-time one (installed generators).
    stim_file: Annotated[str, Runtime] = cfg_field(default="", dump=True, desc=(
        "Path to a raw-binary file preloaded into the memory (empty for none)"
    ))

    power_trigger: bool = cfg_field(default=False, dump=True, desc=(
        "Enable power-capture start/stop triggers on magic writes to offset 0"
    ))

    truncate_size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "If non-zero, incoming addresses are masked with (truncate_size - 1). Only used by "
        "memory.memory"
    ))

    fic_enabled: bool = cfg_field(default=False, dump=True, desc=(
        "True if this memory is fault injected. Only used by memory.memory"
    ))

    power_model: str = cfg_field(default="", desc=(
        "Path of a YAML power model file, resolved from PYTHONPATH, applied onto the "
        "'power' tables. Empty for no power numbers"
    ))

    power: MemoryPowerConfig = cfg_field(default_factory=MemoryPowerConfig, init=False, desc=(
        "Per-source power tables, filled from 'power_model'"
    ))
