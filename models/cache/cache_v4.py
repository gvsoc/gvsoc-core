# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 set-associative cache component.

This module provides the ``Cache`` generator for the io_v2 port of the cache
model (``cache_v4.cpp``). The configuration struct ``CacheConfig`` is shared
with the v3 variant and defined here inline — fields match cache_v3.py.
"""

from __future__ import annotations

from typing_extensions import override

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.gui import Signal


class CacheConfig(Config):
    """Configuration for the io_v2 cache component.

    Attributes
    ----------
    size : int
        Total cache size in bytes.
    line_size : int
        Cache line size in bytes.
    ways : int
        Number of ways (associativity).
    enabled : bool
        True if the cache is enabled at reset.
    refill_shift : int
        Shift applied to the address when issuing a refill request to the next
        memory level.
    refill_offset : int
        Offset added to the address when issuing a refill request to the next
        memory level.
    refill_latency : int
        Latency in cycles added to synchronous refill completions.
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
    """Set-associative cache on the io_v2 protocol.

    Behavioural port of :class:`cache.cache_v3.Cache` to the io_v2 IO
    interface. Replacement policy (pseudo-random 8-bit LFSR), refill
    serialisation, and flush / enable semantics are unchanged; only the
    IO-side plumbing is new:

    - input and refill ports carry the ``io_v2`` signature
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
    - error reporting is via ``req->set_resp_status(IO_RESP_INVALID)`` +
      ``IO_REQ_DONE``
    - replies travel back on the cache's own slave port (no v1
      ``resp_port`` indirection)

    Request flow
    ~~~~~~~~~~~~

    - **Hit**: the cache reads/writes the line's byte buffer and returns
      ``IO_REQ_DONE`` inline. If the line has just been filled and its
      ``timestamp`` is still in the future, the access is stalled via
      ``req->inc_latency(timestamp - now)`` so the master paces itself.
    - **Miss, no refill pending**: a refill request is sent to the
      ``REFILL`` port. A synchronous ``DONE`` from the downstream yields
      an inline ``DONE`` on the input (with accumulated latency); a
      ``GRANTED`` parks the CPU request and forwards the response later
      via the master's ``resp()`` callback on our slave port.
    - **Miss, refill pending**: the CPU request is queued and ack'd as
      ``GRANTED``; it is resumed by ``fsm_handler`` once the current
      refill lands.
    - **Bypass** (cache disabled): the CPU request is forwarded verbatim
      through ``REFILL`` after the address is rewritten by
      ``refill_shift`` / ``refill_offset``. Both sync and async target
      responses are bounced back on the input slave port.
    - **Refill DENIED**: the cache propagates ``DENIED`` to the master and
      remembers that it owes an ``input_itf.retry()`` once its own refill
      port retries.

    Control wires
    ~~~~~~~~~~~~~

    - **ENABLE**  — enables / disables the cache at runtime; while disabled,
      accesses take the bypass path described above.
    - **FLUSH**   — pulse to invalidate every line; acknowledged on
      ``FLUSH_ACK``.
    - **FLUSH_LINE** / **FLUSH_LINE_ADDR** — latch an address then pulse to
      invalidate just that line.

    Configuration fields (:class:`CacheConfig`):

    .. csv-table::
       :header: Field, Meaning
       :widths: auto

       ``size``,           "Total cache size in bytes"
       ``line_size``,      "Line size in bytes (power of two)"
       ``ways``,           "Associativity; sets = size / ways / line_size"
       ``enabled``,        "Cache enabled at reset"
       ``refill_shift``,   "Left-shift applied to addr before refill forward"
       ``refill_offset``,  "Added to addr after the shift"
       ``refill_latency``, "Extra cycles stacked on synchronous refills"

    Parameters
    ----------
    parent : Component
        Parent component this cache is instantiated under.
    name : str
        Local name of the cache within ``parent``. Used for component
        path, traces, and as the lookup key for bindings.
    config : CacheConfig
        All cache parameters live here. The instance is taken over by the
        cache — pass a fresh :class:`CacheConfig` per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'Cache (v4)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/cache/cache_v4',
             'component': 'cache.cache_v4'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: CacheConfig):

        super(Cache, self).__init__(parent, name, config=config)
        self.add_sources(['cache/cache_v4.cpp'])

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

        Incoming io_v2 requests land here. Hits are served inline; misses
        trigger a refill via the ``REFILL`` master port.
        """
        return SlaveItf(self, 'input', signature='io_v2')

    def i_FLUSH(self) -> SlaveItf:
        """Returns the flush slave port.

        A ``True`` pulse on this wire invalidates the entire cache and then
        pulses the ``FLUSH_ACK`` master.
        """
        return SlaveItf(self, 'flush', signature='wire<bool>')

    def i_FLUSH_LINE(self) -> SlaveItf:
        """Returns the flush-line slave port.

        A ``True`` pulse invalidates the single line whose address was
        previously latched via ``FLUSH_LINE_ADDR``.
        """
        return SlaveItf(self, 'flush_line', signature='wire<bool>')

    def i_FLUSH_LINE_ADDR(self) -> SlaveItf:
        """Returns the flush-line-address slave port.

        Latches the address used by the next ``FLUSH_LINE`` pulse.
        """
        return SlaveItf(self, 'flush_line_addr', signature='wire<uint32_t>')

    def i_ENABLE(self) -> SlaveItf:
        """Returns the enable slave port.

        Writing ``True`` enables the cache, ``False`` disables it. While
        disabled every incoming request is forwarded verbatim through the
        ``REFILL`` port (address transformed by ``refill_shift`` /
        ``refill_offset``).
        """
        return SlaveItf(self, 'enable', signature='wire<bool>')

    def o_FLUSH_ACK(self, itf: SlaveItf):
        """Binds the flush-ack master port.

        Pulsed with ``True`` once a full flush completes.
        """
        self.itf_bind('flush_ack', itf, signature='wire<bool>')

    def o_REFILL(self, itf: SlaveItf):
        """Binds the refill master port to the next memory level.

        The address sent downstream is transformed according to the
        configured ``refill_shift`` and ``refill_offset``.
        """
        self.itf_bind('refill', itf, signature='io_v2')

    @override
    def gen_gui(self, parent_signal: Signal):
        """Generate GUI trace signals mirroring cache_v3."""
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
