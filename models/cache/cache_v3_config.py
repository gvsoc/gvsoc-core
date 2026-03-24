# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Cache configuration module.

This module provides configuration classes for cache components in the system,
including settings for size, associativity, and refill behavior.
"""
from dataclasses import dataclass
from config_tree import Config, cfg_field

class CacheConfig(Config):
    """Configuration for cache components.
    This class defines the configuration parameters for cache components in the system,
    such as size, line size, associativity, and refill behavior.

    Attributes
    ----------
    size: int
        The total size of the cache in bytes.
    line_size: int
        The size of a single cache line in bytes.
    ways: int           The number of ways (associativity) of the cache.
    enabled: bool       True if the cache is enabled at reset.
    refill_shift: int
        Shift applied to the address when issuing a refill request to the next
        memory level.
    refill_offset: int
        Offset added to the address when issuing a refill request to the next
        memory level.
    refill_latency: int
        Latency in cycles for a refill request to the next memory level.
    """

    size: int = cfg_field(default=0, dump=True, desc=(
        "Total cache size in bytes"
    ))

    line_size: int = cfg_field(default=16, dump=True, desc=(
        "Cache line size in bytes"
    ))

    ways: int = cfg_field(default=1, dump=True, desc=(
        "Number of ways (associativity) of the cache"
    ))

    enabled: bool = cfg_field(default=True, dump=True, desc=(
        "True if the cache is enabled at reset"
    ))

    refill_shift: int = cfg_field(default=0, dump=True, desc=(
        "Shift applied to the address when issuing a refill request to the next memory level"
    ))

    refill_offset: int = cfg_field(default=0, dump=True, desc=(
        "Offset added to the address when issuing a refill request to the next memory level"
    ))

    refill_latency: int = cfg_field(default=0, dump=True, desc=(
        "Latency in cycles for a refill request to the next memory level"
    ))
