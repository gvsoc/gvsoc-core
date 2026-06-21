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


class SplitterConfig(Config):
    """Configuration for the io_v2 splitter component.

    Attributes
    ----------
    input_width : int
        Width of one fan-out phase, in bytes. Requests larger than
        ``input_width`` are walked as a sequence of phases, each phase
        covering up to ``input_width`` bytes. Bounds the maximum
        observable bandwidth per phase but not the maximum request
        size.
    output_width : int
        Width of each output port, in bytes. Must divide
        ``input_width`` evenly. The splitter creates exactly
        ``input_width / output_width`` output ports and slices each
        phase into consecutive chunks of at most ``output_width``
        bytes, one per output port.
    """

    input_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of one fan-out phase, in bytes"
    ))
    output_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of each output port, in bytes"
    ))


class Splitter(Component):
    """Width-splitting fan-out on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A one-input, N-output component that walks every incoming request
    as a sequence of phases. Each phase covers at most ``input_width``
    bytes and is fanned out across up to ``N = input_width /
    output_width`` consecutive chunks, each chunk at most
    ``output_width`` bytes wide, on distinct output ports — the first
    chunk on ``output_0``, the second on ``output_1``, and so on.
    Chunks within a phase run in parallel; phases run strictly in
    order, the next one starting only after every chunk of the
    current phase has responded.

    This is the io_v2 port of :class:`interco.splitter.Splitter`. Only
    the IO-side plumbing is new:

    - input port is a single io_v2 slave; responses flow back via
      ``input_itf.resp(req)``
    - output ports are io_v2 muxed masters so the shared response /
      retry callbacks can identify which output fired back
    - back-pressure uses the AXI-like ``retry()`` handshake: a DENY
      on any single output parks that sub-request and the others
      keep flowing; upstream sees ``IO_REQ_DENIED`` only when it
      tries to send a *new* parent while another is still in flight
    - requests larger than ``input_width`` are walked as multiple
      phases instead of being refused; this mirrors the role of an
      AXI→memory bridge in front of a wide log interconnect (one AXI
      burst unrolled into per-cycle beats, each beat striped across
      the bank lanes)

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

    - **Idle + new request**: the splitter latches the parent and
      iterates through phases. Each phase carves up to N sub-requests
      and fires them on the corresponding outputs. The iteration is
      eager: as long as every chunk of the current phase resolved
      inline, the next phase is issued in the same call. The return
      status is ``IO_REQ_DONE`` if every chunk of every phase
      resolved synchronously, ``IO_REQ_GRANTED`` otherwise.
    - **Phase boundary**: phase k+1 only fires once every chunk of
      phase k has responded. The slowest chunk dominates the phase's
      observable latency; the splitter accumulates the per-phase max
      ``req->latency`` into a running total used at finalisation.
    - **Asynchronous completion**: when a bank fires ``resp(sub)``,
      the splitter decrements ``parent->remaining_size`` by that
      chunk's size, latches ``IO_RESP_INVALID`` if the sub reported
      an error, and decrements the in-flight count for the current
      phase. When that count hits zero, the phase is done; the
      splitter either issues the next phase or fires
      ``input_itf.resp(parent)`` with the bandwidth annotation
      described below.
    - **Downstream DENY**: a DENY on output ``i`` parks the sub in
      ``stuck[i]``. Other outputs of the same phase keep operating
      normally. When output ``i`` fires ``retry()``, the splitter
      re-sends the parked sub (possibly DENYing again). The phase
      does not advance until that chunk eventually succeeds.
    - **Upstream busy**: any CPU request that arrives while another
      is still in flight is refused with ``IO_REQ_DENIED``. The
      splitter remembers that an upstream retry is owed and fires
      ``input_itf.retry()`` once the in-flight parent completes.
    - **Per-sub error (``IO_RESP_INVALID``)**: the splitter latches
      the error on the parent, keeps processing the remaining sub
      responses of the current phase, advances to subsequent phases
      normally, and completes the parent with ``IO_RESP_INVALID``
      once every chunk of every phase is accounted for.

    Timing model
    ~~~~~~~~~~~~

    Per phase, the splitter takes the maximum ``req->latency``
    reported by any of the phase's chunks (the slowest lane
    dominates) and adds it to a running ``accumulated_phase_latency``
    counter. The fan-out itself is simulator-instantaneous; the
    splitter does not schedule its own ClockEvents.

    At finalisation, real simulator cycles have already elapsed if
    any chunk responded asynchronously or was denied-and-retried.
    The annotation on the parent's response is the leftover ideal
    budget:

    .. code-block:: text

        req->latency = max(0, accumulated_phase_latency - elapsed_sim)

    so the master observes the larger of (ideal bandwidth time,
    real contention time), never double-counts the two, and never
    reports a smaller window than what already elapsed. For a
    single-phase request with all chunks DONE inline, this collapses
    to "max chunk latency" — the same critical-path semantic the
    previous splitter had.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts one
      request at a time, of any size. Returns ``IO_REQ_DONE`` when
      every chunk of every phase lands inline, ``IO_REQ_GRANTED``
      when at least one chunk is pending or stuck, and
      ``IO_REQ_DENIED`` when the splitter is already busy on another
      parent.
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
    - **Requests larger than ``input_width`` are walked in phases.**
      Each phase covers up to ``input_width`` bytes (less if it sits
      at the start of an unaligned burst). Phases run strictly in
      order — phase k+1 only fires after every chunk of phase k has
      responded — so the slowest lane of any phase back-pressures the
      whole burst.
    - **Empty requests (size = 0) complete as DONE with OK status.**
      No sub-request is emitted, no output is touched.
    - **One parent in flight at a time.** Even if the parent has
      left all chunks pending asynchronously, a new incoming CPU
      request will be DENIED until every byte of the first parent
      has been responded to. Use a :class:`interco.router_v2.Router`
      upstream if you need multiple parents overlapping.
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
        return SlaveItf(self, 'input', signature='io_v2')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        """Binds downstream ``output_<id>`` to ``itf``.

        ``id`` must be in ``[0, nb_outputs)`` where
        ``nb_outputs = input_width / output_width``. The bound
        downstream receives the N-th consecutive chunk of every parent
        request that has ``chunk_id`` ≤ ``N``.
        """
        self.itf_bind(f'output_{id}', itf, signature='io_v2')
