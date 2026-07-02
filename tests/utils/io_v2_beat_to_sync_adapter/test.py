# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget

BEAT_WIDTH = 8
BASE = 0x1000_0000
SIZE = 0x1_0000


def build_case(case_name: str) -> dict:
    if case_name == 'r1':
        # Single-beat read: size == beat_width -> 1 beat, round-trips the
        # master's own descriptor.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=8, name='r0')],
        }

    if case_name == 'r_multi':
        # 32 B read -> 4 beats; each a distinct adapter-allocated object, paced
        # 1/cycle.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=32, burst_id=7, name='r0')],
        }

    if case_name == 'w1':
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=8, is_write=True, name='w0')],
        }

    if case_name == 'w_multi':
        # 32 B write -> 4 write-ack beats, all round-tripping the master's req.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=32, is_write=True,
                              burst_id=3, name='w0')],
        }

    if case_name == 'lat_dur':
        # latency=2 + duration=6 -> full_latency=8 over 4 beats: the spread puts
        # the last beat at issue+8 while keeping beats >=1 cycle apart.
        return {
            'target': dict(latency=2, duration=6),
            'schedule': [dict(cycle=10, addr=BASE, size=32, name='r0')],
        }

    if case_name == 'err':
        # The sync slave reports IO_RESP_INVALID; every beat must carry it.
        return {
            'target': dict(latency=1, error=True),
            'schedule': [dict(cycle=10, addr=BASE, size=16, expect_status=1,
                              name='r0')],
        }

    if case_name == 'zero':
        # Degenerate zero-size read -> exactly one is_first=is_last zero-size beat.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=0, name='r0')],
        }

    # ---- Response-path back-pressure cases ----
    # The master denies the listed beats once, then drives out.resp_retry() a few
    # cycles later; the adapter must hold the exact beat and re-send it. Beat count
    # observed by the master is unchanged (each beat is eventually accepted once).

    if case_name == 'bp_read':
        # 32 B read -> 4 beats; back-pressure two middle beats.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=32, burst_id=5,
                              deny_beats=[1, 2], retry_delay=3, name='r0')],
        }

    if case_name == 'bp_write':
        # 32 B write -> 4 write-ack beats; back-pressure the first and last.
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=32, is_write=True,
                              deny_beats=[0, 3], retry_delay=2, name='w0')],
        }

    if case_name == 'bp_last':
        # Single-beat read: back-pressure the only beat (which is also is_last,
        # round-tripping the master's own descriptor).
        return {
            'target': dict(latency=1),
            'schedule': [dict(cycle=10, addr=BASE, size=8,
                              deny_beats=[0], retry_delay=2, name='r0')],
        }

    if case_name == 'bp_all':
        # 32 B read with EVERY beat back-pressured once: repeated hold/retry,
        # including a deny right after a previous re-send.
        return {
            'target': dict(latency=2, duration=6),
            'schedule': [dict(cycle=10, addr=BASE, size=32,
                              deny_beats=[0, 1, 2, 3], retry_delay=2, name='r0')],
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='r1',
            description='Which test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        master = StubMaster(self, 'master', schedule=spec['schedule'],
                            beat_width=BEAT_WIDTH, logname='master')
        clock.o_CLOCK(master.i_CLOCK())

        target = StubTarget(self, 'target', base=BASE, size=SIZE, logname='target',
                            **spec.get('target', {}))
        clock.o_CLOCK(target.i_CLOCK())

        # Beat master -> Sync target: the framework auto-inserts
        # IoV2BeatToSyncAdapter between them.
        master.o_OUTPUT(target.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'io_v2_beat_to_sync_adapter testbench'
    model = Chip
    name = 'test'
