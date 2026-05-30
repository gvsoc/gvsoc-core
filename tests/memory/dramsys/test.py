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
from gvrun.parameter import TargetParameter

from stub_master_v1 import StubMasterV1


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

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='read_basic',
            description='Which dramsys testbench case to run', cast=str,
        ).get_value()

        spec = build_case(case)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=1_000_000_000)

        ddr = memory.dramsys.Dramsys(self, 'ddr')
        clock.o_CLOCK(ddr.i_CLOCK())

        master = StubMasterV1(self, 'master', schedule=spec['schedule'],
                              logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(ddr.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'dramsys wrapper testbench'
    model = Chip
    name = 'test'
