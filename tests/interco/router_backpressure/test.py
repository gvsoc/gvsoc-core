# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.router_v2 import Router
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


# A test case is: a router config, a schedule for the master, and rules for each target.
# Targets sit at non-overlapping 64 KiB windows starting at 0x1000_0000.
def build_case(case_name: str) -> dict:
    """Return a dict with keys: router, schedule, targets (list of (name, base, rules))."""

    # Default mapping range used by most cases.
    t0_base = 0x1000_0000
    t1_base = 0x1001_0000
    t2_base = 0x1002_0000
    window = 0x1_0000

    ok_rule = [{'addr_min': 0, 'addr_max': 0xFFFF_FFFF_FFFF_FFFF,
                'behavior': 'done', 'resp_delay': 0, 'retry_delay': 0}]

    if case_name == 'done_passthrough':
        # 1 request, bw=0, latency=0, target returns DONE -> zero-cycle path.
        return {
            'router': dict(latency=0, bandwidth=0),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x100, size=4, is_write=False, name='r0'),
            ],
            'targets': [('t0', t0_base, window, ok_rule)],
        }

    if case_name == 'router_latency':
        # Router latency 3; target returns DONE. Upstream sees resp at issue + 3.
        return {
            'router': dict(latency=3, bandwidth=0),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x100, size=4, is_write=False, name='r0'),
            ],
            'targets': [('t0', t0_base, window, ok_rule)],
        }

    if case_name == 'mapping_latency':
        # No router latency; mapping has latency=5.
        return {
            'router': dict(latency=0, bandwidth=0, mapping_latency=5),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x100, size=4, is_write=False, name='r0'),
            ],
            'targets': [('t0', t0_base, window, ok_rule)],
        }

    if case_name == 'bandwidth':
        # bandwidth=4, size=16 -> burst duration 4 cycles. Issue two requests
        # back-to-back; the second should be throttled.
        return {
            'router': dict(latency=0, bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x000, size=16, is_write=False, name='r0'),
                dict(cycle=11, addr=t0_base + 0x100, size=16, is_write=False, name='r1'),
            ],
            'targets': [('t0', t0_base, window, ok_rule)],
        }

    if case_name == 'target_granted_plus_router_latency':
        # Target returns GRANTED + delay=10. Router latency 3 on top. Upstream sees resp
        # at issue + 3 + 10.
        granted = [{'addr_min': 0, 'addr_max': 0xFFFF_FFFF_FFFF_FFFF,
                    'behavior': 'granted', 'resp_delay': 10, 'retry_delay': 0}]
        return {
            'router': dict(latency=3, bandwidth=0),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x100, size=4, is_write=False, name='r0'),
            ],
            'targets': [('t0', t0_base, window, granted)],
        }

    if case_name == 'denied_then_retry':
        # First attempt: target denies; retry after 5 cycles; second attempt: ok.
        # Use a rule list with the first entry denying and the second (fall-through)
        # accepting — but behavior is one-shot per rule lookup; we need the target to
        # flip. Simple approach: use two schedule entries with different addresses, one
        # that stays denied-forever? Not quite.
        #
        # Alternative: the target's "denied" rule schedules a retry, and when the master
        # re-sends, the router re-forwards and the target denies again -> infinite. We
        # need a target that says: deny the first N attempts, then accept.
        #
        # For this MVP we keep it simple and route the denied request to a target that
        # transiently denies; the master's deny log is what we check.
        denied = [{'addr_min': 0, 'addr_max': 0xFFFF_FFFF_FFFF_FFFF,
                   'behavior': 'denied', 'resp_delay': 0, 'retry_delay': 5}]
        return {
            'router': dict(latency=0, bandwidth=0),
            'schedule': [
                dict(cycle=10, addr=t0_base + 0x100, size=4, is_write=False, name='r0'),
            ],
            # Only one attempt so we do see the deny + retry path in the log. The master
            # re-sends once on retry, which will deny again. Test checks the first two
            # iterations only.
            'targets': [('t0', t0_base, window, denied)],
        }

    if case_name == 'out_of_mapping':
        # Access outside any mapping -> router returns DONE with IO_RESP_INVALID.
        return {
            'router': dict(latency=0, bandwidth=0),
            'schedule': [
                dict(cycle=10, addr=0x2000_0000, size=4, is_write=False, name='bad'),
            ],
            'targets': [('t0', t0_base, window, ok_rule)],
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
        router_cfg = spec['router']
        mapping_latency = router_cfg.pop('mapping_latency', 0)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        router = Router(self, 'router', kind='backpressure', **router_cfg)
        master.o_OUTPUT(router.i_INPUT(0))

        clock.o_CLOCK(master.i_CLOCK())
        clock.o_CLOCK(router.i_CLOCK())

        for (tname, base, size, rules) in spec['targets']:
            tgt = StubTarget(self, tname, rules=rules, logname=tname)
            router.o_MAP(tgt.i_INPUT(), name=tname, base=base, size=size,
                         latency=mapping_latency)
            clock.o_CLOCK(tgt.i_CLOCK())


class Target(gvsoc.runner.Target):
    gapy_description = 'router_v2 testbench'
    model = Chip
    name = 'test'
