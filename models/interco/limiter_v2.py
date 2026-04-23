# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 bandwidth-shaping limiter component.

This module provides the ``Limiter`` generator for the io_v2 port of the
limiter model (``limiter_v2.cpp``). The configuration struct is the same
as the v1 variant and defined here inline — fields match ``limiter.py``.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf


class LimiterConfig(Config):
    """Configuration for the io_v2 limiter component.

    Attributes
    ----------
    bandwidth : int
        Maximum number of bytes the limiter forwards to the output port per
        cycle. Each incoming request is split into chunks of at most this
        many bytes, and one chunk is emitted per cycle.
    """

    bandwidth: int = cfg_field(default=0, dump=True, desc=(
        "Maximum number of bytes forwarded downstream per cycle"
    ))


class Limiter(Component):
    """Bandwidth-shaping limiter on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A pass-through component that sits on an io_v2 path and enforces a
    ceiling on how many bytes per cycle reach the downstream. Its job is
    to model a bus or memory port whose sustained throughput is lower
    than the raw issue rate of the masters behind it — typically a
    narrow off-chip memory, an interconnect with a bandwidth budget, or
    a DMA channel whose physical datapath is smaller than its logical
    transfer size.

    Every accepted request is split into sub-requests of at most
    :attr:`LimiterConfig.bandwidth` bytes each, and the limiter emits
    exactly one sub-request per cycle. Downstream completion is
    reassembled on the parent request so the upstream master sees a
    single ``resp()`` regardless of how many chunks the limiter
    generated. The limiter is stateless with respect to the data it
    carries — it does not buffer bytes, does not reorder, and does not
    coalesce.

    This is the io_v2 port of :class:`interco.limiter.Limiter`. The
    functional behaviour is identical; only the IO-side plumbing is new:

    - input and output ports carry the ``io_v2`` signature
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
    - error reporting is via ``req->set_resp_status(IO_RESP_INVALID)`` +
      ``IO_REQ_DONE``
    - replies travel back on the limiter's own slave port (no v1
      ``resp_port`` indirection)
    - upstream back-pressure is expressed via the AXI-like
      ``DENIED``/``retry()`` handshake instead of the v1
      grant-queue scheme

    Request flow
    ~~~~~~~~~~~~

    - **Idle + new request**: the incoming CPU request is latched as the
      in-emission request, acknowledged as ``IO_REQ_GRANTED`` upstream,
      and a ClockEvent is armed to start emission on the next cycle.
      The parent's ``remaining_size`` counter is primed to the full
      request size so later sub-responses can be reassembled.
    - **Emission (one per cycle)**: the in-emission request is split
      into a chunk of at most :attr:`LimiterConfig.bandwidth` bytes. A
      fresh sub-request (pooled internally via a free list, linked back
      to the CPU request via the ``parent`` pointer) is forwarded
      through the ``OUTPUT`` master port. Both inline (``IO_REQ_DONE``)
      and deferred (``IO_REQ_GRANTED``) completion are supported — the
      limiter does not care which one the downstream picks. After each
      chunk the event re-arms for the next cycle, until every byte has
      been sent.
    - **Emission complete, responses pending**: as soon as the last
      chunk of the current CPU request has been *sent*, the emission
      slot is freed and a new CPU request may be accepted — even if
      some response chunks for the previous one are still in flight.
      Completion of the parent is tracked through its ``remaining_size``
      counter and signalled upstream via ``input_itf.resp(parent)``
      once the last response byte lands.
    - **Upstream busy**: any CPU request that arrives while another is
      still in emission is rejected with ``IO_REQ_DENIED``. The master
      is expected to hold the request and resend once the limiter
      issues ``retry()``, which happens as soon as the limiter is free
      (either the last chunk has been emitted or the last response has
      landed, whichever comes later).
    - **Downstream DENY**: if the output returns ``IO_REQ_DENIED`` for
      a chunk, the chunk is parked in an internal slot and the emission
      ClockEvent pauses. When the output fires ``retry()`` the parked
      chunk is re-sent (same sub-request object, same address), and
      once accepted the pacing resumes. Emission does not advance while
      stalled, so no byte is dropped.
    - **Per-chunk error**: if a downstream chunk responds with
      ``IO_RESP_INVALID``, that status is latched onto the parent and
      the remaining chunks still proceed. The upstream master sees a
      single ``resp()`` with ``IO_RESP_INVALID`` once every chunk has
      been accounted for.

    Timing model
    ~~~~~~~~~~~~

    The limiter is a hybrid timing model:

    - Emission side is **cycle-accurate**: a ClockEvent fires every
      cycle that emission is active, so wall-clock time spent in the
      limiter is exactly ``ceil(size / bandwidth)`` cycles in the
      happy path (no downstream stalls).
    - Response side is **big-packet**: the limiter does not annotate
      extra latency on the parent; the effective response time is the
      last chunk's ``resp()`` wall-clock time. A downstream that itself
      annotates ``req->latency`` on its chunks will stack those
      latencies into the parent's observed completion time indirectly,
      via when ``resp()`` fires.

    There is **no per-cycle queueing**: only one CPU request is in
    emission at a time, and only one chunk may be denied at a time.
    This makes the limiter cheap but also means a master that wants
    back-to-back throughput must split its requests into
    bandwidth-sized pieces itself if overlapping multi-request
    emission is needed — otherwise the ``DENIED``/``retry()``
    handshake re-serialises them.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts reads and
      writes of any size. Returns ``IO_REQ_GRANTED`` on accept,
      ``IO_REQ_DENIED`` when another request is already in emission,
      or ``IO_REQ_DONE`` with ``IO_RESP_INVALID`` on a per-chunk
      error.
    - **OUTPUT** (master, ``io_v2``) — downstream. One sub-request per
      cycle, each of at most :attr:`LimiterConfig.bandwidth` bytes,
      carrying the same opcode (read/write) as the parent. The
      addresses emitted are ``parent.addr``, ``parent.addr +
      bandwidth``, ``parent.addr + 2*bandwidth``, ... up to
      ``parent.addr + parent.size``.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **One request in emission at a time.** A master that wants to
      overlap requests through the limiter must keep itself below the
      bandwidth budget (size ≤ bandwidth) and expect
      ``IO_REQ_DENIED``/retry cycles otherwise.
    - **Chunk size is fixed.** Every chunk except possibly the last is
      exactly :attr:`LimiterConfig.bandwidth` bytes; the last is the
      remainder. Addresses are contiguous and increase monotonically
      within a parent.
    - **Opcode forwarded, data pointer aliased.** Each sub-request
      carries the parent's opcode and a pointer into the parent's data
      buffer offset by ``chunk_index * bandwidth``. The downstream is
      responsible for reading/writing the correct byte range.
    - **No re-ordering.** Chunks are emitted in strict address order
      and the limiter does not advance past a denied chunk, so the
      downstream sees the request in-order.
    - **No data buffering.** The limiter does not own any byte
      storage; it only holds a pointer into the upstream's buffer for
      the duration of emission. The upstream must not reuse the buffer
      until ``resp()`` has fired.
    - **No bandwidth credit accumulation.** Idle cycles do not bank
      bandwidth for later — if the limiter spends 100 cycles idle, the
      next request still emits at 1 chunk/cycle. Use a token-bucket
      shaper if leaky-bucket semantics are needed.
    - **``bandwidth == 0`` is invalid.** Configure a positive value or
      the emission loop will not make progress. No runtime check is
      performed — it is the caller's responsibility.
    - **Latency is not injected on error.** An ``IO_RESP_INVALID`` on a
      chunk does not short-circuit the remaining chunks, so a partial
      failure still consumes ``ceil(size/bandwidth)`` cycles.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    All limiter parameters live on :class:`LimiterConfig`. The value is
    read once at construction; no runtime reconfiguration.

    ``bandwidth``
        Maximum number of bytes forwarded downstream per cycle. Every
        accepted request is split into chunks of at most this many
        bytes, and exactly one chunk is forwarded per cycle. Must be
        strictly positive. Typical values: the byte width of the bus
        you are modelling (e.g. ``4`` for a 32-bit interconnect,
        ``64`` for a 512-bit memory port). Default: ``0`` — must be
        overridden.

    Parameters
    ----------
    parent : Component
        Parent component this limiter is instantiated under.
    name : str
        Local name of the limiter within ``parent``. Used for component
        path, traces, and as the lookup key for bindings.
    config : LimiterConfig
        All limiter parameters live here. The instance is taken over by
        the limiter — pass a fresh :class:`LimiterConfig` per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'Limiter (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/limiter_v2',
             'component': 'interco.limiter_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: LimiterConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/limiter_v2.cpp'])

    def i_INPUT(self) -> SlaveItf:
        """Returns the input slave port for bandwidth-shaped requests.

        Incoming io_v2 requests land here. Only one request may be in
        emission at a time; any request arriving while another is still
        emitting is answered with ``IO_REQ_DENIED`` until the limiter is
        ready again.
        """
        return SlaveItf(self, 'input', signature='io_v2')

    def o_OUTPUT(self, itf: SlaveItf):
        """Binds the downstream master port.

        Each accepted CPU request is emitted as a stream of sub-requests
        (at most ``bandwidth`` bytes per cycle) through this port.
        """
        self.itf_bind('output', itf, signature='io_v2')
