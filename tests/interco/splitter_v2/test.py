# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""splitter_v2 testbench.

Hooks a stub io_v2 master to splitter.i_INPUT and one stub io_v2 target
per output. Each test case is selected via the ``case`` TargetParameter,
which picks a build_case dict with:

  - splitter_config: SplitterConfig for the DUT
  - schedule:        list of CPU-side io_v2 requests to send
  - targets:         list of {'name', 'rules'} dicts, one per output
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.splitter_v2 import Splitter, SplitterConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


_mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                resp_delay=0, retry_delay=0)]


def _ok_targets(n):
    return [{'name': f'mem{i}', 'rules': _mem_ok} for i in range(n)]


def build_case(case_name: str) -> dict:
    if case_name == 'aligned_full':
        # input_width=16, output_width=4 → 4 outputs. Aligned 16-byte req at
        # addr 0 → 4 x 4-byte chunks, one per output, each at the expected
        # address.
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='full'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'unaligned_head':
        # Start address not aligned to output_width. First chunk must be
        # truncated to the next boundary; subsequent chunks are aligned and
        # full-width.
        # addr=0x02, size=12 → output_0: [0x02, 0x04) (2 bytes);
        # output_1: [0x04, 0x08) (4); output_2: [0x08, 0x0C) (4);
        # output_3: [0x0C, 0x0E) (2).
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x02, size=12, is_write=False, name='head'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'partial_fanout':
        # size < input_width → only a prefix of outputs is used, the others
        # stay idle. size=5, addr=0 → output_0: 4 bytes at 0; output_1: 1
        # byte at 4; output_2 and output_3 idle.
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=5, is_write=False, name='small'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'write_split':
        # Writes fan out the same way; each output must see is_write=1.
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=True, name='w'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'async_resp':
        # Each output replies asynchronously with a different delay. The
        # parent completes only after the slowest chunk has answered.
        rules = {
            'mem0': [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                          resp_delay=5, retry_delay=0)],
            'mem1': [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                          resp_delay=10, retry_delay=0)],
            'mem2': [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                          resp_delay=15, retry_delay=0)],
            'mem3': [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                          resp_delay=20, retry_delay=0)],
        }
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='async'),
            ],
            'targets': [{'name': n, 'rules': r} for n, r in rules.items()],
        }

    if case_name == 'output_deny_retry':
        # Only output 0 DENYs (retry_delay=5). The other three complete
        # inline. The parent cannot complete until the denied chunk
        # eventually gets through; the master must NOT see any DENIED
        # (the upstream back-pressure only fires on a second CPU request,
        # not on sub-request deny).
        rules_deny = [dict(addr_min=0, addr_max=0xFFFF, behavior='denied',
                           resp_delay=0, retry_delay=5)]
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='mixed'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': rules_deny},
                {'name': 'mem1', 'rules': _mem_ok},
                {'name': 'mem2', 'rules': _mem_ok},
                {'name': 'mem3', 'rules': _mem_ok},
            ],
        }

    if case_name == 'upstream_deny':
        # A second CPU request arrives while the first is still stuck on a
        # slow output (async). The splitter must DENY it and RETRY
        # upstream once the first completes.
        rules_slow = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                           resp_delay=30, retry_delay=0)]
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='slow1'),
                dict(cycle=12, addr=0x10, size=16, is_write=False, name='slow2'),
            ],
            'targets': [{'name': f'mem{i}', 'rules': rules_slow}
                         for i in range(4)],
        }

    if case_name == 'partial_invalid':
        # One output returns IO_RESP_INVALID on its chunk. Others return
        # OK. Parent must latch IO_RESP_INVALID, yet every chunk is still
        # accounted for (the splitter does NOT abort remaining chunks).
        rules_bad = [dict(addr_min=0, addr_max=0xFFFF, behavior='done_invalid',
                          resp_delay=0, retry_delay=0)]
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='partial'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': _mem_ok},
                {'name': 'mem1', 'rules': rules_bad},
                {'name': 'mem2', 'rules': _mem_ok},
                {'name': 'mem3', 'rules': _mem_ok},
            ],
        }

    if case_name == 'oversize_reject':
        # Request bigger than input_width: the splitter refuses up front
        # with IO_REQ_DONE + IO_RESP_INVALID. No output sees any REQ.
        return {
            'splitter_config': SplitterConfig(input_width=16, output_width=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=32, is_write=False, name='huge'),
            ],
            'targets': _ok_targets(4),
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='aligned_full',
            description='Which splitter_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        splitter = Splitter(self, 'splitter', config=spec['splitter_config'])
        clock.o_CLOCK(splitter.i_CLOCK())

        # Upstream io_v2 master
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(splitter.i_INPUT())

        # One downstream target per output
        for i, tgt in enumerate(spec['targets']):
            t = StubTarget(self, tgt['name'], rules=tgt['rules'],
                           logname=tgt['name'])
            clock.o_CLOCK(t.i_CLOCK())
            splitter.o_OUTPUT(i, t.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'splitter_v2 testbench'
    model = Chip
    name = 'test'
