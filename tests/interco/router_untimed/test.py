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
            'schedule': [dict(cycle=10, addr=t0_base + 0x100, size=4,
                              is_write=False, name='r0')],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'burst_back_to_back':
        # No timing: 3 requests all complete synchronously at their send cycle.
        return {
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x000, size=16, is_write=False, name='r0'),
                dict(cycle=11, addr=t0_base + 0x100, size=16, is_write=False, name='r1'),
                dict(cycle=12, addr=t0_base + 0x200, size=16, is_write=False, name='r2'),
            ],
            'targets': [('t0', t0_base, window, ok)],
        }

    if case_name == 'target_granted_delay':
        # Target returns GRANTED + resp_delay=5. Router propagates GRANTED, resp
        # arrives later at target's delay.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF,
                      behavior='granted', resp_delay=5, retry_delay=0)]
        return {
            'schedule': [dict(cycle=10, addr=t0_base, size=4,
                              is_write=False, name='r0')],
            'targets': [('t0', t0_base, window, rules)],
        }

    if case_name == 'target_deny_retry':
        # Target denies once then accepts. Router propagates DENY to master; master
        # re-sends on retry.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF,
                      behavior='deny_then_done', deny_count=1, retry_delay=3,
                      resp_delay=0)]
        return {
            'schedule': [dict(cycle=10, addr=t0_base, size=4,
                              is_write=False, name='r0')],
            'targets': [('t0', t0_base, window, rules)],
        }

    if case_name == 'out_of_mapping':
        return {
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

        router = Router(self, 'router', kind='untimed', config=RouterConfig())
        clock.o_CLOCK(router.i_CLOCK())

        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(router.i_INPUT(0))

        for (tname, base, size, rules) in spec['targets']:
            tgt = StubTarget(self, tname, rules=rules, logname=tname)
            clock.o_CLOCK(tgt.i_CLOCK())
            router.o_MAP(tgt.i_INPUT(), name=tname, base=base, size=size)


class Target(gvsoc.runner.Target):
    gapy_description = 'router_untimed testbench'
    model = Chip
    name = 'test'
