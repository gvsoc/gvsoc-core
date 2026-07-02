"""Stand-alone testbench for the memory.dramsys wrapper.

Hooks a stub io v1 master directly to memory.dramsys.i_INPUT (no SoC, no CPU).
Each test case is selected via the ``case`` TargetParameter, which picks a
build_case dict with a schedule of io requests to fire.
"""

from __future__ import annotations

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
import memory.dramsys
import interco.router_v2
from gvrun.parameter import TargetParameter

from stub_master_v1 import StubMasterV1
from stub_master_v2 import StubMasterV2


def build_case(case_name: str) -> dict:
    if case_name == 'read_basic':
        # Single 4-byte read at 0x0. DRAMSys returns whatever the backing
        # store holds (uninitialised, but the wrapper must successfully
        # round-trip the request through PENDING + async resp).
        return {
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'write_then_read':
        # Write 0xdeadbeef at 0x100, then read it back from the same
        # address. The read must observe the same 4 bytes — this is the
        # minimum end-to-end correctness check of the wrapper's
        # write_buffer / strobe + read-reassembly path.
        return {
            'schedule': [
                dict(cycle=10, addr=0x100, size=4, is_write=True,
                     name='w', data_hex='efbeadde'),
                dict(cycle=200, addr=0x100, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'large_burst_checksum':
        # N iterations of 4 KB write + 4 KB read, each at its own 4 KB
        # aligned address and with its own deterministic byte pattern.
        # Issues all N writes back-to-back (10 cycles apart) then all N
        # reads back-to-back from cycle 5000. Stresses (a) the wrapper's
        # multi-chunk paraSendRequest slicing on every burst, (b) multiple
        # in-flight IoReqs queued in pending_read_req_queue and tracked
        # by the master's nb_inflight counter, and (c) — if DRAMSys runs
        # out of capacity — the wrapper's IO_REQ_DENIED + reqCallback
        # grant/replay path. The XOR checksum logged by the master on
        # every completion validates that each burst round-tripped intact
        # at its own address.
        size = 4096
        n_iters = 8
        schedule = []
        for i in range(n_iters):
            # Uniform period-256 fill: XOR of all 4096 bytes is 0 by
            # construction (16 full cycles cancel). Stamp (i+1) into byte
            # 0 so the XOR checksum is (i+1) and distinguishes iterations
            # — otherwise a misrouted read could return zeros and pass.
            base = bytearray(((j * 13 + 17) & 0xFF) for j in range(size))
            base[0] ^= (i + 1)
            schedule.append(dict(
                cycle=10 + i * 10, addr=i * 0x1000, size=size, is_write=True,
                name=f'w{i}', data_hex=bytes(base).hex(),
            ))
        for i in range(n_iters):
            schedule.append(dict(
                cycle=5000 + i * 10, addr=i * 0x1000, size=size, is_write=False,
                name=f'r{i}',
            ))
        return {'schedule': schedule}

    if case_name == 'read_loop':
        # Write a 4 KB pattern once, then loop N serialised reads of the
        # same region: each read fires only after the previous read's
        # RESP (via the stub master's chain_to_prev mechanism). Exercises
        # the wrapper's read path repeatedly with a clean per-iteration
        # boundary — no overlap, no DENIED, no in-flight queue depth.
        # The N RESPs land at N distinct cycles, which is the cleanest
        # way to visualise per-burst DRAMSys latency on the GUI timeline.
        size = 4096
        n_iters = 4
        base = bytearray(((j * 13 + 17) & 0xFF) for j in range(size))
        base[0] ^= 0xa5     # break the period-256 XOR=0 degeneracy
        pattern_hex = bytes(base).hex()
        schedule = [
            dict(cycle=10, addr=0x0, size=size, is_write=True,
                 name='w', data_hex=pattern_hex),
        ]
        for i in range(n_iters):
            schedule.append(dict(
                cycle=0, chain_to_prev=True,
                addr=0x0, size=size, is_write=False, name=f'r{i}',
            ))
        return {'schedule': schedule}

    if case_name == 'v2_read_basic':
        # Single-beat v2 read at addr 0, size == HBM2 access_size (32 B).
        # Wrapper returns GRANTED, then one beat resp with
        # is_first==is_last==true. Smoke test for the v2 read path.
        return {
            'version': 2,
            'schedule': [
                dict(cycle=10, addr=0x0, size=32, is_write=False, name='r',
                     burst_id=0),
            ],
        }

    if case_name == 'v2_write_then_read':
        # Single-beat write (1 byte at 0x100, beat-form: is_first=is_last=true,
        # one beat == whole burst, burst_id=0) then a single-beat read of
        # the same byte. Checker validates the read returns the same value
        # via the XOR checksum.
        return {
            'version': 2,
            'schedule': [
                dict(cycle=10, addr=0x100, size=1, is_write=True,
                     name='w', data_hex='a5', burst_id=0),
                dict(cycle=200, addr=0x100, size=1, is_write=False,
                     name='r', burst_id=1),
            ],
        }

    if case_name == 'v2_beat_read_stream':
        # 4 KB v2 read -> 128 beats of 32 B (access_size). Validate strict
        # cycle ordering on each beat and cumulative XOR.
        size = 4096
        base = bytearray(((j * 13 + 17) & 0xFF) for j in range(size))
        base[0] ^= 0xa5  # break the XOR=0 degeneracy
        # NOTE: stamping the pattern actually requires a prior write —
        # without one, DRAMSys returns whatever the backing store holds.
        # We do that with a 128-beat beat-form write first.
        beat_w = 32
        nbeats = size // beat_w
        schedule = []
        for i in range(nbeats):
            schedule.append(dict(
                cycle=10 + i * 5,
                addr=i * beat_w, size=beat_w, is_write=True,
                name=f'w{i}', data_hex=bytes(base[i*beat_w:(i+1)*beat_w]).hex(),
                burst_id=0,
                is_first=(i == 0), is_last=(i == nbeats - 1),
            ))
        schedule.append(dict(
            cycle=10 + nbeats * 5 + 5000,
            addr=0x0, size=size, is_write=False, name='r',
            burst_id=1,
        ))
        return {'version': 2, 'schedule': schedule}

    if case_name == 'v2_write_beat_form':
        # 4 KB beat-form write (128 beats of 32 B sharing burst_id=0),
        # then a single 4 KB read to verify the pattern landed.
        size = 4096
        base = bytearray(((j * 13 + 17) & 0xFF) for j in range(size))
        base[0] ^= 0xa5
        beat_w = 32
        nbeats = size // beat_w
        schedule = []
        for i in range(nbeats):
            schedule.append(dict(
                cycle=10 + i * 5,
                addr=i * beat_w, size=beat_w, is_write=True,
                name=f'w{i}', data_hex=bytes(base[i*beat_w:(i+1)*beat_w]).hex(),
                burst_id=0,
                is_first=(i == 0), is_last=(i == nbeats - 1),
            ))
        schedule.append(dict(
            cycle=10 + nbeats * 5 + 5000,
            addr=0x0, size=size, is_write=False, name='r',
            burst_id=1,
        ))
        return {'version': 2, 'schedule': schedule}

    if case_name == 'v2_read_loop':
        # Beat-form write of a 4 KB pattern (128 beats of 32 B sharing
        # burst_id=0) pipelined at 1/cycle, then loop 16 chained 4 KB
        # reads of the same region. Each read fires only after the previous
        # read's RESP via chain_to_prev. Routed through a v2 beat-kind
        # router so the traffic is visible on the GUI timeline.
        # Beat width matches the router's 16-byte width (half of DRAMSys's
        # 32 B access_size, so the framework adapter splits 32 B wrapper
        # responses into two 16 B router beats).
        beat_width = 16
        total_size = 4096
        nbeats     = total_size // beat_width
        nreads     = 16
        base = bytearray((j * 13 + 17) & 0xFF for j in range(total_size))
        base[0] = 0xa5
        schedule = []
        write_start = 10
        for b in range(nbeats):
            chunk = base[b * beat_width : (b + 1) * beat_width]
            schedule.append(dict(
                cycle=write_start + b,
                addr=b * beat_width, size=beat_width, is_write=True,
                name=f'wb{b}', data_hex=bytes(chunk).hex(), burst_id=0,
                is_first=(b == 0), is_last=(b == nbeats - 1),
            ))
        for i in range(nreads):
            schedule.append(dict(
                cycle=(write_start + nbeats + 200) if i == 0 else 0,
                chain_to_prev=(i > 0),
                addr=0x0, size=total_size, is_write=False,
                name=f'r{i}', burst_id=10 + i,
            ))
        return {'version': 2, 'schedule': schedule, 'with_router': True}

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='read_basic',
            description='Which dramsys testbench case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        version = spec.get('version', 1)
        with_router = spec.get('with_router', False)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=1_000_000_000)

        ddr = memory.dramsys.Dramsys(self, 'ddr', version=version)
        clock.o_CLOCK(ddr.i_CLOCK())

        if version == 1:
            master = StubMasterV1(self, 'master', schedule=spec['schedule'],
                                  logname='master')
        else:
            master = StubMasterV2(self, 'master', schedule=spec['schedule'],
                                  logname='master')
        clock.o_CLOCK(master.i_CLOCK())

        if with_router:
            # Beat-protocol router (kind='beat'), width=16 (half of the
            # DRAMSys access size of 32 B). The framework auto-inserts an
            # IoV2BeatAdapter between router and wrapper that splits each
            # 32-byte wrapper response into two 16-byte router beats — and
            # since DRAMSys delivers a 32-byte chunk every 2 cycles, the
            # router sees one 16-byte beat per cycle on its resp traces.
            assert version == 2, "with_router only supported on v2"
            router_cfg = interco.router_v2.RouterConfig(
                name='router', kind=interco.router_v2.KIND_BEAT, width=16)
            router = interco.router_v2.Router(self, 'router', config=router_cfg)
            clock.o_CLOCK(router.i_CLOCK())
            master.o_OUTPUT(router.i_INPUT())
            router.o_MAP_DEFAULT(ddr.i_INPUT(), name='ddr')
        else:
            master.o_OUTPUT(ddr.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'dramsys wrapper testbench'
    model = Chip
    name = 'test'
