# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 window-based address remapper component.

This module provides the ``Remapper`` generator for the io_v2 port of
the remapper model (``remapper_v2.cpp``). The configuration struct is
the same as the v1 variant and defined here inline — fields match
``remapper.py``.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf


class RemapperConfig(Config):
    """Configuration for the io_v2 remapper component.

    Attributes
    ----------
    base : int
        Inclusive start address of the window that gets remapped. A
        request at exactly ``base`` is inside the window; a request at
        ``base + size`` is **not** (the upper bound is exclusive).
    size : int
        Size of the remap window, in bytes. If ``size == 0`` the
        remapper is effectively a passthrough — no address ever falls
        in the window.
    target_base : int
        Start address of the destination range. A request whose
        original address is ``addr`` with ``base <= addr < base + size``
        is forwarded with its address rewritten to
        ``target_base + (addr - base)``.
    """

    base: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Base address of the area to remap (inclusive)"
    ))
    size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Size of the area to remap, in bytes"
    ))
    target_base: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Base address of the destination area"
    ))


class Remapper(Component):
    """Window-based address remapper on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A one-input, one-output passthrough that rewrites the request
    address for a single contiguous window of the address map. A
    request whose address falls in ``[base, base + size)`` is
    forwarded with its address changed to
    ``target_base + (addr - base)``. Requests landing outside that
    window are forwarded verbatim.

    Typical uses:

    - Aliasing a private peripheral window into the SoC's address map
      without changing the underlying peripheral's register layout.
    - Shifting a ROM or flash region's "visible" address at boot time
      while keeping the actual storage at its physical location.
    - Presenting a bank-local address window to a CPU that wants to
      see everything at a canonical base (e.g. a secondary boot core
      that expects its vector table at 0x0000 but whose actual memory
      lives at 0x8000).

    This is the io_v2 port of :class:`interco.remapper.Remapper`. Only
    the IO-side plumbing is new:

    - input and output ports carry the ``io_v2`` signature
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
    - responses travel back via ``input_itf.resp(req)`` — no v1
      ``resp_port`` indirection
    - back-pressure uses the AXI-like ``retry()`` handshake: on a
      downstream DENY the remapper returns DENIED upstream, restores
      the request's original address, and propagates ``retry()`` on
      the input when the output clears

    Address rewrite
    ~~~~~~~~~~~~~~~

    The window boundaries are half-open — ``addr == base`` is inside,
    ``addr == base + size`` is outside:

    .. code-block:: text

        in_window   = (size > 0) && (base <= addr < base + size)
        forwarded   = in_window ? (target_base + (addr - base)) : addr

    The rewrite is pure linear translation — there is no shifting,
    masking, or gap handling. If the window has any internal
    structure (unmapped sub-regions, different access sizes), that
    must be modelled by whatever sits behind this remapper.

    Request flow
    ~~~~~~~~~~~~

    - **Incoming request**: the remapper checks whether the request
      address falls in the configured window. If yes, the address is
      rewritten in place on the ``IoReq`` object; if no, the address
      is left untouched. The request is then forwarded to the output.
    - **Downstream returns DONE**: the remapper returns DONE up with
      no change. The request object retains the rewritten address,
      which is normal for io_v2 forwarders — masters that re-read
      ``req->addr`` after completion will see the target address.
    - **Downstream returns GRANTED**: the remapper returns GRANTED
      up. When the downstream later calls ``resp(req)``, the remapper
      relays the call on its own slave port.
    - **Downstream returns DENIED**: the remapper restores the
      original address (so a later resend from the upstream master
      looks byte-for-byte like the first attempt), latches
      ``input_needs_retry``, and returns DENIED up. When the
      downstream fires ``retry()`` the remapper relays the retry on
      the input port.
    - **DONE with ``IO_RESP_INVALID``**: passed through as-is — the
      remapper does not distinguish error responses from successful
      ones beyond forwarding what the downstream reported.

    Timing model
    ~~~~~~~~~~~~

    The remapper is **big-packet, zero-latency**: it never calls
    ``inc_latency`` and never schedules a ClockEvent. Whatever
    latency the downstream annotates on a sync ``IO_REQ_DONE`` is
    observed directly by the upstream; whatever wall-clock delay the
    downstream introduces via a deferred ``resp()`` is observed
    directly too. The remapper costs exactly one address comparison
    and (at most) one subtraction/addition.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — master-facing. Accepts reads
      and writes of any size.
    - **OUTPUT** (master, ``io_v2``) — downstream. Receives the
      original request with its address either rewritten (if in
      window) or unchanged (if outside).

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **Single window.** The remapper supports exactly one
      ``(base, size, target_base)`` triple. For multi-window maps,
      chain several remappers in series or use a
      :class:`interco.router_v2.Router` with multiple mappings.
    - **No straddle splitting.** A request whose address range
      crosses the window boundary (``addr < base+size`` but
      ``addr+size > base+size``) is rewritten based on its base
      address alone — the bytes beyond the window would land at
      ``target_base + size + k`` on the downstream, which is almost
      certainly wrong. Upstreams must keep each request fully inside
      or fully outside the window.
    - **Half-open window.** ``addr == base + size`` is **outside**
      the window. This is the universal convention for byte-range
      windows; watch out when hand-writing test addresses at the
      high end.
    - **Size ≥ 2^64 is representable but unwise.** The comparison
      ``addr < base + size`` is a 64-bit sum; overflow silently
      produces nonsensical results. Keep windows well within the
      64-bit address space.
    - **No permissions, no error injection.** The remapper does not
      check access type, size, or alignment, and does not produce
      ``IO_RESP_INVALID`` on its own. Everything comes from the
      downstream.
    - **Address restoration is DENIED-only.** On ``IO_REQ_DONE`` and
      ``IO_REQ_GRANTED`` the request object retains the rewritten
      address for the rest of its life. The convention is that the
      master does not inspect ``req->addr`` after completion; if
      yours does, wrap the remapper behind an adapter that snapshots
      and restores the address.
    - **``target_base`` can equal ``base`` for a no-op remap.** With
      ``target_base == base``, in-window requests are rewritten to
      the same value they had — functionally a passthrough but with
      the DENY/RETRY bookkeeping still exercised. Useful for testing
      the plumbing without perturbing the address map.

    Configuration fields
    ~~~~~~~~~~~~~~~~~~~~

    All remapper parameters live on :class:`RemapperConfig`. Values
    are read once at construction; no runtime reconfiguration.

    ``base``
        Inclusive start of the source window. Any valid 64-bit
        address. Default: ``0``.

    ``size``
        Length of the source window, in bytes. ``0`` disables the
        remap entirely (every request is forwarded with its original
        address). Default: ``0``.

    ``target_base``
        Start of the destination range. In-window requests are
        forwarded with ``addr_out = target_base + (addr_in - base)``.
        Default: ``0``.

    Example
    ~~~~~~~

    Alias the 64 KiB range starting at ``0x0000_0000`` (for instance
    a core's boot window) onto a flash region at
    ``0x2000_0000``:

    .. code-block:: python

        alias = Remapper(self, 'alias', config=RemapperConfig(
            base=0x00000000, size=0x10000,
            target_base=0x20000000))
        core.o_FETCH(alias.i_INPUT())
        alias.o_OUTPUT(flash.i_INPUT())

    A fetch at ``0x0000_0100`` reaches ``flash`` at
    ``0x2000_0100``; a fetch at ``0x0001_0000`` is outside the
    window and is forwarded unchanged to whatever ``flash`` has at
    that offset.

    Parameters
    ----------
    parent : Component
        Parent component this remapper is instantiated under.
    name : str
        Local name of the remapper within ``parent``. Used for
        component path, traces, and as the lookup key for bindings.
    config : RemapperConfig
        All remapper parameters live here. The instance is taken
        over by the remapper — pass a fresh :class:`RemapperConfig`
        per instance.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'Remapper (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/interco/remapper_v2',
             'component': 'interco.remapper_v2'},
        ],
    }

    def __init__(self, parent: Component, name: str, config: RemapperConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/remapper_v2.cpp'])

    def i_INPUT(self) -> SlaveItf:
        """Returns the single input slave port.

        Accepts every io_v2 request. The remapper inspects the address
        against the configured window and either rewrites it (in
        window) or leaves it alone (outside window) before forwarding
        to the output master port.
        """
        return SlaveItf(self, 'input', signature='io_v2')

    def o_OUTPUT(self, itf: SlaveItf):
        """Binds the downstream master port.

        Receives the request with its address either rewritten
        (``target_base + (addr - base)``) or unchanged, depending on
        whether the original address was in the configured window.
        """
        self.itf_bind('output', itf, signature='io_v2')
