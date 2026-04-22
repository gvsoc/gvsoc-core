# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from __future__ import annotations

import sys
from typing import ClassVar

import gvsoc.systree
from config_tree import Config, cfg_field, HasSize


# Router implementation kinds. Each maps to a different C++ source.
KIND_UNTIMED      = 'untimed'
KIND_BANDWIDTH    = 'bandwidth'
KIND_BACKPRESSURE = 'backpressure'
KIND_BEAT         = 'beat'

_SOURCES = {
    KIND_UNTIMED:      'interco/router/router_v2_untimed.cpp',
    KIND_BANDWIDTH:    'interco/router/router_v2_bandwidth.cpp',
    KIND_BACKPRESSURE: 'interco/router/router_v2_backpressure.cpp',
    KIND_BEAT:         'interco/router/router_v2_beat.cpp',
}


class RouterMapping(Config, HasSize):
    """One address-range entry inside a :class:`RouterConfig`.

    The router applies ``addr - remove_offset + add_offset`` before forwarding
    the request to the master port named ``name``.

    Attributes
    ----------
    base : int
        Start of the mapped address range.
    size : int
        Range size in bytes; ``0`` makes this entry the catch-all.
    remove_base : bool
        Convenience flag honoured by :meth:`Router.o_MAP`: when ``True``
        (the default), ``remove_offset`` defaults to ``base`` so the
        request address is rebased to the downstream port. Set to
        ``False`` for pass-through routing (e.g. router-to-router cascades
        that should preserve the original address).
    remove_offset : int
        Subtracted from the request address before forwarding.
    add_offset : int
        Added to the request address before forwarding.
    latency : int
        Per-mapping latency in cycles, applied on top of the per-router
        ``latency``. Used by the bandwidth and backpressure variants.
    is_error : bool
        Marks this mapping as the error sink — requests resolving to it are
        responded with ``IO_RESP_INVALID``.
    """

    _defer_parent_init: ClassVar[bool] = True

    base: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Base address of the mapping"
    ))

    size: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Size of the mapping"
    ))

    remove_base: bool = cfg_field(default=True, dump=True, desc=(
        "Remove the base address when a request is propagated using this mapping. "
        "Defaults to True (rebase to downstream port); set False for pass-through routing."
    ))

    remove_offset: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Subtracted from the request address before forwarding. Set automatically "
        "from ``base`` when ``remove_base`` is true."
    ))

    add_offset: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Added to the request address before forwarding."
    ))

    latency: int = cfg_field(default=0, dump=True, desc=(
        "Global latency applied to all incoming requests. This impacts the start "
        "time of the burst."
    ))

    is_error: bool = cfg_field(default=False, dump=True, desc=(
        "If true, this mapping is treated as an error sink: requests resolving to "
        "it are responded with IO_RESP_INVALID."
    ))


class RouterConfig(Config, HasSize):
    """Configuration class for the io_v2 :class:`Router`.

    Carries every parameter the router needs — implementation flavour
    (``kind``), per-router timing and topology (``latency``, ``bandwidth``,
    ``shared_rw_channel``, ``width``, ``max_input_pending_size``,
    ``max_pending_bursts``, ``nb_input_port``), and the address mappings list.

    A single template can be reused across several routers: ``Router``'s
    constructor copies the scalar fields into a per-instance config so each
    router keeps its own mappings list and its own dynamic ``nb_input_port``
    counter.

    Attributes
    ----------
    kind : str
        Implementation flavour. One of ``'untimed'``, ``'bandwidth'``
        (default), ``'backpressure'``, ``'beat'``.
    synchronous : bool
        Hint for v1 compatibility — the v2 sources do not actually read it.
    shared_rw_channel : bool
        ``True``  → single channel shared between read and write traffic
        (R and W compete for the same per-cycle slot).
        ``False`` (default) → two independent channels, doubling effective
        throughput when traffic is balanced. Read by the bandwidth,
        backpressure and beat variants.
    max_input_pending_size : int
        Per-input FIFO depth in bytes. Used by the beat variant.
    bandwidth : int
        Bytes per cycle the router can forward. Used by the bandwidth and
        backpressure variants.
    latency : int
        Per-router latency in cycles, applied on every request on top of any
        per-mapping latency. Used by the bandwidth and backpressure variants.
    width : int
        Beat width in bytes. Required by the beat variant (beats with
        ``size > width`` are rejected as ``IO_RESP_INVALID``).
    max_pending_bursts : int
        Burst-table capacity. Used by the beat variant. ``0`` means "one slot
        per input port".
    nb_input_port : int
        Number of input ports the router exposes. Grown on demand by
        :meth:`Router.i_INPUT`.
    mappings : list[RouterMapping]
        Address mappings owned by this router. Populated via
        :meth:`add_mappings` (or :meth:`Router.o_MAP` / :meth:`Router.add_mapping`).
    """

    kind: str = cfg_field(default=KIND_BANDWIDTH, dump=True, desc=(
        "Router implementation flavour: 'untimed', 'bandwidth', 'backpressure' or 'beat'."
    ))
    synchronous: bool = cfg_field(default=True, dump=True, desc=(
        "True if the router should use synchronous mode where all incoming requests are handled as "
        "far as possible in synchronous IO mode."
    ))
    shared_rw_channel: bool = cfg_field(default=False, dump=True, desc=(
        "True if the router has a single channel shared between read and write traffic "
        "(reads and writes compete for the same per-cycle slot). False (default) gives two "
        "independent channels, effectively doubling the router throughput when read and write "
        "traffic is balanced."
    ))
    max_input_pending_size: int = cfg_field(default=0, dump=True, desc=(
        "Size of the FIFO for each input. Only valid for asynchronous mode and only when input "
        "packet size is smaller or equal to the bandwidth."
    ))
    bandwidth: int = cfg_field(default=0, dump=True, desc=(
        "Global bandwidth, in bytes per cycle, applied to all incoming request. This impacts the "
        "end time of the burst."
    ))
    latency: int = cfg_field(default=0, dump=True, desc=(
        "Per-router latency in cycles applied to every request, on top of any per-mapping latency."
    ))
    width: int = cfg_field(default=0, dump=True, desc=(
        "Beat width in bytes (only used by the beat-streaming variant)."
    ))
    max_pending_bursts: int = cfg_field(default=0, dump=True, desc=(
        "Burst-table capacity (only used by the beat-streaming variant)."
    ))
    nb_input_port: int = cfg_field(default=1, dump=True, desc=(
        "Number of input ports the router exposes."
    ))

    mappings: list["RouterMapping"] = cfg_field(default_factory=list, init=False, desc=(
        "List of address mappings for the router"
    ))

    def add_mappings(self, *mappings: "RouterMapping"):
        for mapping in mappings:
            mapping.adopt(self)
            self.mappings.append(mapping)
        return self

    @property
    def size(self):
        min_addr = sys.maxsize
        max_addr = 0
        for mapping in self.mappings:
            if not mapping.size:
                continue
            if mapping.base < min_addr:
                min_addr = mapping.base
            if mapping.base + mapping.size > max_addr:
                max_addr = mapping.base + mapping.size
        return max_addr - min_addr


class Router(gvsoc.systree.Component):
    """Address-decoding router on the io_v2 protocol.

    Four implementations are available via the ``kind`` field on the
    :class:`RouterConfig`:

    - **'untimed'**: zero timing. Address decode + forward, nothing else. Fastest.
      Use for pure functional simulation or when timing is modeled elsewhere.
    - **'bandwidth'** (default): rate-limited, non-blocking. Requests are always
      accepted; latency grows with the per-input/per-output bandwidth backlog and
      is reported via ``req->latency`` (no ClockEvent scheduling). A master can
      pipeline an arbitrary number of outstanding requests without handshaking.
    - **'backpressure'**: rate-limited, blocking. Same bandwidth model, but the
      router returns ``IO_REQ_DENIED`` while a previous request from the same input
      is still in its delayed-forward window. The master must wait for ``retry()``.
    - **'beat'**: beat-streaming, AXI-like. Each IoReq is one beat of size <=
      ``width`` bytes. Bursts are tracked by ``burst_id`` (remapped through a
      fixed per-router burst table). Multiple inputs compete for outputs via
      round-robin arbitration with burst atomicity.

    Which fields each kind actually reads:

    .. csv-table::
       :header: Field, untimed, bandwidth, backpressure, beat
       :widths: auto

       ``latency``,                –, yes, yes, –
       ``bandwidth``,              –, yes, yes, –
       ``shared_rw_channel``,      –, yes, yes, yes
       ``width``,                  –, –,   –,   yes
       ``max_input_pending_size``, –, –,   –,   yes
       ``max_pending_bursts``,     –, –,   –,   yes

    Fields not used by the selected kind are still packed into the compiled
    config struct, but the C++ model simply ignores them.

    Parameters
    ----------
    parent : gvsoc.systree.Component
        Parent component this router is instantiated under.
    name : str
        Local name of the router within ``parent``. Used for component path,
        traces, and as the lookup key for bindings.
    config : RouterConfig
        All router parameters live here. The instance is taken over by the
        router — :meth:`add_mapping` / :meth:`o_MAP` append to its
        ``mappings`` list and :meth:`i_INPUT` grows ``nb_input_port`` —
        so each router needs its own ``RouterConfig``.
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 config: RouterConfig):
        if config.kind not in _SOURCES:
            raise ValueError(
                f"Router kind must be one of {list(_SOURCES)}, got {config.kind!r}")

        # ``config`` is owned by this router from now on: ``add_mapping`` /
        # ``o_MAP`` will append to ``config.mappings`` and ``i_INPUT`` will
        # grow ``config.nb_input_port``. Pass a fresh ``RouterConfig`` per
        # router instance — sharing one template across several would mix
        # their mappings and input-port counts.
        self.config = config
        super(Router, self).__init__(parent, name, config=self.config)
        self.add_sources([_SOURCES[config.kind]])

    def add_mapping(self, name: str, base: int = 0, size: int = 0,
                    remove_offset: int = 0, add_offset: int = 0, latency: int = 0,
                    is_error: bool = False):
        """Append a mapping entry to ``self.config.mappings``.

        Lower-level than ``o_MAP``: only registers the mapping in the config,
        does not bind any port. Use ``o_MAP`` instead when wiring an actual
        downstream component — it calls this and then ``itf_bind`` together.

        On every forwarded request, the router translates the address as
        ``addr - remove_offset + add_offset`` before handing it to the
        downstream port named ``name``.

        Parameters
        ----------
        name : str
            Mapping name. Also becomes the master port name on the C++ side
            (must match the slave port the mapping is bound to). The literal
            string ``"error"`` is recognised as the error mapping (any
            request resolving to it is responded with ``IO_RESP_INVALID``).
        base : int, optional
            Start of the address range covered by this mapping.
        size : int, optional
            Range size in bytes. ``0`` means open-ended (the entry becomes
            the catch-all "default" branch of the mapping tree).
        remove_offset : int, optional
            Subtracted from the request address before forwarding. Defaults
            to 0; ``o_MAP(rm_base=True)`` sets this to ``base``.
        add_offset : int, optional
            Added to the request address before forwarding. Defaults to 0.
        latency : int, optional
            Per-mapping latency in cycles, added on top of the per-router
            ``latency``. Used by ``'bandwidth'`` and ``'backpressure'``.
        is_error : bool, optional
            Marks this mapping as the error sink. ``True`` is also implied
            when ``name == 'error'`` for backward compatibility.
        """
        self.config.add_mappings(RouterMapping(
            name=name,
            base=base,
            size=size,
            remove_offset=remove_offset,
            add_offset=add_offset,
            latency=latency,
            is_error=is_error or name == 'error',
        ))

    def __alloc_input_port(self, id):
        if id >= self.config.nb_input_port:
            self.config.nb_input_port = id + 1

    def i_INPUT(self, id: int = 0) -> gvsoc.systree.SlaveItf:
        """Return a slave interface for input port ``id``.

        Each call grows the router's input-port count if needed: the C++
        side allocates ``cfg.nb_input_port`` slave ports at construction,
        named ``input`` for id 0 and ``input_<N>`` for id >= 1.

        Parameters
        ----------
        id : int, optional
            Input port index, 0-based. Defaults to 0.

        Returns
        -------
        gvsoc.systree.SlaveItf
            The ``io_v2`` slave interface to bind a master output to.
        """
        self.__alloc_input_port(id)
        if id == 0:
            return gvsoc.systree.SlaveItf(self, 'input', signature='io_v2')
        return gvsoc.systree.SlaveItf(self, f'input_{id}', signature='io_v2')

    def o_MAP(self, itf: gvsoc.systree.SlaveItf, mapping: RouterMapping,
              name: str = None):
        """Attach a :class:`RouterMapping` to this router and bind a master
        output port to ``itf``.

        ``mapping`` is taken over by the router: it is appended to
        ``self.config.mappings`` and adopted as a child of the config, so a
        fresh ``RouterMapping`` must be passed for each ``o_MAP`` call.
        The master port name on the C++ side is ``mapping.name`` after this
        call returns (possibly overridden by the ``name`` kwarg or defaulted
        to ``itf.component.name``).

        Parameters
        ----------
        itf : gvsoc.systree.SlaveItf
            Downstream slave interface to bind to.
        mapping : RouterMapping
            All mapping parameters live here (``base``, ``size``,
            ``remove_base``, ``remove_offset``, ``add_offset``, ``latency``,
            ``is_error``).
        name : str, optional
            Override for ``mapping.name``. When neither this kwarg nor
            ``mapping.name`` was set, the binding defaults to
            ``itf.component.name``.
        """
        if name is not None:
            mapping.name = name
        elif not mapping.name:
            mapping.name = itf.component.name
        if mapping.remove_base and mapping.remove_offset == 0:
            mapping.remove_offset = mapping.base
        if mapping.name == 'error':
            mapping.is_error = True
        self.config.add_mappings(mapping)
        self.itf_bind(mapping.name, itf, signature='io_v2')
