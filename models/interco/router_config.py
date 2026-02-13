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

"""
Router configuration and mapping classes for the gvrun framework.

This module provides configuration classes for defining router behavior and address mappings
in the simulation environment.
"""

from dataclasses import dataclass
from gvrun.config import Config, cfg_field

@dataclass(repr=False)
class RouterConfig(Config):
    """
    Configuration class for router behavior.

    This class defines the operational parameters for a router component, including
    synchronization mode, buffering, and bandwidth settings.

    Attributes:
    ----------
    synchronous: bool
        True if the router should use synchronous mode where all incoming
        requests are handled as far as possible in synchronous IO mode.
    max_input_pending_size: int
        Size of the FIFO for each input. Only valid for
        asynchronous mode and only when input packet size is smaller or equal
        to the bandwidth.
    bandwidth: int
        Global bandwidth, in bytes per cycle, applied to all incoming
        request. This impacts the end time of the burst.
    """

    synchronous: bool = cfg_field(default=False, dump=True, desc=(
        "True if the router should use synchronous mode where all incoming requests are handled as "
        "far as possible in synchronous IO mode."
    ))

    max_input_pending_size: int = cfg_field(default=0, dump=True, desc=(
        "Size of the FIFO for each input. Only valid for asynchronous mode and only when input "
        "packet size is smaller or equal to the bandwidth."
    ))

    bandwidth: int = cfg_field(default=0, dump=True, desc=(
        "Global bandwidth, in bytes per cycle, applied to all incoming request. This impacts the "
        "end time of the burst."
    ))

@dataclass(repr=False)
class RouterMapping(Config):
    """
    Configuration class for router address mapping.

    This class defines how address ranges are mapped through the router, including
    address translation and timing parameters.

    Attributes:
    ----------
    base: int
        Base address of the mapping (in hexadecimal).
    size: int
        Size of the mapping (in hexadecimal).
    remove_base: bool
        Remove the base address when a request is propagated using this mapping.
    latency: int
        Global latency applied to all incoming requests. This impacts
        the start time of the burst.
    """

    base: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Base address of the mapping"
    ))

    size: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Size of the mapping"
    ))

    remove_base: bool = cfg_field(default=True, dump=True, desc=(
        "Remove the base address when a request is propagated using this mapping"
    ))

    latency: int = cfg_field(default=0, dump=True, desc=(
        "Global latency applied to all incoming requests. This impacts the start time of the burst."
    ))
