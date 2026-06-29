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

    # ---- Async response cases (target returns GRANTED + one resp() per req) ----
    # Exercises the adapter's async-resp path: the read sub-reads and the write
    # are answered out-of-line via resp() rather than inline IO_REQ_DONE.

    if case_name == 'async_read':
        return {
            'target': dict(latency=1, async_resp=True),
            'schedule': [dict(cycle=10, addr=BASE, size=8, name='r0')],
        }

    if case_name == 'async_multi':
        # 32 B read -> 4 beat-sized sub-reads, each answered async; the adapter
        # keeps several outstanding and drains them in order.
        return {
            'target': dict(latency=2, async_resp=True),
            'schedule': [dict(cycle=10, addr=BASE, size=32, burst_id=9, name='r0')],
        }

    if case_name == 'async_write':
        return {
            'target': dict(latency=1, async_resp=True),
            'schedule': [dict(cycle=10, addr=BASE, size=32, is_write=True,
                              name='w0')],
        }

    # ---- Request-path back-pressure (target denies, then retries) ----
    # The target returns IO_REQ_DENIED for the first deny_count requests and
    # calls in.retry() retry_delay cycles later; the adapter must hold the denied
    # sub-read/write and re-send it synchronously inside retry().

    if case_name == 'deny_read':
        # 32 B read: the target denies the first two sub-reads.
        return {
            'target': dict(latency=1, deny_count=2, retry_delay=3),
            'schedule': [dict(cycle=10, addr=BASE, size=32, burst_id=4, name='r0')],
        }

    if case_name == 'deny_write':
        return {
            'target': dict(latency=1, deny_count=1, retry_delay=2),
            'schedule': [dict(cycle=10, addr=BASE, size=8, is_write=True,
                              name='w0')],
        }

    if case_name == 'deny_async':
        # Combine downstream deny/retry with the async resp path.
        return {
            'target': dict(latency=2, async_resp=True, deny_count=2, retry_delay=2),
            'schedule': [dict(cycle=10, addr=BASE, size=32, name='r0')],
        }

    # ---- Combined: async target + upstream response back-pressure ----
    if case_name == 'async_bp':
        return {
            'target': dict(latency=2, async_resp=True),
            'schedule': [dict(cycle=10, addr=BASE, size=32, burst_id=2,
                              deny_beats=[1, 3], retry_delay=2, name='r0')],
        }

    # ---- Read-burst limit (request-path back-pressure) ----
    if case_name == 'burst_limit':
        # 5 single-beat reads issued one per cycle to a slow target (latency 8).
        # With the default max_read_bursts=4, the 5th read is back-pressured
        # (IO_REQ_DENIED) until the 1st burst completes and frees a slot; the
        # master holds it and re-sends on retry. All 5 still complete.
        return {
            'target': dict(latency=8),
            'schedule': [dict(cycle=10 + i, addr=BASE + i * 8, size=8,
                              name=f'r{i}') for i in range(5)],
        }

    # ---- Negative test: out-of-order responses must trip the in-order assert ----
    # The async target answers sub-reads with adjacent pairs swapped, so a beat
    # arrives that is not the oldest outstanding sub-read. The adapter must abort
    # (asserts-mode) rather than deliver a scrambled beat stream.
    if case_name == 'ooo':
        return {
            'target': dict(latency=2, async_resp=True, reorder_resp=True),
            'schedule': [dict(cycle=10, addr=BASE, size=32, name='r0')],
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

        # Beat master -> SingleReq target: the framework auto-inserts
        # IoV2BeatToSingleReqAdapter between them.
        master.o_OUTPUT(target.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'io_v2_beat_to_single_req_adapter testbench'
    model = Chip
    name = 'test'
