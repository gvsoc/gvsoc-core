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


ALL_ADDR = 0xFFFF_FFFF_FFFF_FFFF


def burst(cycle, addr, size, nb_beats=1, burst_id=-1, is_write=False, name=None,
          stride=None):
    d = dict(cycle=cycle, addr=addr, size=size, nb_beats=nb_beats, burst_id=burst_id,
             is_write=is_write)
    if name is not None:
        d['name'] = name
    if stride is not None:
        d['stride'] = stride
    return d


def rule(addr_min=0, addr_max=ALL_ADDR, behavior='done',
         resp_delay=0, retry_delay=0, deny_count=0):
    return dict(addr_min=addr_min, addr_max=addr_max, behavior=behavior,
                resp_delay=resp_delay, retry_delay=retry_delay, deny_count=deny_count)


def build_case(case: str):
    t0_base = 0x1000_0000
    t1_base = 0x1001_0000
    window = 0x1_0000
    ok = [rule('done')]

    if case == 'single_beat':
        return dict(
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base + 0x100, size=4, name='r0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'burst4':
        return dict(
            router=dict(width=4, max_input_pending_size=64),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'two_masters_contend':
        # Two masters issue a 4-beat burst at cycle 10; expect round-robin interleaving
        # between bursts (not within).
        return dict(
            router=dict(width=4, max_input_pending_size=64),
            schedule_a=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                              name='A')],
            schedule_b=[burst(cycle=10, addr=t0_base + 0x40, size=4, nb_beats=4,
                              burst_id=2, name='B')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=2,
        )

    if case == 'size_over_width':
        return dict(
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=8, name='oversized')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'deny_mid_burst':
        # Target denies the first 2 matching beats then accepts (simulates a downstream
        # stall on beat 1 and 2 of a 4-beat burst).
        rules_t0 = [rule(behavior='deny_then_done', deny_count=2, retry_delay=3)]
        return dict(
            router=dict(width=4, max_input_pending_size=64),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'granted_resp_delay':
        rules_t0 = [rule(behavior='granted', resp_delay=5)]
        return dict(
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, name='r0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'deny_then_granted':
        # Target denies the first beat twice, then accepts subsequent requests with
        # GRANTED + resp_delay=5. Exercises the DENY → retry → re-forward → GRANTED
        # → deferred resp path end-to-end.
        rules_t0 = [rule(behavior='deny_then_granted', deny_count=2, retry_delay=3,
                         resp_delay=5)]
        return dict(
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, name='r0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'cascade_single':
        # master -> router_a -> router_b -> target. Single beat. Expect 2-cycle
        # router latency (1 per router's arbiter+fsm).
        return dict(
            topology='cascade',
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base + 0x100, size=4, name='r0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'cascade_burst4':
        # master -> router_a -> router_b -> target. 4-beat burst. Expect beats to
        # pipeline: the Nth beat reaches the target at cycle master_send_cycle + 2.
        return dict(
            topology='cascade',
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, ok)],
            nb_masters=1,
        )

    if case == 'cascade_deny':
        # master -> router_a -> router_b -> target. Target denies first beat once
        # then accepts. Verifies deny propagation and id-remap chain across two routers.
        rules_t0 = [rule(behavior='deny_then_done', deny_count=1, retry_delay=3)]
        return dict(
            topology='cascade',
            router=dict(width=4, max_input_pending_size=16),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=2, burst_id=7,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    if case == 'fifo_overflow':
        # Force the router's input FIFO to fill: target denies the first beat once
        # with a long retry_delay, so the output stalls and beats back up in the
        # router FIFO. With FIFO = 8B (2 beats) and a 4-beat burst, the 3rd and 4th
        # beats should be DENIED upstream and re-sent after retry().
        rules_t0 = [rule(behavior='deny_then_done', deny_count=1, retry_delay=10)]
        return dict(
            router=dict(width=4, max_input_pending_size=8),
            schedule=[burst(cycle=10, addr=t0_base, size=4, nb_beats=4, burst_id=1,
                            name='b0')],
            targets=[('t0', t0_base, window, rules_t0)],
            nb_masters=1,
        )

    raise ValueError(f'Unknown case: {case}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)

        case = TargetParameter(
            self, name='case', value='single_beat',
            description='Which test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        topology = spec.get('topology', 'single')

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        if topology == 'cascade':
            # master -> router_a -> router_b -> target(s)
            router_a = Router(self, 'router_a', kind='beat', **spec['router'])
            router_b = Router(self, 'router_b', kind='beat', **spec['router'])
            clock.o_CLOCK(router_a.i_CLOCK())
            clock.o_CLOCK(router_b.i_CLOCK())

            master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
            clock.o_CLOCK(master.i_CLOCK())
            master.o_OUTPUT(router_a.i_INPUT(0))

            # router_a forwards through to router_b preserving the address
            # (rm_base=False, remove_offset=0 -> pass-through).
            router_a.o_MAP(router_b.i_INPUT(0), name='through',
                           base=0x1000_0000, size=0x10_0000, rm_base=False)

            for (tname, base, size, rules) in spec['targets']:
                tgt = StubTarget(self, tname, rules=rules, logname=tname)
                clock.o_CLOCK(tgt.i_CLOCK())
                router_b.o_MAP(tgt.i_INPUT(), name=tname, base=base, size=size)
            return

        router = Router(self, 'router', kind='beat', **spec['router'])
        clock.o_CLOCK(router.i_CLOCK())

        nb_masters = spec['nb_masters']
        if nb_masters == 1:
            master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
            clock.o_CLOCK(master.i_CLOCK())
            master.o_OUTPUT(router.i_INPUT(0))
        else:
            ma = StubMaster(self, 'master_a', schedule=spec['schedule_a'], logname='master_a')
            mb = StubMaster(self, 'master_b', schedule=spec['schedule_b'], logname='master_b')
            clock.o_CLOCK(ma.i_CLOCK())
            clock.o_CLOCK(mb.i_CLOCK())
            ma.o_OUTPUT(router.i_INPUT(0))
            mb.o_OUTPUT(router.i_INPUT(1))

        for (tname, base, size, rules) in spec['targets']:
            tgt = StubTarget(self, tname, rules=rules, logname=tname)
            clock.o_CLOCK(tgt.i_CLOCK())
            router.o_MAP(tgt.i_INPUT(), name=tname, base=base, size=size)


class Target(gvsoc.runner.Target):
    gapy_description = 'router_async_v2 testbench'
    model = Chip
    name = 'test'
