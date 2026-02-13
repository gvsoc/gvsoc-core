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

from dataclasses import dataclass
from gvrun.config import Config, cfg_field


@dataclass(repr=False)
class MemoryConfig(Config):
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

    latency: int = cfg_field(default=0, dump=True, desc=(
        "Specify extra latency which will be added to any "
        "incoming request"
    ))
