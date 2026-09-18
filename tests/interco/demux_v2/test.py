# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""demux_v2 testbench.

Hooks a stub io_v2 master to demux.i_INPUT and one stub io_v2 target per
output port. Each test case is selected via the ``case`` TargetParameter,
which picks a build_case dict with:

  - demux_config: DemuxConfig for the DUT
  - schedule:    list of CPU-side io_v2 requests to send
  - targets:     list of {'name', 'rules'} dicts, one per output port
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.demux_v2 import Demux, DemuxConfig
from gvsoc.signature import IoV2SingleReq
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


# Catch-all "always DONE" rule for a stub target.
_mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                resp_delay=0, retry_delay=0)]


def _ok_targets(n):
    return [{'name': f'mem{i}', 'rules': _mem_ok} for i in range(n)]


def build_case(case_name: str) -> dict:
    if case_name == 'route_basic':
        # Offset=0, width=2 → 4 outputs, selector is the bottom 2 bits. The
        # stub target aligns each rule on 1-byte ranges so we can verify
        # routing by checking which target saw which address.
        return {
            'demux_config': DemuxConfig(offset=0, width=2),
            'schedule': [
                dict(cycle=10, addr=0x00, size=1, is_write=False, name='r0'),
                dict(cycle=20, addr=0x01, size=1, is_write=False, name='r1'),
                dict(cycle=30, addr=0x02, size=1, is_write=False, name='r2'),
                dict(cycle=40, addr=0x03, size=1, is_write=False, name='r3'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'route_bits':
        # Offset=12, width=2 → 4 outputs, selector bits [13:12]. Four
        # requests on 0x1000-stride addresses must hit each output in turn.
        return {
            'demux_config': DemuxConfig(offset=12, width=2),
            'schedule': [
                dict(cycle=10, addr=0x0000, size=4, is_write=False, name='r0'),
                dict(cycle=20, addr=0x1000, size=4, is_write=False, name='r1'),
                dict(cycle=30, addr=0x2000, size=4, is_write=False, name='r2'),
                dict(cycle=40, addr=0x3000, size=4, is_write=False, name='r3'),
            ],
            'targets': _ok_targets(4),
        }

    if case_name == 'route_write':
        # Width=1 → 2 outputs. Writes on distinct targets must preserve the
        # write opcode.
        return {
            'demux_config': DemuxConfig(offset=0, width=1),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=True, name='w0'),
                dict(cycle=20, addr=0x1, size=4, is_write=True, name='w1'),
            ],
            'targets': _ok_targets(2),
        }

    if case_name == 'split_inline':
        # split: a 16-byte read at 0xffc crosses the 0x1000 selector boundary.
        # Both outputs answer inline, so the master gets one inline DONE and
        # each target sees its own piece, in the same cycle, at full address.
        return {
            'demux_config': DemuxConfig(offset=12, width=1, split=True),
            'schedule': [
                dict(cycle=10, addr=0xffc, size=16, is_write=False, name='x0'),
                dict(cycle=20, addr=0xff0, size=16, is_write=False, name='fit'),
            ],
            'targets': _ok_targets(2),
        }

    if case_name == 'split_rebase':
        # split + rebase: each output sees addresses relative to its slice.
        return {
            'demux_config': DemuxConfig(offset=12, width=1, split=True, rebase=True),
            'schedule': [
                dict(cycle=10, addr=0x10000ffc, size=16, is_write=True, name='x0'),
                dict(cycle=20, addr=0x10001008, size=4, is_write=True, name='fit'),
            ],
            'targets': _ok_targets(2),
        }

    if case_name == 'split_denied':
        # The second output refuses its piece once: the piece is held in the
        # demux and re-sent on that output's retry, the first piece is NOT
        # sent again; the master is denied, then retried when the second piece
        # lands and its re-issue completes inline.
        return {
            'demux_config': DemuxConfig(offset=12, width=1, split=True),
            'schedule': [
                dict(cycle=10, addr=0xffc, size=16, is_write=True, name='x0'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': _mem_ok},
                {'name': 'mem1', 'rules': [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF,
                    behavior='denied_once', resp_delay=0, retry_delay=3)]},
            ],
        }

    if case_name == 'split_granted':
        # The first output answers asynchronously after 2 cycles, the second
        # inline: the master is denied, then retried and completed when the
        # slower piece answers.
        return {
            'demux_config': DemuxConfig(offset=12, width=1, split=True),
            'schedule': [
                dict(cycle=10, addr=0xffc, size=16, is_write=False, name='x0'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF,
                    behavior='granted', resp_delay=2, retry_delay=0)]},
                {'name': 'mem1', 'rules': _mem_ok},
            ],
        }

    if case_name == 'single_output':
        # Width=0 → one output. Every request, whatever the address, goes
        # to output_0. Validates the edge case.
        return {
            'demux_config': DemuxConfig(offset=0, width=0),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='a'),
                dict(cycle=20, addr=0x40, size=4, is_write=False, name='b'),
                dict(cycle=30, addr=0x1234, size=4, is_write=False, name='c'),
            ],
            'targets': _ok_targets(1),
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='route_basic',
            description='Which demux_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        # A splitting demux can deny and retry an access: it needs a
        # signature looser than the default IoV2Sync.
        demux = Demux(self, 'demux', config=spec['demux_config'],
            signature=IoV2SingleReq() if spec['demux_config'].split else None)
        clock.o_CLOCK(demux.i_CLOCK())

        # Upstream io_v2 master
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(demux.i_INPUT())

        # One downstream target per output
        for i, tgt_spec in enumerate(spec['targets']):
            t = StubTarget(self, tgt_spec['name'], rules=tgt_spec['rules'],
                           logname=tgt_spec['name'])
            clock.o_CLOCK(t.i_CLOCK())
            demux.o_OUTPUT(i, t.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'demux_v2 testbench'
    model = Chip
    name = 'test'
