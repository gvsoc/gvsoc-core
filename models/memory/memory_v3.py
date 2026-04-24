# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 SRAM backing store.

This module provides the ``Memory`` generator for the io_v2 port of
``memory_v2`` (``memory_v3.cpp``). The configuration struct
:class:`MemoryV3Config` is private to this module — v3 owns every tunable
through it, with no JSON fall-back at the C++ level. The legacy
``memory_v2`` keeps its own :class:`memory.memory_config.MemoryConfig`
and is unaffected.
"""

from __future__ import annotations

import gvsoc.systree
from config_tree import Config, cfg_field, HasSize


class MemoryV3Config(Config, HasSize):
    """Private configuration for the io_v2 ``Memory`` (v3) generator.

    Snake-cased to ``memory/memory_v3_config.hpp`` at build time; only
    ``memory_v3.cpp`` includes it. The field set is a superset of the
    legacy :class:`memory.memory_config.MemoryConfig`: the first four
    fields are identical, the rest were added so v3 can read every
    tunable out of the compiled config struct and skip ``get_js_config()``
    entirely.

    Attributes
    ----------
    size: int
        Memory size in bytes.
    atomics: bool
        True to compile in the RISC-V atomic handler. Atomics cost
        extra simulation time, so leave ``False`` when not needed.
    latency: int
        Extra latency (cycles) added to every request. Default ``1``.
    truncate: bool
        If True, the incoming address is masked with ``size - 1`` so
        the memory appears as a repeating window over the whole
        address space.
    width_log2: int
        log2 of the bandwidth (bytes per cycle). ``-1`` disables the
        bandwidth model (infinite bandwidth).
    align: int
        Alignment (bytes) requested from ``aligned_alloc`` for the
        backing buffer. ``0`` falls back to ``calloc``.
    check: bool
        Allocate a side-band bitmap for tracking uninitialised
        accesses.
    init: bool
        True to poison the backing buffer with ``0x57`` bytes at
        construction (skipped for memories ≥ 32 MiB).
    stim_file: str
        Optional path to a raw-binary file preloaded into the backing
        buffer at startup. Empty string means "no preload".
    power_trigger: bool
        True to enable the power-capture trigger (magic writes of
        ``0xabbaabba`` / ``0xdeadcaca`` at offset 0 start/stop
        capture).
    """

    size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Memory size in bytes"
    ))

    atomics: bool = cfg_field(default=False, dump=True, desc=(
        "True if the memory should support riscv atomics. "
        "Since this is slowing down the model, it should be "
        "set to True only if needed"
    ))

    latency: int = cfg_field(default=1, dump=True, desc=(
        "Specify extra latency which will be added to any "
        "incoming request"
    ))

    truncate: int = cfg_field(default=True, dump=True, desc=(
        "If true, this truncates the global input address with the "
        "memory size to make it relative to the memory"
    ))

    width_log2: int = cfg_field(default=-1, dump=True, desc=(
        "log2 of the bytes/cycle bandwidth (-1 disables the bandwidth model)"
    ))

    align: int = cfg_field(default=0, dump=True, desc=(
        "Alignment in bytes requested from aligned_alloc for the backing buffer"
    ))

    check: bool = cfg_field(default=False, dump=True, desc=(
        "Allocate a bitmap for tracking uninitialised accesses"
    ))

    init: bool = cfg_field(default=True, dump=True, desc=(
        "True to poison the backing buffer with 0x57 bytes at construction"
    ))

    stim_file: str = cfg_field(default="", dump=True, desc=(
        "Path to a raw-binary file preloaded into the backing buffer "
        "(empty string means no preload)"
    ))

    power_trigger: bool = cfg_field(default=False, dump=True, desc=(
        "Enable power-capture start/stop triggers on magic writes to offset 0"
    ))


class Memory(gvsoc.systree.Component):
    """SRAM backing store on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A byte-addressable, word-granular memory array terminating an
    io_v2 request path. Accepts reads, writes, and (optionally)
    RISC-V atomic memory operations on a single slave port; emits
    exactly one synchronous response per request. This is the
    drop-in replacement for :class:`memory.memory_v2.Memory` for
    masters that speak the io_v2 protocol — every other aspect
    (preload, power-capture trigger, bandwidth model, atomics) is
    identical. Memcheck bookkeeping is *not* carried over: v3 is a
    plain backing store.

    Pair this with io_v2 interconnect components like
    :class:`interco.router_v2.Router`,
    :class:`interco.remapper_v2.Remapper`,
    :class:`interco.splitter_v2.Splitter`, and
    :class:`utils.loader.loader_v2.ElfLoader`.

    Request flow
    ~~~~~~~~~~~~

    - **Read / Write**: served inline from the internal byte buffer
      (``mem_data``). The response carries ``IO_RESP_OK`` and returns
      ``IO_REQ_DONE`` synchronously. Reads on a powered-down memory
      return all-zeros; writes to a powered-down memory are silently
      dropped.
    - **Atomic** (``LR`` / ``SC`` / ``SWAP`` / ``ADD`` / ``XOR`` /
      ``AND`` / ``OR`` / ``MIN`` / ``MAX`` / ``MINU`` / ``MAXU``):
      served inline when ``atomics=True`` was passed at
      construction. The reservation table is keyed on
      ``req->initiator`` (a ``void *`` in v2) so different masters
      hold independent reservations, and ``SC`` returns 1 in the
      previous-value slot on failure. Without ``atomics=True`` the
      request is refused with ``IO_RESP_INVALID``.
    - **Out-of-bounds** (``addr + size > cfg.size`` after optional
      truncation): a warning is logged and the request returns
      ``IO_REQ_DONE`` with ``IO_RESP_INVALID``. No bytes are touched.

    Every accepted access contributes to the per-type statistics
    (``reads`` / ``writes`` / ``bytes_read`` / ``bytes_written``) and
    to the ``read_bw`` / ``write_bw`` bandwidth derivations, and pulses
    the VCD ``req_addr`` / ``req_size`` / ``req_is_write`` signals.

    Timing model
    ~~~~~~~~~~~~

    Fully synchronous, latency-annotated:

    .. code-block:: text

        total_latency = max(cfg.latency, max(0, next_packet_start - cycles))
                      + ceil(size / (1 << width_log2))

    The second term is only added when a bandwidth limit is
    configured (``width_log2 >= 0``). ``next_packet_start`` tracks
    when the memory's (single) port becomes free again; the formula
    matches memory_v2's ``max(latency, diff) + duration`` total to
    the cycle, except that v2 stores the whole contribution in
    ``req->latency`` (io_v2 has no ``duration`` field).

    Debug-flag bypass does not exist in io_v2 (there is no
    ``req->is_debug()``), so every access — including syscalls and
    loader debug reads — pays the same latency. For regular traffic
    this is the right behaviour; for pure-diagnostic accesses the
    user should route them through a separate, zero-latency memory
    instance if needed.

    Ports
    ~~~~~

    - **INPUT** (slave, ``io_v2``) — the memory's master-facing
      port. Accepts reads, writes, and atomics of any 1-, 2-, 4-,
      or 8-byte alignment.
    - **power_ctrl** (slave, ``wire<bool>``) — ``False`` gates the
      store (reads return zero, writes are dropped); ``True``
      re-enables it. Defaults to ``True`` at reset.
    - **meminfo** (slave, ``wire<void *>``) — a back-channel used by
      tools to read / swap the raw backing pointer (e.g. the loader
      ``ElfLoader`` can populate the buffer before the simulation
      starts). Not normally bound in runtime traffic.

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **Inline completion only.** The memory never returns
      ``IO_REQ_GRANTED`` or ``IO_REQ_DENIED``. Any request that hits
      this component will be answered within its own ``req()`` call.
      Upstream masters that need async completion must insert a
      deliberate delay component (such as
      :class:`interco.limiter_v2.Limiter`) between themselves and
      the memory.
    - **No back-pressure.** The memory has no queue and no deny
      path, so it never fires ``retry()``. Masters with heavy
      concurrent traffic must pace themselves (or rely on an
      upstream shaper).
    - **Atomics are opt-in.** The atomic path is compiled out unless
      ``atomics=True`` is passed at construction (via the
      ``CONFIG_ATOMICS`` compile flag). A build without this support
      refuses atomic opcodes with ``IO_RESP_INVALID``. Atomics cost
      latency on the model, so leave the default off where they are
      not exercised.
    - **No memcheck integration.** v3 is deliberately a plain
      backing store — no buffer-overflow tracking, no allocator
      hook, no shadow buffer. Use :class:`memory.memory_v2.Memory`
      when memcheck bookkeeping is needed.
    - **Size must satisfy the bounds check.** Any ``addr+size`` past
      ``cfg.size`` (after truncation) is rejected at the request
      level; no partial completion. Masters that want to straddle a
      bank boundary must split upstream.
    - **Truncation is coarse.** With ``truncate=True`` the incoming
      address is masked by ``cfg.size - 1``, which only behaves as
      expected when ``cfg.size`` is a power of two. Non-power-of-two
      sizes with ``truncate=True`` alias into unexpected regions.
    - **``init=True`` fills the buffer with 0x57.** Memories larger
      than 32 MiB skip the poison fill to keep startup fast. If you
      care about fresh zeros for large regions, load a stim file or
      clear the buffer explicitly.

    Configuration fields (:class:`MemoryV3Config`)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ``size``
        Memory size, in bytes. Must be a power of two when
        ``truncate=True``. No default — must be overridden.
    ``atomics``
        ``True`` to compile in the RISC-V atomic handler. Default
        ``False`` (atomics respond with ``IO_RESP_INVALID``).
    ``latency``
        Extra latency (cycles) added to every request. Default 1.
    ``truncate``
        When ``True``, the incoming address is masked by
        ``cfg.size - 1`` before bounds checking and lookup —
        effectively giving the memory a repeating address space.
        When ``False``, addresses above ``cfg.size`` fail the bounds
        check. Default ``True``.
    ``width_log2``
        log2 of the bytes-per-cycle bandwidth. ``-1`` disables the
        bandwidth model (infinite bandwidth). With ``width_log2=2``,
        the memory can move 4 bytes per cycle; a 16-byte access
        takes 4 cycles.
    ``align``
        Alignment (bytes) passed to ``aligned_alloc`` for the
        backing buffer. ``0`` falls back to ``calloc``.
    ``check``
        Allocate a side-band bitmap for tracking uninitialised
        accesses.
    ``init``
        ``True`` to poison the backing buffer with ``0x57`` at
        construction (skipped for memories ≥ 32 MiB).
    ``stim_file``
        Path to a raw-binary file loaded via ``fread`` into the
        backing store at startup. Empty string means "no preload".
    ``power_trigger``
        When ``True``, a write of ``0xabbaabba`` to offset 0 starts
        power capture; ``0xdeadcaca`` stops it and prints a measure
        line.

    Example
    ~~~~~~~

    A 64 KiB io_v2 memory with a 4-byte-per-cycle bandwidth cap and
    atomic support:

    .. code-block:: python

        mem = Memory(self, 'mem',
            config=MemoryV3Config(size=0x10000, latency=1, atomics=True,
                                    width_log2=2))
        loader.o_OUT(mem.i_INPUT())
        lim.o_OUTPUT(mem.i_INPUT())

    Parameters
    ----------
    parent : Component
        Parent component this memory is instantiated under.
    name : str
        Local name of the memory within ``parent``.
    config : MemoryV3Config
        Full configuration. Every tunable — size, latency, bandwidth,
        stim_file, power_trigger, etc. — lives on this object.
    """

    # Developer-manual doc registration. Discovered by AST scan at
    # doc build time; the class docstring above is rendered via
    # autoclass.
    __gvsoc_doc__ = {
        'title': 'Memory (v3)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/memory/memory_v3',
             'component': 'memory.memory_v3'},
        ],
    }

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 config: MemoryV3Config):
        super().__init__(parent, name, config=config)
        self.add_sources(['memory/memory_v3.cpp'])

        # Atomics cost a meaningful amount of simulation time; only
        # compile the support in when the user asked for it.
        if config.atomics:
            self.add_c_flags(['-DCONFIG_ATOMICS=1'])

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        """Returns the io_v2 input port.

        Every read / write / atomic request lands here and is served
        synchronously. The signature is ``io_v2`` — v1 masters
        cannot bind to this port; use :class:`memory.memory_v2.Memory`
        for v1 traffic.
        """
        return gvsoc.systree.SlaveItf(self, 'input', signature='io_v2')

    def gen_gui(self, parent_signal):
        import gvsoc.gui
        top = gvsoc.gui.Signal(self, parent_signal, name=self.name, path="req_addr",
            groups=['regmap', 'memory', 'active'])

        gvsoc.gui.Signal(self, top, "req_size",     path="req_size",     groups=['regmap'])
        gvsoc.gui.Signal(self, top, "req_is_write", path="req_is_write", groups=['regmap'])
