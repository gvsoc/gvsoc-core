# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""loader_v2 testbench.

Generates a minimal ELF binary on the fly (``_build_elf32``), then
instantiates the loader against a stub io_v2 memory target plus a wire
sink that observes the ``start`` and ``entry`` finalisation pulses.
"""

from __future__ import annotations

import os
import struct

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from utils.loader.loader_v2 import ElfLoader
from gvrun.parameter import TargetParameter

from stub_target import StubTarget
from stub_wire_sink import StubWireSink


# -----------------------------------------------------------------------------
# Minimal ELF32 builder.
#
# Builds an ELF32-LE image with one or more PT_LOAD segments. Each segment
# specifies (paddr, filesz bytes as ``bytes``, memsz). ``memsz >= filesz``;
# the trailing zeros model a bss fill.
# -----------------------------------------------------------------------------

def _build_elf32(path: str, entry: int, segments: list) -> None:
    """Write a minimal ELF32 little-endian image to ``path``.

    ``segments`` is a list of dicts: ``{'paddr', 'data': bytes, 'memsz': int}``.
    ``memsz`` is allowed to be ``> len(data)`` — the difference is written
    as a bss section.
    """
    EHDR_SIZE = 52
    PHDR_SIZE = 32
    data_offset = EHDR_SIZE + PHDR_SIZE * len(segments)

    e_ident = b'\x7fELF' + bytes([
        1,   # EI_CLASS = ELFCLASS32
        1,   # EI_DATA  = ELFDATA2LSB
        1,   # EI_VERSION = EV_CURRENT
        0, 0, 0, 0, 0, 0, 0, 0, 0,
    ])

    ehdr = e_ident + struct.pack('<HHIIIIIHHHHHH',
        2,                 # e_type = ET_EXEC
        0xf3,              # e_machine = EM_RISCV (doesn't matter for loader)
        1,                 # e_version
        entry & 0xFFFFFFFF,
        EHDR_SIZE,         # e_phoff
        0,                 # e_shoff
        0,                 # e_flags
        EHDR_SIZE,         # e_ehsize
        PHDR_SIZE,         # e_phentsize
        len(segments),     # e_phnum
        0,                 # e_shentsize
        0,                 # e_shnum
        0,                 # e_shstrndx
    )

    phdrs = b''
    data_blob = b''
    cursor = data_offset
    for s in segments:
        filesz = len(s['data'])
        memsz  = max(s['memsz'], filesz)
        phdr = struct.pack('<IIIIIIII',
            1,                 # p_type = PT_LOAD
            cursor,            # p_offset
            s['paddr'],        # p_vaddr
            s['paddr'],        # p_paddr
            filesz,            # p_filesz
            memsz,             # p_memsz
            5,                 # p_flags = RX
            4,                 # p_align
        )
        phdrs += phdr
        data_blob += s['data']
        cursor += filesz

    with open(path, 'wb') as f:
        f.write(ehdr + phdrs + data_blob)


def _ensure_elf(work_dir: str, case_name: str, entry: int, segments: list) -> str:
    """Write an ELF into ``work_dir`` and return its absolute path."""
    os.makedirs(work_dir, exist_ok=True)
    path = os.path.join(work_dir, f'{case_name}.elf')
    _build_elf32(path, entry, segments)
    return path


# -----------------------------------------------------------------------------
# Cases
# -----------------------------------------------------------------------------

def build_case(case_name: str, work_dir: str) -> dict:
    # Catch-all "always DONE" rule for the memory target.
    mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF,
                   behavior='done', resp_delay=0, retry_delay=0)]

    if case_name == 'basic_load':
        # One segment with 16 bytes of known data.
        data = bytes(range(16))
        path = _ensure_elf(work_dir, 'basic_load', 0x1000, [
            {'paddr': 0x1000, 'data': data, 'memsz': 16},
        ])
        return {
            'binary': path,
            'rules':  mem_ok,
        }

    if case_name == 'bss_fill':
        # One segment with 4 bytes of filesz + 12 bytes of bss.
        data = b'\xde\xad\xbe\xef'
        path = _ensure_elf(work_dir, 'bss_fill', 0x1100, [
            {'paddr': 0x1100, 'data': data, 'memsz': 16},
        ])
        return {
            'binary': path,
            'rules':  mem_ok,
        }

    if case_name == 'multi_segment':
        # Two disjoint PT_LOAD segments.
        path = _ensure_elf(work_dir, 'multi_segment', 0x2000, [
            {'paddr': 0x2000, 'data': b'\x01\x02\x03\x04', 'memsz': 4},
            {'paddr': 0x3000, 'data': b'\x05\x06\x07\x08', 'memsz': 4},
        ])
        return {
            'binary': path,
            'rules':  mem_ok,
        }

    if case_name == 'entry_addr':
        # Write the entry address at 0x0 in memory in addition to the
        # regular section copy.
        data = b'\xaa\xbb\xcc\xdd'
        path = _ensure_elf(work_dir, 'entry_addr', 0x4000, [
            {'paddr': 0x4000, 'data': data, 'memsz': 4},
        ])
        return {
            'binary': path,
            'entry_addr': 0x0,
            'rules':  mem_ok,
        }

    if case_name == 'entry_override':
        # Explicit entry= overrides the ELF header's e_entry. mem isn't
        # written with the override since we don't give entry_addr — we
        # verify the override through the ENTRY wire pulse instead.
        path = _ensure_elf(work_dir, 'entry_override', 0x5000, [
            {'paddr': 0x5000, 'data': b'\x11\x22\x33\x44', 'memsz': 4},
        ])
        return {
            'binary': path,
            'entry':  0xDEADBEEF,
            'rules':  mem_ok,
        }

    if case_name == 'deny_retry':
        # Memory DENIEs the first attempt, retries after 5 cycles. The
        # loader must hold the chunk, replay on retry, and still make
        # progress. Use a single retry so the chunk ultimately lands.
        # (stub_target stays denied forever, so we'd never complete —
        # swap to a "one-shot" behaviour by restricting the deny to the
        # first window of cycles.)
        data = b'\x99\x88\x77\x66'
        path = _ensure_elf(work_dir, 'deny_retry', 0x6000, [
            {'paddr': 0x6000, 'data': data, 'memsz': 4},
        ])
        # stub_target does not support "deny once then accept" — so we
        # just test that the deny/retry handshake is exercised. The
        # checker tolerates no RESP (the chunk never lands) but looks
        # for repeated REQ/RETRY events on mem.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='denied',
                      resp_delay=0, retry_delay=5)]
        return {
            'binary': path,
            'rules':  rules,
        }

    if case_name == 'async_resp':
        # Memory replies asynchronously with a 10-cycle delay. Loader
        # must wait for the deferred resp before issuing the next
        # chunk. With a single small segment that means one round-trip.
        data = bytes(range(32))
        path = _ensure_elf(work_dir, 'async_resp', 0x7000, [
            {'paddr': 0x7000, 'data': data, 'memsz': 32},
        ])
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=10, retry_delay=0)]
        return {
            'binary': path,
            'rules':  rules,
        }

    if case_name == 'invalid_resp':
        # Memory returns IO_RESP_INVALID on one chunk. v2 must log the
        # warning and *continue* past the error (v1 hung). Split the
        # address range so only the target chunk fails; surrounding
        # ones succeed.
        data = bytes(range(8))
        path = _ensure_elf(work_dir, 'invalid_resp', 0x8000, [
            {'paddr': 0x8000, 'data': data, 'memsz': 8},
        ])
        rules = [
            dict(addr_min=0x8000, addr_max=0x8007, behavior='done_invalid',
                 resp_delay=0, retry_delay=0),
        ]
        return {
            'binary': path,
            'rules':  rules,
        }

    if case_name == 'missing_binary':
        # Non-existent binary path. The loader must log a warning and
        # not crash; no writes reach mem and no wire sync fires.
        return {
            'binary': os.path.join(work_dir, '__does_not_exist__.elf'),
            'rules':  mem_ok,
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='basic_load',
            description='Which loader_v2 test case to run', cast=str,
        ).get_value()

        # Pre-build the ELF into a per-case directory next to the
        # generated gvsoc config. Use an absolute path under /tmp so
        # the loader's mmap call resolves regardless of where gvrun
        # changes the cwd.
        work_dir = os.path.abspath(os.path.join(
            os.path.dirname(__file__), 'build', 'elfs'))
        spec = build_case(case, work_dir)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        loader_kwargs = dict(binary=spec['binary'])
        for k in ('entry', 'entry_addr', 'fetchen_addr', 'fetchen_value'):
            if k in spec:
                loader_kwargs[k] = spec[k]
        loader = ElfLoader(self, 'loader', **loader_kwargs)
        clock.o_CLOCK(loader.i_CLOCK())

        mem = StubTarget(self, 'mem', rules=spec['rules'], logname='mem')
        clock.o_CLOCK(mem.i_CLOCK())
        loader.o_OUT(mem.i_INPUT())

        sink = StubWireSink(self, 'sink', logname='sink')
        clock.o_CLOCK(sink.i_CLOCK())
        loader.o_START(sink.i_START())
        loader.o_ENTRY(sink.i_ENTRY())


class Target(gvsoc.runner.Target):
    gapy_description = 'loader_v2 testbench'
    model = Chip
    name = 'test'
