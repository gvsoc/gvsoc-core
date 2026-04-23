# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""limiter_v2 testbench.

Hooks a stub io_v2 master to limiter.i_INPUT and a stub io_v2 target to
limiter.o_OUTPUT. Each test case is selected via the ``case``
TargetParameter, which picks a build_case dict with:
  - limiter_config: LimiterConfig for the DUT
  - schedule:       list of CPU-side io_v2 requests to send
  - rules:          list of stub_target behaviours (DONE / GRANTED / DENIED)
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.limiter_v2 import Limiter, LimiterConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


def build_case(case_name: str) -> dict:
    # Catch-all "always DONE" rule for the downstream target.
    mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                   resp_delay=0, retry_delay=0)]

    if case_name == 'passthrough':
        # Single small request fits in one chunk. With bandwidth=64 a size=4
        # request is emitted in a single cycle; target DONE inline.
        return {
            'limiter_config': LimiterConfig(bandwidth=64),
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r0'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'split_chunks':
        # bandwidth=4 and size=16 => 4 chunks emitted one per cycle. Target
        # sees exactly 4 REQs; master sees one RESP only after the last chunk.
        return {
            'limiter_config': LimiterConfig(bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=0x0, size=16, is_write=False, name='big'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'busy_deny':
        # Two CPU requests arrive back-to-back. bandwidth=4, first size=16 ->
        # 4 cycles of emission. The second request arrives at cycle 11, while
        # the first is still being emitted, and must be DENIED. After the
        # first finishes, the limiter must RETRY the master so the master can
        # re-send. Both requests ultimately complete.
        return {
            'limiter_config': LimiterConfig(bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=0x00, size=16, is_write=False, name='big'),
                dict(cycle=11, addr=0x20, size=4,  is_write=False, name='small'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'downstream_granted':
        # Target replies GRANTED + deferred resp after 10 cycles. Each chunk
        # is emitted one per cycle, but responses arrive interleaved. The
        # master sees one RESP per accepted CPU request (the limiter collapses
        # per-chunk completions back into the parent).
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=10, retry_delay=0)]
        return {
            'limiter_config': LimiterConfig(bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=0x0, size=16, is_write=False, name='big'),
            ],
            'rules': rules,
        }

    if case_name == 'downstream_denied':
        # Target denies the very first chunk (retry_delay=5, then a single
        # retry; stub_target doesn't switch behaviour, so subsequent chunks
        # keep being denied). The limiter must propagate the stall by not
        # emitting further chunks until the output retries, at which point it
        # re-sends the same denied chunk.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='denied',
                      resp_delay=0, retry_delay=5)]
        return {
            'limiter_config': LimiterConfig(bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=0x0, size=16, is_write=False, name='big'),
            ],
            'rules': rules,
        }

    if case_name == 'write_split':
        # Same splitting logic on a write request: bandwidth=8, size=32 ->
        # 4 chunks. Validates that is_write is forwarded on every sub-request.
        return {
            'limiter_config': LimiterConfig(bandwidth=8),
            'schedule': [
                dict(cycle=10, addr=0x100, size=32, is_write=True, name='w0'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'back_to_back':
        # First CPU request fits in a single chunk (size==bandwidth), so the
        # limiter frees its slot immediately; a second request arriving one
        # cycle later is accepted cleanly (no DENY). Both must see a RESP.
        return {
            'limiter_config': LimiterConfig(bandwidth=8),
            'schedule': [
                dict(cycle=10, addr=0x00, size=8, is_write=False, name='r0'),
                dict(cycle=12, addr=0x40, size=8, is_write=False, name='r1'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'invalid_chunk':
        # One of the chunks hits an address range the target marks
        # done_invalid. The parent CPU request must complete with
        # IO_RESP_INVALID (latched from the failing chunk), but all chunks
        # must still be consumed — i.e. we don't abort mid-stream.
        rules = [
            dict(addr_min=0x00, addr_max=0x07, behavior='done',
                 resp_delay=0, retry_delay=0),
            dict(addr_min=0x08, addr_max=0x0F, behavior='done_invalid',
                 resp_delay=0, retry_delay=0),
            dict(addr_min=0x10, addr_max=0xFF, behavior='done',
                 resp_delay=0, retry_delay=0),
        ]
        return {
            'limiter_config': LimiterConfig(bandwidth=4),
            'schedule': [
                dict(cycle=10, addr=0x0, size=16, is_write=False, name='mixed'),
            ],
            'rules': rules,
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='passthrough',
            description='Which limiter_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        limiter = Limiter(self, 'limiter', config=spec['limiter_config'])
        clock.o_CLOCK(limiter.i_CLOCK())

        # Upstream io_v2 master.
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(limiter.i_INPUT())

        # Downstream io_v2 target.
        target = StubTarget(self, 'mem', rules=spec['rules'], logname='mem')
        clock.o_CLOCK(target.i_CLOCK())
        limiter.o_OUTPUT(target.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'limiter_v2 testbench'
    model = Chip
    name = 'test'
