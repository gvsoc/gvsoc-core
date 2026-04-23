# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""cache_v4 testbench.

Hooks a stub io_v2 master to cache.i_INPUT, cache.o_REFILL to a stub io_v2
target, and a stub wire driver to the cache's control signals
(enable / flush / flush_line / flush_line_addr / flush_ack).

Each test case is selected via the ``case`` TargetParameter, which picks a
build_case dict with:
  - cache_config: CacheConfig for the DUT
  - schedule:      list of CPU-side io_v2 requests to send
  - rules:         list of stub_target behaviours (DONE / GRANTED / DENIED)
  - wires:         list of cache control-wire pulses (optional)
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from cache.cache_v4 import Cache, CacheConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget
from stub_wires import StubWires


def build_case(case_name: str) -> dict:
    # Default cache geometry for the simple cases: 128B, 16B line, 1 way -> 8 sets.
    # With ways=1 the pseudo-random LFSR is a constant (always picks way 0), so the
    # replacement behaviour is fully deterministic.
    default_cache = dict(size=128, line_size=16, ways=1,
                         refill_shift=0, refill_offset=0, refill_latency=0,
                         enabled=True)

    def cache_cfg(**overrides):
        d = dict(default_cache)
        d.update(overrides)
        return CacheConfig(**d)

    # Catch-all "always DONE" rule for the refill target.
    mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                   resp_delay=0, retry_delay=0)]

    if case_name == 'hit_basic':
        # Read addr 0x00 twice. First is a miss (refill, DONE inline from target),
        # second is a hit and must not generate a refill REQ.
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
                dict(cycle=20, addr=0x04, size=4, is_write=False, name='r1'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'miss_sync':
        # Two misses on different lines. Target returns DONE inline with 3 cycles of
        # req->latency; cache_v4 should stack refill_latency on top.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='done',
                      resp_delay=0, retry_delay=0)]
        return {
            'cache_config': cache_cfg(refill_latency=5),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
                dict(cycle=50, addr=0x40, size=4, is_write=False, name='r1'),
            ],
            'rules': rules,
        }

    if case_name == 'miss_async':
        # Target replies GRANTED with resp_delay=20. Cache must:
        #  - return GRANTED upstream (master logs GRANTED, not DONE)
        #  - reply to the master at cycle = issue + 20 via input_itf.resp()
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=20, retry_delay=0)]
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
            ],
            'rules': rules,
        }

    if case_name == 'queue_during_refill':
        # Miss A starts a long async refill. Miss B (different line) arrives during
        # the refill and must be queued (GRANTED upstream) and only processed once
        # A's refill lands. Validates refill_pending_reqs + fsm_handler.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=30, retry_delay=0)]
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='rA'),
                dict(cycle=12, addr=0x40, size=4, is_write=False, name='rB'),
            ],
            'rules': rules,
        }

    if case_name == 'bypass_done':
        # Cache disabled at reset, target returns DONE inline. The cache should
        # forward the request addr (after refill_shift/refill_offset transform) and
        # return DONE to the master without touching any line.
        return {
            'cache_config': cache_cfg(enabled=False,
                                      refill_shift=0, refill_offset=0x1000),
            'schedule': [
                dict(cycle=10, addr=0x20, size=4, is_write=False, name='b0'),
            ],
            'rules': mem_ok,
        }

    if case_name == 'bypass_async':
        # Cache disabled, target returns GRANTED + resp after 15 cycles. The
        # cache's refill_resp must recognise the req as non-&refill_req and
        # bounce it back on input_itf.resp().
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=15, retry_delay=0)]
        return {
            'cache_config': cache_cfg(enabled=False),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='b0'),
            ],
            'rules': rules,
        }

    if case_name == 'refill_denied':
        # Target denies the first two requests (with retry_delay=5) then accepts
        # the third. Cache should propagate DENIED upstream and issue retry() on
        # our behalf; master re-sends on RETRY and eventually gets DONE.
        rules = [
            dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='denied',
                 resp_delay=0, retry_delay=5),
        ]
        # Allow the target to *switch* behaviour mid-run? The current stub_target
        # doesn't support that out of the box. So this case exercises one DENY
        # then a RETRY; the second send would still get denied. To converge,
        # rewrite the rule to 'done' after the first retry — not supported yet.
        # For now, this test just verifies DENIED path up to the first retry.
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
            ],
            'rules': rules,
        }

    if case_name == 'flush_full':
        # Prime two lines, pulse `flush` at cycle 60, then re-read both lines.
        # Post-flush reads must miss and re-refill (stub_target sees two more REQs).
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='prime_a'),
                dict(cycle=20, addr=0x40, size=4, is_write=False, name='prime_b'),
                dict(cycle=80, addr=0x00, size=4, is_write=False, name='post_a'),
                dict(cycle=90, addr=0x40, size=4, is_write=False, name='post_b'),
            ],
            'rules': mem_ok,
            'wires': [
                dict(cycle=60, signal='flush', value=1),
                dict(cycle=61, signal='flush', value=0),
            ],
        }

    if case_name == 'flush_line':
        # Prime two lines, flush only line A (addr 0x00). Read both again:
        # A must miss (re-refill), B must hit (no new REQ to target).
        return {
            'cache_config': cache_cfg(),
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='prime_a'),
                dict(cycle=20, addr=0x40, size=4, is_write=False, name='prime_b'),
                dict(cycle=80, addr=0x00, size=4, is_write=False, name='post_a'),
                dict(cycle=90, addr=0x40, size=4, is_write=False, name='post_b'),
            ],
            'rules': mem_ok,
            'wires': [
                dict(cycle=60, signal='flush_line_addr', value=0x00),
                dict(cycle=61, signal='flush_line',      value=1),
                dict(cycle=62, signal='flush_line',      value=0),
            ],
        }

    if case_name == 'addr_transform':
        # refill_shift=1 and refill_offset=0x8000. A CPU access to 0x40 should
        # trigger a refill at (0x40 << 1) + 0x8000 = 0x8080.
        return {
            'cache_config': cache_cfg(refill_shift=1, refill_offset=0x8000),
            'schedule': [
                dict(cycle=10, addr=0x40, size=4, is_write=False, name='r0'),
            ],
            'rules': mem_ok,
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='hit_basic',
            description='Which cache_v4 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        cache = Cache(self, 'cache', config=spec['cache_config'])
        clock.o_CLOCK(cache.i_CLOCK())

        # Upstream io_v2 master (drives requests into the cache).
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(cache.i_INPUT())

        # Downstream io_v2 target (receives refill requests).
        target = StubTarget(self, 'mem', rules=spec['rules'], logname='mem')
        clock.o_CLOCK(target.i_CLOCK())
        cache.o_REFILL(target.i_INPUT())

        # Control wires: only instantiate if the case asks for any, to keep
        # logs terse for the simple cases.
        wires_sched = spec.get('wires', [])
        if wires_sched:
            wires = StubWires(self, 'wires', schedule=wires_sched, logname='wires')
            clock.o_CLOCK(wires.i_CLOCK())
            wires.o_ENABLE(cache.i_ENABLE())
            wires.o_FLUSH(cache.i_FLUSH())
            wires.o_FLUSH_LINE(cache.i_FLUSH_LINE())
            wires.o_FLUSH_LINE_ADDR(cache.i_FLUSH_LINE_ADDR())
            cache.o_FLUSH_ACK(wires.i_FLUSH_ACK())


class Target(gvsoc.runner.Target):
    gapy_description = 'cache_v4 testbench'
    model = Chip
    name = 'test'
