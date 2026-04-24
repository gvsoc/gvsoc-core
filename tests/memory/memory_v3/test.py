# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""memory_v3 testbench.

Hooks a stub io_v2 master to memory.i_INPUT. Each test case is selected
via the ``case`` TargetParameter, which picks a build_case dict with:

  - memory_kwargs: kwargs for Memory(...)
  - schedule:      list of io_v2 requests to send (optionally carrying
                   a ``data_hex`` pre-fill for writes/atomics)
"""

from __future__ import annotations

import os
import struct

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from memory.memory_v3 import Memory, MemoryV3Config
from gvrun.parameter import TargetParameter

from stub_master import StubMaster


def _write_stim(work_dir: str, name: str, data: bytes) -> str:
    os.makedirs(work_dir, exist_ok=True)
    path = os.path.join(work_dir, f'{name}.bin')
    with open(path, 'wb') as f:
        f.write(data)
    return path


def build_case(case_name: str, work_dir: str) -> dict:
    if case_name == 'read_basic':
        # Plain 4-byte read at 0x0. With init=True (default) the fresh
        # memory holds 0x57 bytes, so the master should see data=57575757.
        return {
            'config': MemoryV3Config(size=0x1000, latency=1),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'write_then_read':
        # Write a known 4-byte pattern, then read it back and verify the
        # same bytes come out.
        return {
            'config': MemoryV3Config(size=0x1000, latency=1),
            'schedule': [
                dict(cycle=10, addr=0x20, size=4, is_write=True,  name='w',
                     data_hex='deadbeef'),
                dict(cycle=20, addr=0x20, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'multi_word':
        # 16-byte write + 16-byte read round-trip. The first 8 bytes of
        # the pattern are all the master logs, so we only need
        # ``data_hex`` long enough for that prefix to be non-trivial.
        return {
            'config': MemoryV3Config(size=0x1000, latency=1),
            'schedule': [
                dict(cycle=10, addr=0x40, size=16, is_write=True,  name='w',
                     data_hex='0102030405060708090a0b0c0d0e0f10'),
                dict(cycle=30, addr=0x40, size=16, is_write=False, name='r'),
            ],
        }

    if case_name == 'latency_nonzero':
        # latency=5 → inline DONE must carry req->latency >= 5.
        return {
            'config': MemoryV3Config(size=0x1000, latency=5),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'width_bandwidth':
        # width_log2=2 (4 bytes per cycle). A 32-byte request takes 8
        # cycles of bandwidth. Back-to-back requests should observe the
        # second one's latency being bumped by the first's tail.
        return {
            'config': MemoryV3Config(size=0x1000, latency=0, width_log2=2),
            'schedule': [
                dict(cycle=10, addr=0x0,  size=32, is_write=False, name='r0'),
                dict(cycle=11, addr=0x40, size=32, is_write=False, name='r1'),
            ],
        }

    if case_name == 'out_of_range':
        # 8-byte read starting 4 bytes before the end of a 64-byte memory:
        # crosses the boundary. Memory should refuse with IO_RESP_INVALID.
        return {
            'config': MemoryV3Config(size=0x40, latency=1),
            'schedule': [
                dict(cycle=10, addr=0x3c, size=8, is_write=False, name='r_oob'),
            ],
        }

    if case_name == 'stim_file':
        # Preload the memory with 0xDEADBEEF at offset 0; the first read
        # should return those exact bytes (LE: ef be ad de).
        stim_path = _write_stim(work_dir, 'stim_file',
                                  b'\xef\xbe\xad\xde' + b'\x00' * 28)
        return {
            'config': MemoryV3Config(size=0x20, latency=1, stim_file=stim_path),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r_stim'),
            ],
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='read_basic',
            description='Which memory_v3 test case to run', cast=str,
        ).get_value()

        work_dir = os.path.abspath(os.path.join(
            os.path.dirname(__file__), 'build', 'stims'))
        spec = build_case(case, work_dir)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        mem = Memory(self, 'mem', config=spec['config'])
        clock.o_CLOCK(mem.i_CLOCK())

        master = StubMaster(self, 'master', schedule=spec['schedule'],
                             logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(mem.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'memory_v3 testbench'
    model = Chip
    name = 'test'
