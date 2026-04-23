# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 logarithmic (bank-interleaved) crossbar component.

This module provides the ``LogIco`` generator for the io_v2 port of the
logarithmic interconnect model (``log_ico_v2.cpp``). The configuration
struct is the same as the v1 variant and defined here inline — fields
match ``log_ico.py``.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf


class LogIcoConfig(Config):
    """Configuration for the io_v2 logarithmic crossbar component.

    Attributes
    ----------
    nb_masters : int
        Number of input master ports exposed by the crossbar. Each
        master can concurrently address any of the ``nb_slaves`` banks.
    nb_slaves : int
        Number of output bank ports. Address space is striped across
        these banks at :attr:`interleaving_width`-byte granularity.
    interleaving_width : int
        Number of low-order address bits defining the granule that
        stays within one bank before the traffic rolls over to the next
        one. Expressed in *bit positions*, not bytes: a value of
        ``4`` means 16-byte (``1 << 4``) granules.
    remove_offset : int
        Base address subtracted from every incoming request before bank
        decoding. Use this when the crossbar covers a sub-range of the
        address map that does not start at 0 — the bank-local address
        delivered downstream is always relative to ``remove_offset``.
    """

    nb_masters: int = cfg_field(default=0, dump=True, desc=(
        "Number of master input ports"
    ))
    nb_slaves: int = cfg_field(default=0, dump=True, desc=(
        "Number of slave output ports (banks)"
    ))
    interleaving_width: int = cfg_field(default=0, dump=True, desc=(
        "Number of low-order address bits kept local to a bank (granule size = 1 << interleaving_width)"
    ))
    remove_offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Base address subtracted from incoming requests before bank decode"
    ))


class LogIco(Component):
    """Logarithmic (bank-interleaved) crossbar on the io_v2 protocol.

    Overview
    ~~~~~~~~

    An M-to-N crossbar that spreads a contiguous address range across
    ``nb_slaves`` banks at a fixed interleaving granularity, and lets
    up to ``nb_masters`` masters issue independent requests into that
    range. The model is a functional, zero-latency shuffle: it does
    not arbitrate between masters (every master has its own input port
    and races with no other on that port), it does not buffer data,
    and it does not add any timing.

    This is the io_v2 port of :class:`interco.log_ico.LogIco`. Only the
    IO-side plumbing is new:

    - inputs are muxed slave ports; the shared request callback
      receives the master id
    - outputs are muxed master ports; the shared response and retry
      callbacks receive the bank id
    - replies travel back via the specific input port of the master
      that issued the request — per-request tracking is done through
      an internal ``InFlight`` slot that replaces ``req->initiator``
      for the duration of the transaction
    - back-pressure uses the AXI-like ``retry()`` handshake with
      per-master denied-state tracking (``denied_on_bank``) so that
      bank-local stalls never ripple across masters that are accessing
      other banks

    Address decoding
    ~~~~~~~~~~~~~~~~

    Given an incoming address ``addr``, the crossbar computes:

    .. code-block:: text

        offset      = addr - remove_offset
        slave_bits  = ceil_log2(nb_slaves)
        bank_id     = (offset >> interleaving_width) & (nb_slaves - 1)
        bank_offset = ((offset >> (slave_bits + interleaving_width)) << interleaving_width)
                      | (offset & ((1 << interleaving_width) - 1))

    The address delivered to the bank is ``bank_offset`` — the bank
    sees a compacted local address space starting at 0. Concretely,
    bits in the address play these roles:

    .. code-block:: text

        | high bits ... | bank_id (slave_bits) | granule bits (interleaving_width) |
                 |_________|     selector     |__________________________________|
                     |                                        |
                     +---- moved down, shifted left by        +---- kept in place, becomes
                           interleaving_width, into the             the low part of the bank
                           bank_offset high part                    local address

    Worked example
    ^^^^^^^^^^^^^^

    ``nb_slaves = 4`` (``slave_bits = 2``), ``interleaving_width = 4``
    (16-byte granules), ``remove_offset = 0``. Address ``0x0A34``:

    - binary: ``0000_1010_0011_0100``
    - granule bits [3:0] = ``0100`` → low 4 bits of bank-local addr
    - bank selector [5:4] = ``11`` → bank ``3``
    - high bits [11:6] = ``101000`` → bank-local high part
    - bank-local address = ``(101000 << 4) | 0100`` = ``0x284``

    So byte ``0x0A34`` of the global range is served by bank ``3`` at
    its local offset ``0x284``.

    Memory map properties:

    - Bytes ``0`` .. ``(1 << interleaving_width) - 1`` all go to
      bank ``0`` at local addresses ``0`` .. ``(1 << interleaving_width) - 1``.
    - The next ``(1 << interleaving_width)`` bytes go to bank ``1`` at
      the same local addresses, and so on.
    - After ``nb_slaves`` granules the cycle repeats, incremented by
      one granule in each bank's local address space.

    Request flow
    ~~~~~~~~~~~~

    - **Incoming request**: the crossbar computes ``bank_id`` and
      ``bank_offset``, saves the original address, saves the master's
      ``initiator`` pointer into a pooled ``InFlight`` slot, rewrites
      the request address to ``bank_offset``, and forwards the request
      to the selected bank.
    - **Bank returns DONE**: the crossbar restores the master's
      ``initiator``, frees the ``InFlight`` slot, and returns DONE up
      to the originating master.
    - **Bank returns GRANTED**: the ``InFlight`` slot stays live; the
      crossbar propagates GRANTED up. When the bank later calls
      ``resp(req)``, the crossbar reads ``input_id`` off the
      ``InFlight``, restores the master's ``initiator``, frees the
      slot, and forwards the response on the correct input port.
    - **Bank returns DENIED**: the crossbar rolls the request back to
      its pre-forward state (restores ``addr`` and ``initiator``),
      records ``denied_on_bank[m] = bank_id`` for the master ``m``
      that issued the request, and returns DENIED up to that master.
      No other master is affected.
    - **Bank fires retry**: the crossbar calls ``retry()`` on every
      input port whose denied-bank entry matches the bank that just
      retried. Masters waiting on a different bank are not disturbed.

    Timing model
    ~~~~~~~~~~~~

    The crossbar is **big-packet, zero-latency**: it never touches
    ``req->latency`` and never schedules a ClockEvent. Whatever
    latency a bank annotates (sync path) or whatever wall-clock delay
    it introduces via a deferred ``resp()`` (async path) is observed
    directly by the calling master. A real bank interconnect would
    model at least a fixed routing cycle — compose this crossbar with
    a per-bank :class:`interco.limiter_v2.Limiter` or a
    :class:`interco.router_v2.Router` variant if cycle accuracy
    matters.

    Ports
    ~~~~~

    - **input_0 .. input_{M-1}** (slave, ``io_v2``, muxed) — one per
      master. ``M = nb_masters``. Each input is independent; the
      crossbar does not arbitrate between them.
    - **output_0 .. output_{N-1}** (master, ``io_v2``, muxed) — one
      per bank. ``N = nb_slaves``. Each output receives requests
      whose bank selector bits equal its index, with the address
      rewritten to bank-local form.

    The port methods :meth:`i_INPUT` and :meth:`o_OUTPUT` take an
    ``id`` argument to name the specific port.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **``nb_slaves`` should be a power of two for clean decode.** The
      decode uses ``(1 << slave_bits) - 1`` as the bank-id mask; with
      non-power-of-two ``nb_slaves``, a subset of bank-id values
      would map onto valid banks while the rest would index out of
      range. No runtime check is performed — it is the caller's
      responsibility.
    - **``interleaving_width`` must be non-negative.** A value of 0
      degenerates to *byte-level* interleaving — every byte goes to a
      different bank. Valid but usually not what you want; set it to
      the log2 of the granule size you actually want.
    - **Requests must not straddle a bank boundary.** The crossbar
      samples the bank selector once from the base address and
      forwards the whole request to that bank. A request that crosses
      a granule boundary delivers bytes beyond the granule to the
      wrong place inside the bank. Upstreams must keep each request
      within a single granule (``addr % (1 << iw) + size <= (1 << iw)``).
    - **No arbitration, no ordering.** Two masters issuing concurrent
      requests to the *same* bank are forwarded in the order the
      engine scheduled them — no explicit fairness, no round-robin.
      A real crossbar would arbitrate; this model lets the bank see
      whichever request the simulator processed first.
    - **Fixed topology at build time.** ``nb_masters``, ``nb_slaves``,
      ``interleaving_width``, ``remove_offset`` are read once. Runtime
      reconfiguration is not supported.
    - **Address rewrite is not reversed on DONE/GRANTED.** On the
      response path the crossbar restores only ``initiator``; the
      request object retains the bank-local address. Masters that
      re-read ``req->addr`` after completion will see the bank-local
      value. On DENIED the original address is restored so a resend
      works correctly.
    - **Single denied bank per master.** At most one denied request
      per master can be pending (a direct consequence of the io_v2
      protocol: a master holds its denied request until the slave
      retries). The ``denied_on_bank`` vector therefore has exactly
      one entry per master.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    All crossbar parameters live on :class:`LogIcoConfig`. Values are
    read once at construction; no runtime reconfiguration.

    ``nb_masters``
        Number of input master ports. Must be ≥ 1. Default: ``0`` —
        must be overridden.

    ``nb_slaves``
        Number of output bank ports. Must be ≥ 1; power of two for
        a clean address decode. Default: ``0`` — must be overridden.

    ``interleaving_width``
        Log2 of the bank-local granule size, in bytes. With
        ``interleaving_width=2`` the granule is 4 bytes (word-level
        interleaving); with ``interleaving_width=6`` the granule is
        64 bytes (cache-line interleaving). Default: ``0`` (byte
        interleaving).

    ``remove_offset``
        Global base address of the region this crossbar covers. Every
        incoming request has this subtracted before decode so the
        bank-local address starts at 0 relative to the region origin.
        Default: ``0``.

    Example
    ~~~~~~~

    Four masters fan in to eight banks, 32-byte interleaving, region
    starting at ``0x1000_0000``:

    .. code-block:: python

        xbar = LogIco(self, 'xbar', config=LogIcoConfig(
            nb_masters=4, nb_slaves=8,
            interleaving_width=5, remove_offset=0x10000000))
        for m in range(4):
            cores[m].o_DATA(xbar.i_INPUT(m))
        for s in range(8):
            xbar.o_OUTPUT(s, banks[s].i_INPUT())

    A request from core 2 to ``0x1000_0040`` (offset 0x40, granule-id
    0x2, bank 2) reaches ``banks[2]`` at bank-local address 0. A
    concurrent request from core 3 to ``0x1000_0060`` (offset 0x60,
    granule 0x3, bank 3) reaches ``banks[3]`` at bank-local 0 in
    parallel — the two masters do not stall each other.

    Parameters
    ----------
    parent : Component
        Parent component this crossbar is instantiated under.
    name : str
        Local name of the crossbar within ``parent``. Used for
        component path, traces, and as the lookup key for bindings.
    config : LogIcoConfig
        All crossbar parameters live here. The instance is taken over
        by the crossbar — pass a fresh :class:`LogIcoConfig` per
        instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'LogIco (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/log_ico_v2',
             'component': 'interco.log_ico_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: LogIcoConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/log_ico_v2.cpp'])

    def i_INPUT(self, id: int) -> SlaveItf:
        """Returns the slave port ``input_<id>`` for master ``id``.

        Every master gets its own muxed input port. Requests arriving
        on different inputs are decoded independently; there is no
        arbitration between masters.
        """
        return SlaveItf(self, f'input_{id}', signature='io_v2')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        """Binds downstream bank ``output_<id>`` to ``itf``.

        ``id`` must be in ``[0, nb_slaves)``. The bound downstream
        receives requests whose selector bit field equals ``id``,
        with the address rewritten to bank-local form.
        """
        self.itf_bind(f'output_{id}', itf, signature='io_v2')
