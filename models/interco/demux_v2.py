# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 bit-field address demultiplexer component.

This module provides the ``Demux`` generator for the io_v2 port of the
demultiplexer model (``demux_v2.cpp``). The configuration struct is the
same as the v1 variant and defined here inline — fields match
``demux.py``.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf


class DemuxConfig(Config):
    """Configuration for the io_v2 demux component.

    Attributes
    ----------
    offset : int
        Bit position of the first bit of the address field used to select
        an output port. For example, ``offset=12`` ignores the bottom 12
        bits of the address before extracting the target index — useful
        when the downstreams are placed on 4 KiB boundaries.
    width : int
        Number of contiguous bits (starting at ``offset``) used to
        select an output port. The demux exposes exactly ``1 << width``
        output ports, named ``output_0`` through
        ``output_{2**width - 1}``.
    """

    offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "First bit used to extract the target index"
    ))
    width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Number of bits used to extract the target index"
    ))


class Demux(Component):
    """Bit-field address demultiplexer on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A trivial 1-to-N passthrough that routes every incoming io_v2 request
    to exactly one of its downstream output ports. The output index is
    extracted from a fixed bit window of the request address:

    .. code-block:: text

        output_id = (addr >> offset) & ((1 << width) - 1)

    The demux is purely functional and **stateless** — it holds no
    buffer, does not modify the address or data, does not reorder, does
    not add or subtract latency, and does not make any arbitration
    decisions. It is typically used to split a single address space
    among bank-like or channel-like downstreams (e.g. L2 SRAM banks,
    peripheral sub-regions, SIMD lane memories) where the choice is
    determined statically by an address bit field.

    This is the io_v2 port of :class:`interco.demux.Demux`. Only the
    IO-side plumbing is new:

    - input and output ports carry the ``io_v2`` signature
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
    - responses from any output travel back via the demux's own input
      slave port using ``input_itf.resp(req)`` — no v1 ``resp_port``
      indirection
    - back-pressure uses the AXI-like ``retry()`` handshake: the demux
      propagates ``DENIED`` upstream and relays ``retry()`` on the
      input only when the specific output that denied us becomes
      ready again

    Address decoding
    ~~~~~~~~~~~~~~~~

    :attr:`DemuxConfig.offset` and :attr:`DemuxConfig.width` define a
    contiguous bit slice of the input address:

    ======================  ===============================================
    ``offset``              LSB of the selector (bits below are ignored)
    ``width``               Number of selector bits
    ``1 << width``          Number of output ports created
    ``(addr >> offset) & ((1 << width) - 1)``  Output index
    ======================  ===============================================

    The address is **not rewritten** before forwarding — the full,
    original address is handed to the chosen output. If the downstreams
    need to see a bank-local address, insert a remap (address offset
    subtract) on each output branch.

    Bits outside the selector (bits below ``offset`` and bits above
    ``offset + width``) do not participate in routing. A demux with
    ``offset=12, width=2`` therefore partitions the address space into
    four 4 KiB-aligned groups, each group interleaved at 16 KiB stride.

    Request flow
    ~~~~~~~~~~~~

    - **Incoming request**: the demux extracts the output index and
      forwards the request verbatim. Whatever status the downstream
      returns (``IO_REQ_DONE``, ``IO_REQ_GRANTED``, ``IO_REQ_DENIED``)
      is propagated to the upstream master.
    - **Deferred completion**: if the selected output returns
      ``IO_REQ_GRANTED``, the downstream will eventually call
      ``resp()`` on the demux's master port. The demux relays that
      call on its own slave port, so the upstream master sees a normal
      asynchronous response.
    - **Downstream DENY**: if the output returns ``IO_REQ_DENIED``, the
      demux returns ``IO_REQ_DENIED`` upstream and latches the output
      id in ``denied_output``. The upstream master is expected to hold
      the request until the demux calls ``retry()``.
    - **Output retry**: when the latched output fires ``retry()`` the
      demux relays the retry on the input. Retries from **other**
      outputs are ignored (a ready-state toggle on an output unrelated
      to the denied request does not concern the upstream — acting on
      it would cause a pointless round-trip of DENY/RETRY cycles).

    Timing model
    ~~~~~~~~~~~~

    The demux is a **big-packet, zero-latency** io_v2 model: it never
    touches ``req->latency`` and never schedules a ClockEvent. Forward
    and response latency are whatever the downstream decides to report
    — a sync downstream that annotates latency on ``IO_REQ_DONE`` is
    observed directly by the upstream; an async downstream's ``resp()``
    wall-clock is observed directly. The demux adds no cycle.

    This makes the model a true "wire with a selector" — its only cost
    is a couple of bit operations on the address. If you need any
    timing effect (per-target latency, arbitration, bandwidth
    shaping), compose with a :class:`interco.limiter_v2.Limiter` or a
    :class:`interco.router_v2.Router` on the appropriate side.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts reads and
      writes of any size. Returns whatever the selected output
      returned.
    - **output_0 .. output_{N-1}** (master, ``io_v2``, muxed) — N =
      ``1 << width`` downstream ports. Each one receives the subset of
      requests whose ``offset``-relative bit field equals its index.
      Ports use the muxed io_v2 master flavour so the shared
      ``resp()`` / ``retry()`` callbacks can identify which output
      fired back.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **Single upstream master.** The demux exposes one input slave
      port. Multiple masters cannot bind to the same input — use a
      :class:`interco.router_v2.Router` upstream if you need
      arbitration.
    - **Fixed fan-out at build time.** The number of outputs
      (``1 << width``) is set at construction. Runtime reconfiguration
      of ``offset`` or ``width`` is not supported.
    - **Address is forwarded unchanged.** Downstreams see the original
      byte address, including the bits that participated in routing.
      Chain a remapper if a bank-local address is required.
    - **Requests must target exactly one output.** A request whose
      address+size would straddle the selector bit field is **not**
      split — the bits are only sampled once at request entry, so a
      straddling request lands on whichever output the base address
      points to. Upstreams that issue straddling requests (e.g. a
      32-byte access across a 16-byte bank stripe) need to split
      themselves.
    - **``width == 0`` yields a single output.** Edge case: with
      ``width=0`` the demux creates exactly one output port
      (``output_0``) and every request goes there, which is
      functionally equivalent to a wire. Still valid and useful for
      parametric designs.
    - **No request reordering.** Requests are forwarded strictly in
      the order they arrive, with no priority between outputs. A DENY
      on one output stalls only that output from the upstream's
      perspective — other outputs remain addressable as soon as the
      upstream's DENY is retried (which only happens when the denied
      output retries).
    - **No latency model.** The demux does not reflect any physical
      routing cost. Insert a cycle-aware component in front of or
      behind it if cycle accuracy matters for the decode step.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    All demux parameters live on :class:`DemuxConfig`. Values are read
    once at construction; no runtime reconfiguration.

    ``offset``
        Bit position (LSB) of the selector field within the request
        address. Bits below ``offset`` are ignored — they become part
        of the byte offset delivered to the downstream. Default:
        ``0``.

    ``width``
        Number of selector bits. The demux creates exactly
        ``1 << width`` output ports. Must be non-negative. Default:
        ``0`` (single output — functionally a wire). Typical values:
        ``1`` for a two-bank split, ``2`` for four banks, ``3`` for
        eight, etc.

    Example
    ~~~~~~~

    Splitting a 16 KiB region into four 4 KiB banks placed on natural
    4 KiB alignment:

    .. code-block:: python

        demux = Demux(self, 'demux', config=DemuxConfig(offset=12, width=2))
        for i in range(4):
            demux.o_OUTPUT(i, banks[i].i_INPUT())

    An access at address ``0x0200`` goes to ``output_0`` (bits [13:12]
    = 0b00), ``0x1200`` to ``output_1`` (= 0b01), ``0x2200`` to
    ``output_2`` (= 0b10), ``0x3200`` to ``output_3`` (= 0b11). Each
    bank still sees the full byte address; to give them a local
    address space, place a remapper between the demux and the bank.

    Parameters
    ----------
    parent : Component
        Parent component this demux is instantiated under.
    name : str
        Local name of the demux within ``parent``. Used for component
        path, traces, and as the lookup key for bindings.
    config : DemuxConfig
        All demux parameters live here. The instance is taken over by
        the demux — pass a fresh :class:`DemuxConfig` per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'Demux (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/demux_v2',
             'component': 'interco.demux_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: DemuxConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/demux_v2.cpp'])

    def i_INPUT(self) -> SlaveItf:
        """Returns the single input slave port.

        Every incoming io_v2 request lands here. The demux extracts the
        output index from the address and forwards the request to the
        corresponding ``output_<id>`` master port, verbatim.
        """
        return SlaveItf(self, 'input', signature='io_v2')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        """Binds downstream ``output_<id>`` to ``itf``.

        ``id`` must be in ``[0, (1 << width))``. The bound downstream
        receives every request whose selector bit field equals ``id``.
        Responses and retries flow back through this port and are
        forwarded on the demux's input slave port.
        """
        self.itf_bind(f'output_{id}', itf, signature='io_v2')
