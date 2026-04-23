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
        Width of the input port, in bytes. Bounds the maximum size of a
        single incoming request: a request larger than ``input_width``
        cannot fit in the outputs and is refused with
        ``IO_RESP_INVALID``.
    output_width : int
        Width of each output port, in bytes. Must divide
        ``input_width`` evenly. The splitter creates exactly
        ``input_width / output_width`` output ports and slices every
        request into consecutive chunks of at most ``output_width``
        bytes, each chunk going to the next output port.
    """

    input_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of the input port, in bytes"
    ))
    output_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width of each output port, in bytes"
    ))


class Splitter(Component):
    """Width-splitting fan-out on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A one-input, N-output component that carves every incoming request
    into up to ``N = input_width / output_width`` consecutive chunks,
    each chunk at most ``output_width`` bytes wide, and fires those
    chunks as independent sub-requests on distinct output ports — the
    first chunk on ``output_0``, the second on ``output_1``, and so
    on. All sub-requests run in parallel; the parent request is
    completed once every byte has been accounted for.

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
    - oversized requests (``size > input_width``) are refused with
      ``IO_REQ_DONE`` + ``IO_RESP_INVALID`` instead of silently
      hanging the master

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

    - **Idle + new request**: the splitter latches the parent,
      primes ``parent->remaining_size`` to the parent's size, carves
      up to N sub-requests, and fires each one on its output. The
      return status is ``IO_REQ_DONE`` if every sub-request resolved
      synchronously, ``IO_REQ_GRANTED`` if any sub is pending or
      stuck on a DENY.
    - **Asynchronous completion**: when a bank fires ``resp(sub)``,
      the splitter decrements ``parent->remaining_size`` by that
      chunk's size and latches ``IO_RESP_INVALID`` onto the parent
      if the sub reported an error. Once ``remaining_size`` hits
      zero, the parent is completed via ``input_itf.resp(parent)``
      and any upstream retry owed is fired.
    - **Downstream DENY**: a DENY on output ``i`` parks the sub in
      ``stuck[i]``. Other outputs keep operating normally. When
      output ``i`` fires ``retry()``, the splitter re-sends the
      parked sub (possibly DENYing again).
    - **Upstream busy**: any CPU request that arrives while another
      is still in flight is refused with ``IO_REQ_DENIED``. The
      splitter remembers that an upstream retry is owed and fires
      ``input_itf.retry()`` once the in-flight parent completes.
    - **Per-sub error (``IO_RESP_INVALID``)**: the splitter latches
      the error on the parent, keeps processing the remaining sub
      responses, and completes the parent with
      ``IO_RESP_INVALID`` once every chunk is accounted for.

    Timing model
    ~~~~~~~~~~~~

    The splitter is **big-packet, zero-latency**: it never touches
    ``req->latency`` and never schedules a ClockEvent. Fan-out and
    completion are simulator-instantaneous within a cycle. Whatever
    latency each downstream annotates or whatever wall-clock delay
    each downstream introduces via ``resp()`` determines the
    parent's observed completion time — the slowest chunk is the
    critical path.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts one
      request at a time, up to ``input_width`` bytes. Returns
      ``IO_REQ_DONE`` when every chunk lands inline,
      ``IO_REQ_GRANTED`` when at least one chunk is pending or
      stuck, ``IO_REQ_DENIED`` when the splitter is already busy on
      another parent, and ``IO_REQ_DONE`` + ``IO_RESP_INVALID`` for
      oversized requests.
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
    - **Oversized requests are rejected.** A request whose size
      exceeds ``input_width`` is refused with ``IO_REQ_DONE`` +
      ``IO_RESP_INVALID``. Masters that want to stream bytes beyond
      ``input_width`` must issue multiple requests (or use a
      :class:`interco.limiter_v2.Limiter` to serialize them).
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
