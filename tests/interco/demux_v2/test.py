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

    if case_name == 'resp_async':
        # Target 0 replies GRANTED + deferred resp after 15 cycles; target 1
        # returns DONE inline. Both CPU requests must see a RESP from the
        # master's perspective, and target 0's must land strictly after
        # cycle ~25.
        rules_async = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                            resp_delay=15, retry_delay=0)]
        return {
            'demux_config': DemuxConfig(offset=0, width=1),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='async'),
                dict(cycle=20, addr=0x1, size=4, is_write=False, name='sync'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': rules_async},
                {'name': 'mem1', 'rules': _mem_ok},
            ],
        }

    if case_name == 'deny_retry':
        # Target 0 denies with retry_delay=5, target 1 is normal. A request
        # to output 0 must be DENIED upstream, then RETRY-ed upstream only
        # when target 0 fires retry (not when target 1 does anything).
        rules_deny = [dict(addr_min=0, addr_max=0xFFFF, behavior='denied',
                           resp_delay=0, retry_delay=5)]
        return {
            'demux_config': DemuxConfig(offset=0, width=1),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='to0'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': rules_deny},
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

    if case_name == 'interleaved':
        # Mix traffic across all 4 outputs, some with async targets, to
        # check that responses from different outputs don't cross-leak.
        rules_async_slow = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                                 resp_delay=8, retry_delay=0)]
        rules_async_fast = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                                 resp_delay=2, retry_delay=0)]
        return {
            'demux_config': DemuxConfig(offset=0, width=2),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='to0'),
                dict(cycle=11, addr=0x01, size=4, is_write=False, name='to1'),
                dict(cycle=12, addr=0x02, size=4, is_write=False, name='to2'),
                dict(cycle=13, addr=0x03, size=4, is_write=False, name='to3'),
            ],
            'targets': [
                {'name': 'mem0', 'rules': _mem_ok},          # DONE inline
                {'name': 'mem1', 'rules': rules_async_slow}, # deferred long
                {'name': 'mem2', 'rules': _mem_ok},          # DONE inline
                {'name': 'mem3', 'rules': rules_async_fast}, # deferred short
            ],
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
        demux = Demux(self, 'demux', config=spec['demux_config'])
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
