# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 width-splitting fan-out component.

This module provides the ``Splitter`` generator for the io_v2 port of
the splitter model (``splitter_v2.cpp``). The configuration struct is
the same as the v1 variant and defined here inline — fields match
``splitter.py``.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2SingleReq


class SplitterConfig(Config):
    """Configuration for the io_v2 splitter component.

    Attributes
    ----------
    input_width : int
        Width of the input port / one fan-out window, in bytes. Every
        request must fit a single window
        (``(addr % input_width) + size <= input_width``); larger bursts
        must be chunked into ``input_width``-wide beats upstream before
        they reach the splitter.
    output_width : int
        Width of each output port, in bytes. Must divide
        ``input_width`` evenly. The splitter creates exactly
        ``input_width / output_width`` output ports and slices each
        request into consecutive chunks of at most ``output_width``
        bytes, one per output port.
    """

    input_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of the input port / one fan-out window, in bytes"
    ))
    output_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of each output port, in bytes"
    ))


class Splitter(Component):
    """Width-splitting fan-out on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A one-input, N-output component that fans every incoming request
    across up to ``N = input_width / output_width`` consecutive
    chunks, each chunk at most ``output_width`` bytes wide, on distinct
    output ports — the first chunk on ``output_0``, the second on
    ``output_1``, and so on. The chunks of one request run in parallel.

    The splitter is **stateless and pipelined**: it keeps no per-request
    state of its own. Each request is the aggregation context for its
    own chunks — ``remaining_size`` counts down as chunks respond and
    ``latency`` takes the max (the critical path) — and every chunk
    carries a ``parent`` back-link, so a chunk response is folded
    straight into its parent no matter how many parents are in flight.
    The splitter therefore never serialises on a single in-flight
    parent: it forwards a request's chunks and returns immediately,
    ready to accept the next request, so beats stream back-to-back.

    Single-fan-out only. A request must fit one ``input_width`` window
    (``(addr % input_width) + size <= input_width``). Larger bursts are
    expected to be chunked into ``input_width``-wide beats upstream
    (e.g. by an AXI beat router / its auto-inserted ``IoV2BeatAdapter``)
    before reaching the splitter; a request that would straddle the
    window aborts.

    This is the io_v2 port of :class:`interco.splitter.Splitter`. Only
    the IO-side plumbing is new:

    - input port is a single io_v2 slave; responses flow back via
      ``input_itf.resp(req)``
    - output ports are io_v2 muxed masters so the shared response /
      retry callbacks can identify which output fired back
    - back-pressure uses the AXI-like ``retry()`` handshake: a DENY
      on any single output parks that sub-request and the others
      keep flowing; upstream sees ``IO_REQ_DENIED`` only while an
      output still holds a denied chunk (so it never accumulates a
      second one)

    Address/chunk layout
    ~~~~~~~~~~~~~~~~~~~~

    The fan-out is address-contiguous. Given an incoming
    ``(addr, size)`` pair:

    - **Chunk 0 → output_0**: covers ``[addr, addr + port_size_0)``
      where ``port_size_0 = output_width - (addr mod output_width)``.
      In other words, the first chunk extends from the incoming
      address up to the next ``output_width`` byte boundary.
    - **Chunk 1..N-1 → output_1..output_{N-1}**: each chunk starts
      aligned to ``output_width`` and covers up to ``output_width``
      bytes, continuing until the request is exhausted or every
      output has been given a sub-request.
    - **output_i for i >= chunks_needed**: no sub-request is emitted;
      that output stays idle for this parent.

    The splitter does **not** rewrite the address — each sub-request
    keeps the global address of its first byte. Downstream targets
    see the original address space, not a bank-local one. Chain a
    :class:`interco.remapper_v2.Remapper` on each output branch if a
    bank-local address is needed.

    Request flow
    ~~~~~~~~~~~~

    - **New request**: the splitter carves up to N sub-requests and
      fires them on the corresponding outputs, then returns. The
      status is ``IO_REQ_DONE`` if every chunk resolved inline,
      ``IO_REQ_GRANTED`` if at least one chunk is pending or stuck.
      It does not wait for prior parents — the next request can be
      accepted on the very next cycle.
    - **Asynchronous completion**: when a bank fires ``resp(sub)``,
      the splitter folds the chunk into ``sub->parent`` — decrements
      ``parent->remaining_size`` by the chunk size, max-ins the
      chunk's ``latency``, and latches ``IO_RESP_INVALID`` on error.
      When ``remaining_size`` reaches zero the parent is complete and
      ``input_itf.resp(parent)`` fires.
    - **Downstream DENY**: a DENY on output ``i`` parks the sub in
      ``stuck[i]``. Other outputs keep operating normally. When output
      ``i`` fires ``retry()``, the splitter re-sends the parked sub
      (possibly DENYing again).
    - **Upstream back-pressure**: while any output holds a denied
      chunk, a new incoming request is refused with ``IO_REQ_DENIED``
      (so an output never accumulates a second stuck chunk). The
      splitter fires ``input_itf.retry()`` once every output is free.
    - **Per-sub error (``IO_RESP_INVALID``)**: latched on the parent;
      the parent completes with ``IO_RESP_INVALID`` once all chunks
      are accounted for.

    Timing model
    ~~~~~~~~~~~~

    The splitter adds no cycles of its own and schedules no
    ClockEvents. A request's reported ``latency`` is the maximum
    ``latency`` annotated by any of its chunks — the slowest lane
    dominates the critical path. All cycle accuracy comes from the
    downstream models.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts a request
      that fits one ``input_width`` window. Returns ``IO_REQ_DONE``
      when every chunk lands inline, ``IO_REQ_GRANTED`` when at least
      one chunk is pending or stuck, and ``IO_REQ_DENIED`` while an
      output still holds a denied chunk.
    - **output_0 .. output_{N-1}** (master, ``io_v2``, muxed) — one
      per chunk slot, ``N = input_width / output_width``. Each
      output receives the N-th consecutive chunk of the parent (if
      one exists). Muxed flavour so ``resp()`` / ``retry()``
      callbacks identify the output.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **``input_width`` must be a multiple of ``output_width``.** The
      model derives ``nb_outputs`` by integer division; a non-integer
      ratio silently rounds down. Use aligned widths (both powers of
      two, ``input_width >= output_width``).
    - **``output_width`` must be a power of two.** The chunk boundary
      computation uses ``addr & (output_width - 1)`` as the mask —
      non-power-of-two widths produce nonsensical splits.
    - **Single fan-out only.** A request must satisfy
      ``(addr % input_width) + size <= input_width``; one that would
      straddle the window aborts. Chunk larger bursts into
      ``input_width``-wide beats upstream (an AXI beat router does this
      via its auto-inserted ``IoV2BeatAdapter``).
    - **Empty requests (size = 0) complete as DONE with OK status.**
      No sub-request is emitted, no output is touched.
    - **Multiple parents may overlap.** The splitter is stateless and
      pipelined; it does not serialise on one in-flight parent. The
      only back-pressure is the one-stuck-chunk-per-output rule.
    - **No arbitration fairness.** Chunks go out on their fixed
      output slot — chunk 0 always hits ``output_0``, chunk 1 always
      hits ``output_1``, etc. A pathological workload where every
      request is addressed so only ``output_0`` ever gets traffic
      will still work, but the other outputs will sit idle.
    - **No address rewrite.** Sub-requests carry global addresses.
      The chunk splitting happens on the *data* axis, not the
      *address* axis — downstream banks see addresses exactly as the
      master emitted them.
    - **Sub-request addresses are contiguous.** A single parent's
      chunks always cover ``[addr, addr + size)`` without gap and
      without overlap. If your downstream expects per-output address
      isolation, add per-output remappers.
    - **No latency model.** The splitter adds zero cycles. Cycle
      accuracy must come from the downstream models.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    All splitter parameters live on :class:`SplitterConfig`. Values
    are read once at construction; no runtime reconfiguration.

    ``input_width``
        Width of the input port, in bytes. Also the upper bound on
        an incoming request's size. Default: ``0`` — must be
        overridden.

    ``output_width``
        Width of each output port, in bytes. Must be a power of two
        and divide ``input_width`` evenly. Default: ``0`` — must be
        overridden.

    Example
    ~~~~~~~

    Split a 16-byte-wide input across four 4-byte-wide outputs (e.g.
    four narrow SRAM banks, each handling one 32-bit lane):

    .. code-block:: python

        splitter = Splitter(self, 'splitter', config=SplitterConfig(
            input_width=16, output_width=4))
        master.o_OUTPUT(splitter.i_INPUT())
        for i in range(4):
            splitter.o_OUTPUT(i, lanes[i].i_INPUT())

    An access at ``(addr=0x00, size=16)`` emits four aligned
    sub-requests of 4 bytes each: chunk 0 on ``output_0`` at 0x00,
    chunk 1 on ``output_1`` at 0x04, chunk 2 on ``output_2`` at
    0x08, chunk 3 on ``output_3`` at 0x0C. An access at
    ``(addr=0x02, size=12)`` first emits a 2-byte chunk on
    ``output_0`` at 0x02 (up to the next 4-byte boundary) and then
    three 4-byte chunks at 0x04, 0x08, 0x0C — exactly matching the
    remaining 12 bytes.

    Parameters
    ----------
    parent : Component
        Parent component this splitter is instantiated under.
    name : str
        Local name of the splitter within ``parent``. Used for
        component path, traces, and as the lookup key for bindings.
    config : SplitterConfig
        All splitter parameters live here. The instance is taken
        over by the splitter — pass a fresh :class:`SplitterConfig`
        per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'Splitter (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/splitter_v2',
             'component': 'interco.splitter_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: SplitterConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/splitter_v2.cpp'])
        self.nb_outputs = config.input_width // config.output_width

    def gen_gui(self, parent_signal):
        """Surface the input access and each fan-out chunk in the GUI."""
        import gvsoc.gui
        top = gvsoc.gui.Signal(self, parent_signal, name=self.name,
            path="active", groups=['regmap', 'active'],
            display=gvsoc.gui.DisplayLogicBox('ACTIVE'))
        inp = gvsoc.gui.Signal(self, top, "input", path="addr",
            groups=['regmap', 'active'])
        gvsoc.gui.Signal(self, inp, "size", path="size", groups=['regmap'])
        gvsoc.gui.Signal(self, inp, "is_write", path="is_write", groups=['regmap'])
        for i in range(self.nb_outputs):
            out = gvsoc.gui.Signal(self, top, f"output_{i}",
                path=f"output_{i}/addr", groups=['regmap', 'active'])
            gvsoc.gui.Signal(self, out, "size", path=f"output_{i}/size",
                groups=['regmap'])

    def i_INPUT(self) -> SlaveItf:
        """Returns the single input slave port.

        Incoming io_v2 requests land here. Each request is carved into
        up to ``nb_outputs`` consecutive sub-requests and fired out on
        the corresponding output ports.
        """
        return SlaveItf(self, 'input', signature=IoV2SingleReq())

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        """Binds downstream ``output_<id>`` to ``itf``.

        ``id`` must be in ``[0, nb_outputs)`` where
        ``nb_outputs = input_width / output_width``. The bound
        downstream receives the N-th consecutive chunk of every parent
        request that has ``chunk_id`` ≤ ``N``.
        """
        self.itf_bind(f'output_{id}', itf, signature=IoV2SingleReq())
