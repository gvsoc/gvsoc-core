# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.router_v2 import Router
from interco.router_config import RouterConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


def build_case(case_name: str) -> dict:
    t0_base = 0x1000_0000
    window = 0x1_0000
    ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
               resp_delay=0, retry_delay=0)]

    if case_name == 'done_passthrough':
        return {
            'router': dict(latency=0, config=RouterConfig(bandwidth=0)),
            'schedule': [dict(cycle=10, addr=t0_base + 0x100, size=4,
                              is_write=False, name='r0')],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'no_deny_burst':
        # bandwidth=4, size=16 per request -> 4-cycle burst_duration. Master pushes
        # 3 requests back-to-back at cycles 10, 11, 12. Expect ALL accepted
        # immediately (no DENY), but responses stagger based on bandwidth.
        return {
            'router': dict(latency=0, config=RouterConfig(bandwidth=4)),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x000, size=16, is_write=False, name='r0'),
                dict(cycle=11, addr=t0_base + 0x100, size=16, is_write=False, name='r1'),
                dict(cycle=12, addr=t0_base + 0x200, size=16, is_write=False, name='r2'),
            ],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'bandwidth_latency_stack':
        # Verify that each successive request's response cycle = prior + 4 (the
        # burst_duration for a 16B request at 4B/cycle).
        return {
            'router': dict(latency=0, config=RouterConfig(bandwidth=4)),
            'schedule': [
                dict(cycle=0, addr=t0_base, size=16, is_write=False, name='r0'),
                dict(cycle=0, addr=t0_base + 0x10, size=16, is_write=False, name='r1'),
                dict(cycle=0, addr=t0_base + 0x20, size=16, is_write=False, name='r2'),
                dict(cycle=0, addr=t0_base + 0x30, size=16, is_write=False, name='r3'),
            ],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'input_throttle_two_outputs':
        # One master pushes three size=16 requests at cycles 10, 11, 12 targeting
        # three DIFFERENT outputs. Without the per-input watermark, each request
        # would see latency=4 (only its output's watermark matters). With the
        # per-input watermark, the master itself is the bottleneck: latencies should
        # stack as 4, 7, 10.
        t1_base = 0x1001_0000
        t2_base = 0x1002_0000
        return {
            'router': dict(latency=0, config=RouterConfig(bandwidth=4)),
            'schedule': [
                dict(cycle=10, addr=t0_base, size=16, is_write=False, name='r0'),
                dict(cycle=11, addr=t1_base, size=16, is_write=False, name='r1'),
                dict(cycle=12, addr=t2_base, size=16, is_write=False, name='r2'),
            ],
            'targets': [
                ('t0', t0_base, window, ok),
                ('t1', t1_base, window, ok),
                ('t2', t2_base, window, ok),
            ],
        }

    if case_name == 'latency_annotation':
        # bandwidth=4, size=16 -> burst_duration=4. latency=2, mapping latency handled
        # by the router config (here: 0). Expect DONE inline with latency=6
        # (2 router latency + 4 burst_duration). Wall-clock stays at `now`.
        return {
            'router': dict(latency=2, config=RouterConfig(bandwidth=4)),
            'schedule': [dict(cycle=10, addr=t0_base, size=16,
                              is_write=False, name='r0')],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'out_of_mapping':
        return {
            'router': dict(latency=0, config=RouterConfig(bandwidth=0)),
            'schedule': [dict(cycle=10, addr=0x2000_0000, size=4,
                              is_write=False, name='bad')],
            'targets': [('t0', t0_base, window, ok)],
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='done_passthrough',
            description='Which test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        router = Router(self, 'router', kind='bandwidth', **spec['router'])
        clock.o_CLOCK(router.i_CLOCK())

        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(router.i_INPUT(0))

        for (tname, base, size, rules) in spec['targets']:
            tgt = StubTarget(self, tname, rules=rules, logname=tname)
            clock.o_CLOCK(tgt.i_CLOCK())
            router.o_MAP(tgt.i_INPUT(), name=tname, base=base, size=size)


class Target(gvsoc.runner.Target):
    gapy_description = 'router_bandwidth testbench'
    model = Chip
    name = 'test'
