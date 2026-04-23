# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 read/write opcode splitter component.

This module provides the ``RwSplitter`` generator for the io_v2 port of
the opcode-splitting router (``rw_splitter_v2.cpp``). The configuration
struct is the same as the v1 variant and defined here inline — fields
match ``rw_splitter.py``.
"""

from __future__ import annotations

from config_tree import Config
from gvsoc.systree import Component, SlaveItf


class RwSplitterConfig(Config):
    """Configuration for the io_v2 rw_splitter component.

    This component has no tunable parameters; the class exists only to
    satisfy the generator's requirement of a Config-typed argument.
    """
    pass


class RwSplitter(Component):
    """Read/write opcode demultiplexer on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A stateless, two-output passthrough that routes every incoming
    io_v2 request to exactly one of its two downstream ports based on
    the request opcode: writes go out on ``output_write``, everything
    else (reads and read-like atomics such as ``LR``) goes out on
    ``output_read``. The component is a plain wire with a 1-bit
    selector — no buffering, no address rewrite, no latency
    contribution.

    Typical uses:

    - Gate a shared interconnect's read and write paths through
      separate downstreams — for instance, to apply different
      latency models, arbitration policies, or bandwidth shapers to
      each direction.
    - Drive a unified-memory view over a split-interface bank (like
      TCDM stacks with separate R/W ports).
    - Instrument a design with per-direction trace/stats components
      by inserting them on one branch only.

    This is the io_v2 port of :class:`interco.rw_splitter.RwSplitter`.
    Only the IO-side plumbing is new:

    - input and output ports carry the ``io_v2`` signature
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
    - responses from either output travel back through the rw
      splitter's single input slave port via ``input_itf.resp(req)``
    - the output ports are muxed io_v2 masters, so the shared
      ``resp()`` / ``retry()`` callbacks know which side fired back
    - back-pressure uses the AXI-like ``retry()`` handshake with
      per-output deny tracking: a DENY on the read side does not
      accidentally retry a DENY on the write side, and vice versa

    Opcode classification
    ~~~~~~~~~~~~~~~~~~~~~

    The routing decision uses ``req->get_is_write()``, which in
    io_v2 boils down to ``(opcode != READ)``. So:

    ======================  ============================
    Opcode                  Output
    ======================  ============================
    ``READ``                ``output_read``
    ``WRITE``               ``output_write``
    ``LR``                  ``output_write``
    ``SC``                  ``output_write``
    ``SWAP`` / ``ADD`` /    ``output_write``
    ``XOR`` / ``AND`` /
    ``OR`` / ``MIN`` /
    ``MAX`` / ``MINU`` /
    ``MAXU``
    ======================  ============================

    If your downstream needs atomics to be treated as reads (because
    they return data) or if you want ``LR`` on the read side and
    ``SC`` on the write side, a simple opcode check before the
    rw_splitter is the recommended adaptation — the rw_splitter
    itself only looks at the read/write bit.

    Request flow
    ~~~~~~~~~~~~

    - **Incoming request**: the splitter inspects the opcode and
      forwards the request to the matching output master. The
      return status (``IO_REQ_DONE``, ``IO_REQ_GRANTED``, or
      ``IO_REQ_DENIED``) is propagated back up to the upstream
      master unchanged.
    - **Downstream returns GRANTED**: the eventual ``resp()`` on
      that output is relayed on the input slave port. The rw
      splitter does not need to remember which output was used —
      io_v2 callbacks carry the ``id`` of the firing port (0 for
      read, 1 for write), and the response contents travel on the
      request object itself.
    - **Downstream returns DENIED**: the rw splitter remembers which
      output denied the request (``denied_output``) and returns
      DENIED up. The upstream master is expected to hold the
      request until the splitter fires ``retry()``. When the same
      downstream output fires its own ``retry()``, the splitter
      relays it upstream. Retries on the *other* output are
      ignored — they don't concern a request that was destined for
      the denying side.

    Timing model
    ~~~~~~~~~~~~

    The rw splitter is **big-packet, zero-latency**. It never
    touches ``req->latency`` and never schedules a ClockEvent.
    Whatever latency each downstream reports — whether via a
    synchronous ``IO_REQ_DONE`` with an annotated ``req->latency``
    or via a deferred ``resp()`` — is observed directly by the
    upstream master. The component itself costs one branch and one
    call.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing.
    - **output_read** (master, ``io_v2``, muxed, id 0) — receives
      every request whose ``is_write`` bit is false.
    - **output_write** (master, ``io_v2``, muxed, id 1) — receives
      every request whose ``is_write`` bit is true.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **Single upstream master.** Only one master can bind to the
      input port. For multiple masters, insert a router (e.g.
      :class:`interco.router_v2.Router`) upstream.
    - **Two outputs, fixed.** Exactly one read output and one write
      output are created; topology is not configurable.
    - **Opcode classification is coarse.** Every atomic is treated
      as a write because ``get_is_write()`` returns ``true`` for
      every non-``READ`` opcode. Models that need atomics split out
      separately must insert a finer-grained classifier.
    - **No request rewriting.** The address, size, and data pointer
      are forwarded verbatim. Downstreams see whatever the upstream
      sent.
    - **No arbitration.** The two outputs are independent from the
      splitter's point of view; if you pair them with the same
      final downstream, arbitration must happen there.
    - **DENY tracking is single-slot.** At most one upstream
      request can be in the denied state at a time. This matches
      the io_v2 protocol's invariant (a master holds its denied
      request until it is retried), so no state is needed per
      output beyond "which of the two denied us".
    - **No latency model.** The splitter adds zero cycles. Pair
      with a :class:`interco.limiter_v2.Limiter` or
      :class:`interco.router_v2.Router` if cycle accuracy matters.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    :class:`RwSplitterConfig` has no tunable parameters. A
    :class:`RwSplitterConfig` instance is still required by the
    generator's ``config`` argument — just pass a fresh empty
    instance.

    Example
    ~~~~~~~

    Split a CPU's data port so that reads go through one latency
    model and writes through another:

    .. code-block:: python

        split = RwSplitter(self, 'rwsplit', config=RwSplitterConfig())
        core.o_DATA(split.i_INPUT())
        split.o_READ_OUTPUT(read_pipeline.i_INPUT())
        split.o_WRITE_OUTPUT(write_pipeline.i_INPUT())

    A CPU load at address ``0x1000`` reaches ``read_pipeline``; a
    CPU store at ``0x1000`` reaches ``write_pipeline``.

    Parameters
    ----------
    parent : Component
        Parent component this splitter is instantiated under.
    name : str
        Local name of the splitter within ``parent``. Used for
        component path, traces, and as the lookup key for bindings.
    config : RwSplitterConfig
        A (currently empty) configuration object. Pass a fresh
        :class:`RwSplitterConfig` per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'RwSplitter (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/rw_splitter_v2',
             'component': 'interco.rw_splitter_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: RwSplitterConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/rw_splitter_v2.cpp'])

    def i_INPUT(self) -> SlaveItf:
        """Returns the single input slave port.

        Every incoming io_v2 request lands here. The splitter
        inspects the request opcode and forwards the request to
        ``output_read`` (reads, atomics with read semantics) or
        ``output_write`` (writes, atomics with write semantics),
        verbatim.
        """
        return SlaveItf(self, 'input', signature='io_v2')

    def o_READ_OUTPUT(self, itf: SlaveItf):
        """Binds the downstream read-side master port.

        Receives every request whose ``is_write`` bit is false.
        """
        self.itf_bind('output_read', itf, signature='io_v2')

    def o_WRITE_OUTPUT(self, itf: SlaveItf):
        """Binds the downstream write-side master port.

        Receives every request whose ``is_write`` bit is true.
        """
        self.itf_bind('output_write', itf, signature='io_v2')
