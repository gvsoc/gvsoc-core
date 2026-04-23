# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 ELF loader component.

This module provides the ``ElfLoader`` generator for the io_v2 port of
the boot-time binary loader (``loader_v2.cpp``).
"""

from __future__ import annotations

import gvsoc.systree


class ElfLoader(gvsoc.systree.Component):
    """ELF binary loader on the io_v2 protocol.

    Overview
    ~~~~~~~~

    A boot-time helper that streams one or more ELF binaries from the
    host filesystem into simulated memory, then (optionally) hands the
    target processor an entry address and a go-signal so it can start
    executing. Typical use is to replace a real bootloader in the
    simulation — the ``ElfLoader`` sits on an interconnect that can
    reach every memory the ELF images target, and it emits write
    transactions to populate those memories before the CPU leaves
    reset.

    The loader accepts any number of ELF32 or ELF64 files. Their
    ``PT_LOAD`` program segments are discovered at reset, and the
    file-backed bytes (``p_filesz``) are copied to the target
    addresses; the trailing ``bss`` region (``p_memsz - p_filesz``) is
    written as zeros. Transfers are chunked at 64 KiB and serialized —
    exactly one chunk is in flight at any time.

    This is the io_v2 port of :class:`utils.loader.loader.ElfLoader`.
    Only the IO-side plumbing is new:

    - ``out`` port is an io_v2 master; responses and retries are
      routed through its callbacks
    - status is ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` / ``IO_REQ_DENIED``
      with a response-status sideband
      (``IO_RESP_OK`` / ``IO_RESP_INVALID``)
    - downstream DENY is handled via the AXI-like ``retry()``
      handshake: the chunk is held and re-sent when the output fires
      its retry
    - downstream error (``IO_RESP_INVALID``) is logged and the loader
      moves on to the next chunk instead of hanging (v1's behaviour
      on any ``IO_REQ_INVALID``)

    Boot sequence
    ~~~~~~~~~~~~~

    The loader performs its work in a deterministic order at reset:

    1. For every configured binary, mmap it, parse its ELF header,
       and queue one ``Section`` per non-empty ``PT_LOAD`` segment.
       Separate ``bss`` fills are queued as zero sections.
    2. If ``entry_addr`` is set, queue a section that writes the
       entry address (4 bytes for ELF32, 8 for ELF64) at that
       address.
    3. If ``fetchen_addr`` is set, queue a section that writes
       ``fetchen_value`` at that address, same width as the entry.
    4. Schedule the streaming event one cycle after reset
       de-assertion.
    5. The event walks the section queue one chunk at a time. After
       the last byte of the last section has been acknowledged, the
       loader pulses the ``entry`` wire (with the entry address)
       and then the ``start`` wire (with ``True``).

    Request flow for a chunk
    ~~~~~~~~~~~~~~~~~~~~~~~~

    - **Issue**: the event fires. The current chunk's (addr, data,
      size) snapshot is written into the reused ``IoReq`` and the
      write is forwarded through ``out``.
    - **DONE**: the downstream absorbed the chunk synchronously. If
      its response status is ``IO_RESP_INVALID``, a warning is
      emitted; either way the section pointer advances and the next
      chunk is scheduled.
    - **GRANTED**: the downstream will acknowledge later; the
      loader waits for ``resp()`` before advancing.
    - **DENIED**: the chunk is held; the section pointer is not
      advanced. When the downstream fires ``retry()``, the loader
      re-sends the exact same chunk.

    A single idle cycle is inserted between the acknowledgement of a
    chunk and the issue of the next one, so other components get a
    chance to advance.

    Timing model
    ~~~~~~~~~~~~

    The loader itself is **big-packet, zero-latency**: it does not
    annotate ``req->latency`` and does not add any cycle cost on its
    own. Total load time is driven by the downstream: whatever
    latency each destination memory reports (either synchronously
    via ``req->latency`` or as a deferred ``resp()`` wall-clock
    time) is what the loader paces itself with, plus the fixed
    one-cycle inter-chunk gap.

    Ports
    ~~~~~

    - **OUT** (master, ``io_v2``) — write stream into the target
      memory system. Every chunk is a write; the loader never reads.
      Bind to an interconnect that reaches every memory the ELF
      images target.
    - **START** (master, ``wire<bool>``) — optional. Pulsed ``True``
      once every byte has landed and all finalisation writes are
      done.
    - **ENTRY** (master, ``wire<uint64_t>``) — optional. Pulsed with
      the entry address extracted from the last loaded ELF (or the
      explicit ``entry`` parameter, if set).

    Constraints and limitations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    - **Loading happens only at reset de-assertion.** The loader is
      not intended for runtime code injection; its sections are
      queued once in ``reset(false)`` and drained deterministically.
      If you need to reload a different binary mid-run, reset the
      component.
    - **Sections are copied verbatim.** The loader does not perform
      address relocation, permission enforcement, or alignment
      checks — every ``PT_LOAD`` segment reaches its physical
      address as-is, and an ``addr+size`` range that falls outside
      any bound memory simply triggers ``IO_RESP_INVALID`` from the
      interconnect (logged, not fatal).
    - **One chunk in flight at a time.** The loader waits for each
      chunk to be acknowledged before issuing the next. Throughput
      is therefore governed by the slowest downstream on the path,
      and a stalled interconnect will hold the loader hostage until
      it clears.
    - **64 KiB chunk granularity.** Larger sections are split into
      64 KiB writes; smaller sections fit in a single write.
    - **ELF32 or ELF64 only.** The mmap'd buffer is interpreted
      straight from its ``EI_CLASS`` byte; other object formats are
      not supported.
    - **``entry_addr`` / ``fetchen_addr`` writes follow the class
      width.** For ELF32 the entry/fetchen values are written as
      4-byte words; for ELF64 as 8-byte words. The last-loaded
      binary determines the width — if you mix ELF32 and ELF64
      binaries, the final width is whichever class ``load_elf`` saw
      last.
    - **The loader does not fail hard on missing files.** A warning
      is logged and the boot continues. If your test depends on the
      binary actually being loaded, ensure the path resolves.
    - **Stack-to-heap buffer change.** The v1 model wrote
      zero-sections from a 64 KiB stack buffer whose lifetime ended
      when ``req()`` returned. v2 uses a persistent member buffer,
      so downstream targets that defer the copy past ``req()`` (for
      example async memory models) now see valid zeros.

    Configuration
    ~~~~~~~~~~~~~

    All loader parameters are passed as constructor arguments (no
    :class:`Config` dataclass). They are serialised into the JSON
    config tree and read by the C++ model at reset.

    ``binary``
        Path to a single ELF binary to load, or ``None`` if passing
        a list via ``binaries``.

    ``binaries``
        List of ELF binary paths. Loaded in order; the last file's
        entry address wins if none is given explicitly.

    ``entry``
        Optional integer: address of the first instruction to
        execute. If set, overrides whatever the ELF header's
        ``e_entry`` field says; pulsed on the ``ENTRY`` wire at the
        end of loading.

    ``entry_addr``
        Optional integer: if set, the loader writes the entry
        address at this address in memory (4 or 8 bytes per the ELF
        class).

    ``fetchen_addr`` / ``fetchen_value``
        Optional pair: if both set, the loader writes
        ``fetchen_value`` (width per the ELF class) at
        ``fetchen_addr`` in memory. Useful for lock-step start
        schemes where a "go" flag in memory releases the core.

    Example
    ~~~~~~~

    Boot a single RISC-V core from an ELF at a custom base:

    .. code-block:: python

        loader = ElfLoader(self, 'loader',
            binary='./bin/hello.elf')
        loader.o_OUT(interco.i_INPUT())
        loader.o_ENTRY(core.i_ENTRY())
        loader.o_START(core.i_FETCHEN())

    At reset, ``loader`` writes every ``PT_LOAD`` byte of
    ``hello.elf`` through the interconnect, then pulses the
    entry-point wire with ``e_entry`` and the start wire with
    ``True``. The core's fetch-enable goes high; it starts
    executing at the entry address.

    Parameters
    ----------
    parent : Component
        Parent component this loader is instantiated under.
    name : str
        Local name of the loader within ``parent``. Used for
        component path, traces, and as the lookup key for bindings.
    binary : str, optional
        Path to a single ELF binary.
    binaries : list of str, optional
        Paths to multiple ELF binaries (appended after ``binary``).
    entry : int, optional
        Explicit entry address override.
    entry_addr : int, optional
        If set, write the entry address at this memory location.
    fetchen_addr : int, optional
        If set together with ``fetchen_value``, write that value at
        this address.
    fetchen_value : int, optional
        Value to write at ``fetchen_addr``.
    """

    # Developer-manual doc registration. Discovered by AST scan at doc
    # build time; the class docstring above is rendered via autoclass.
    __gvsoc_doc__ = {
        'title': 'ElfLoader (v2)',
        'tests_dirs': [
            {'dir':       'gvsoc/core/tests/utils/loader_v2',
             'component': 'utils.loader.loader_v2'},
        ],
    }

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 binary: str = None, binaries: list = None,
                 entry: int = None, entry_addr: int = None,
                 fetchen_addr: int = None, fetchen_value=None):
        super().__init__(parent, name)

        whole_binaries = []
        if binary is not None:
            whole_binaries.append(binary)
        if binaries is not None:
            whole_binaries += binaries

        self.set_component('utils.loader.loader_v2')

        self.add_properties({'binary': whole_binaries})

        if entry is not None:
            self.add_properties({'entry': entry})
        if entry_addr is not None:
            self.add_properties({'entry_addr': entry_addr})
        if fetchen_addr is not None:
            self.add_properties({
                'fetchen_addr': fetchen_addr,
                'fetchen_value': fetchen_value,
            })

    def set_binary(self, binary: str):
        """Replace the list of binaries with a single one.

        Typically called late in the build flow when the actual ELF
        path is only known once the application has been compiled.
        """
        self.add_properties({'binary': [binary]})

    def o_OUT(self, itf: gvsoc.systree.SlaveItf):
        """Binds the output io_v2 master port.

        This port is used by the loader to inject write requests
        that populate the target memories. It should be connected
        to an interconnect that can reach every memory containing
        at least one section of the binary.
        """
        self.itf_bind('out', itf, signature='io_v2')

    def o_START(self, itf: gvsoc.systree.SlaveItf):
        """Binds the fetch-enable wire master port.

        Pulsed with ``True`` once the entire binary has been
        delivered. Typically wired to a CPU's fetch-enable input so
        the core starts executing once memory is populated.
        """
        self.itf_bind('start', itf, signature='wire<bool>')

    def o_ENTRY(self, itf: gvsoc.systree.SlaveItf):
        """Binds the entry-point wire master port.

        Pulsed with the entry address (from the ELF header or from
        the explicit ``entry`` parameter) once all sections have
        been copied. Typically wired to a CPU's entry-point input
        so the core starts at the right address.
        """
        self.itf_bind('entry', itf, signature='wire<uint64_t>')
