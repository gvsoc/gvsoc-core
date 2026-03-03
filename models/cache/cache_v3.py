# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Cache component module.
This module provides the Cache component for GVSOC, implementing a configurable
set-associative cache with support for flushing, enabling/disabling, and refill
to the next memory level. The cache uses a pseudo-random LRU replacement policy.
"""
from typing_extensions import override
from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.gui import Signal

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

class Cache(Component):
    """Set-associative cache component.

    This component models a configurable set-associative cache with support for
    cache line refill to the next memory level, flushing (full cache or
    individual lines), and runtime enable/disable. The replacement policy is
    based on a pseudo-random LFSR.
    The cache is parameterized through a :class:`CacheConfig` object which
    controls the total size, line size, associativity, and refill behavior.
    Attributes
    ----------
    config : CacheConfig
        The cache configuration.

    """
    def __init__(self, parent: Component, name: str, config: CacheConfig):

        super(Cache, self).__init__(parent, name, config=config)
        self.add_sources(['cache/cache_v3.cpp'])

        self.add_properties({
            'size': config.size,
            'ways': config.ways,
            'line_size': config.line_size,
            'refill_latency': config.refill_latency,
            'refill_offset': config.refill_offset,
            'refill_shift': config.refill_shift,
            'enabled': config.enabled,
        })

    def i_INPUT(self) -> SlaveItf:
        """Returns the input slave port for memory access requests.

        This port accepts IO requests (reads and writes) from the upstream
        component. On a cache hit the request is served directly; on a miss
        a refill is triggered through the ``REFILL`` master port.
        Returns
        -------
        SlaveItf
            The input slave interface with ``io`` signature.
        """
        return SlaveItf(self, 'input', signature='io')

    def i_FLUSH(self) -> SlaveItf:
        """Returns the flush slave port.

        When a ``True`` pulse is received on this port the entire cache is
        invalidated and a flush acknowledgment is sent back through the
        ``FLUSH_ACK`` master port.
        Returns
        -------
        SlaveItf
            The flush slave interface with ``wire<bool>`` signature.
        """
        return SlaveItf(self, 'flush', signature='wire<bool>')

    def o_FLUSH_ACK(self, itf: SlaveItf):
        """Binds the flush acknowledgment master port.

        This wire is pulsed with ``True`` once a flush operation initiated
        through the ``FLUSH`` slave port has completed.

        Parameters
        ----------
        itf : SlaveItf
            The slave interface to bind to, with ``wire<bool>`` signature.
        """
        self.itf_bind('flush_ack', itf, signature='wire<bool>')

    def o_REFILL(self, itf: SlaveItf):
        """Binds the refill master port to the next memory level.

        On a cache miss the component issues a refill request through this
        port. The address sent is transformed according to the configured
        ``refill_shift`` and ``refill_offset`` parameters.

        Parameters        ----------
        itf : SlaveItf
            The slave interface of the next memory level, with ``io``
            signature.
        """
        self.itf_bind('refill', itf, signature='io')

    @override
    def gen_gui(self, parent_signal: Signal):
        """Generate GUI trace signals for cache visualization.

        Creates a signal hierarchy that exposes the cache request address,
        refill address, and per-way / per-set tag state for interactive
        debugging in the GVSOC GUI.

        Parameters
        ----------
        parent_signal : Signal
            The parent GUI signal under which this cache's signals are
            registered.
        """
        cache = Signal(self, parent_signal, name=self.name, path='req_addr',
            groups=['cache', 'active'])
        _ = Signal(self, cache, name='refill', path='refill_addr', groups=['cache'])
        tags = Signal(self, cache, name='tags')

        ways = self.get_property('ways')
        size = self.get_property('size')
        line_size = self.get_property('line_size')

        for way in range(0, ways):
            set_trace = Signal(self, tags, name=f'set_{way}')
            for line in range(0, int(size / line_size / ways)):
                _ = Signal(self, set_trace, name=f'line_{line}', path=f'set_{way}/line_{line}',
                    groups=['cache'])
