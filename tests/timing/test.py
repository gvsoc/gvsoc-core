# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Testbench for the hierarchical timing-accuracy levels (gvrun.timing).

Each case builds a stub_master -> Router (kind='auto') -> Memory (v3)
platform, sets the timing level a different way (not at all, on the chip
via set_timing_level(), through a CLI ``timing.`` qualifier, or against
an explicit router kind) and asserts at generation time that the router
flavour and memory latency resolved as expected. The simulation itself
is a smoke run: one read per router path must complete.
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.router_v2 import (Router, RouterConfig, RouterMapping,
    KIND_UNTIMED, KIND_BANDWIDTH, KIND_BACKPRESSURE, KIND_BEAT)
from memory.memory_v3 import Memory, MemoryV3Config
from gvrun.parameter import TargetParameter

from stub_master import StubMaster

MEM_BASE = 0x1000_0000
SUB_BASE = 0x2000_0000
MEM_SIZE = 0x1_0000

# case name -> (level set on the chip, RouterConfig kwargs,
#               expected router kind, expected memory latency)
CASES = {
    # No level anywhere: 'auto' resolves to the built-in default 'timed'.
    'default':        (None,         {},                KIND_BANDWIDTH, 1),
    # Generator-set level on the chip root.
    'functional':     ('functional', {},                KIND_UNTIMED,   0),
    'cycle':          ('cycle',      {'width': 8},      KIND_BEAT,      1),
    # Nearest-fallback: no beat width, so 'cycle' snaps to 'timed'.
    'cycle_no_width': ('cycle',      {},                KIND_BANDWIDTH, 1),
    # An explicit kind always wins over the level (which still governs
    # the memory latency).
    'explicit_wins':  ('functional', {'kind': KIND_BACKPRESSURE, 'bandwidth': 4},
                                                        KIND_BACKPRESSURE, 0),
    # Global level set inside the target string ('test:...:timing=functional',
    # see testset.cfg): same expectations as 'functional' without any
    # generator-set level.
    'global_functional': (None,      {},                KIND_UNTIMED,   0),
    # Level set from the CLI qualifier 'timing.r_sub=cycle' (see
    # testset.cfg): only the second router goes beat, the first stays
    # on the default.
    'subtree':        (None,         {},                KIND_BANDWIDTH, 1),
}


def expect(condition: bool, message: str):
    if not condition:
        raise RuntimeError(f'timing testbench: {message}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='default',
            description='Which test case to run', cast=str,
        ).get_value()

        level, router_kwargs, expected_kind, expected_latency = CASES[case]
        if level is not None:
            self.set_timing_level(level)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        schedule = [dict(cycle=10, addr=MEM_BASE + 0x100, size=4,
                         is_write=False, name='r0')]
        if case == 'subtree':
            schedule.append(dict(cycle=20, addr=SUB_BASE + 0x100, size=4,
                                 is_write=False, name='r1'))

        router = Router(self, 'router', config=RouterConfig(**router_kwargs))
        clock.o_CLOCK(router.i_CLOCK())

        master = StubMaster(self, 'master', schedule=schedule, logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(router.i_INPUT(0))

        mem_cfg = MemoryV3Config(size=MEM_SIZE)
        mem = Memory(self, 'mem', config=mem_cfg)
        clock.o_CLOCK(mem.i_CLOCK())
        router.o_MAP(mem.i_INPUT(),
            RouterMapping(name='mem', base=MEM_BASE, size=MEM_SIZE))

        if case == 'subtree':
            r_sub = Router(self, 'r_sub', config=RouterConfig(width=8))
            clock.o_CLOCK(r_sub.i_CLOCK())
            mem2_cfg = MemoryV3Config(size=MEM_SIZE)
            mem2 = Memory(self, 'mem2', config=mem2_cfg)
            clock.o_CLOCK(mem2.i_CLOCK())
            router.o_MAP(r_sub.i_INPUT(0),
                RouterMapping(name='r_sub', base=SUB_BASE, size=MEM_SIZE))
            r_sub.o_MAP(mem2.i_INPUT(),
                RouterMapping(name='mem2', base=0, size=MEM_SIZE))
            expect(r_sub.config.kind == KIND_BEAT,
                f"r_sub kind is {r_sub.config.kind!r}, expected 'beat' from "
                "the timing.r_sub=cycle qualifier")
            expect(mem2_cfg.latency == 1,
                f"mem2 latency is {mem2_cfg.latency}, expected 1")

        expect(router.config.kind == expected_kind,
            f"case {case!r}: router kind is {router.config.kind!r}, "
            f"expected {expected_kind!r}")
        expect(mem_cfg.latency == expected_latency,
            f"case {case!r}: memory latency is {mem_cfg.latency}, "
            f"expected {expected_latency}")


class Target(gvsoc.runner.Target):
    gapy_description = 'timing-level testbench'
    model = Chip
    name = 'test'
