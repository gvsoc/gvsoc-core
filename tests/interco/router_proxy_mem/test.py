# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Testbench for the backdoor proxy mem_read/mem_write path.

Two cascaded v2 routers (kind selected by the `kind` target parameter)
in front of two memory_v3 instances:

    router ── mem0     @ 0x1000_0000 (size 0x1000)
           └─ router2  @ 0x2000_0000 (size 0x2000, rebased)
                 └─ mem1 @ 0x1000 (size 0x1000)
                    => entry window 0x2000_1000 .. 0x2000_1FFF

The control script (control.py) accesses both memories through the proxy
WITHOUT ever running the simulation — the whole point of the backdoor
debug-memory path. 0x2000_0000..0x2000_0FFF is a hole inside router2 and
anything outside the two top windows is unmapped at level 1; both must
fail cleanly instead of hanging.
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.router_v2 import Router, RouterConfig, RouterMapping
from memory.memory_v3 import Memory, MemoryV3Config
from gvrun.parameter import TargetParameter


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        kind = TargetParameter(
            self, name='kind', value='untimed',
            description='Router implementation kind', cast=str,
        ).get_value()

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # width / max_input_pending_size only matter for the beat kind;
        # the other kinds ignore them.
        router = Router(self, 'router', config=RouterConfig(kind=kind, width=8))
        clock.o_CLOCK(router.i_CLOCK())

        mem0 = Memory(self, 'mem0', config=MemoryV3Config(size=0x1000))
        clock.o_CLOCK(mem0.i_CLOCK())
        router.o_MAP(mem0.i_INPUT(),
                     RouterMapping(name='mem0', base=0x1000_0000, size=0x1000))

        # Second-level router, rebased: entry 0x2000_0000 + x => router2 @ x
        router2 = Router(self, 'router2', config=RouterConfig(kind=kind, width=8))
        clock.o_CLOCK(router2.i_CLOCK())
        router.o_MAP(router2.i_INPUT(0),
                     RouterMapping(name='router2', base=0x2000_0000, size=0x2000))

        mem1 = Memory(self, 'mem1', config=MemoryV3Config(size=0x1000))
        clock.o_CLOCK(mem1.i_CLOCK())
        router2.o_MAP(mem1.i_INPUT(),
                      RouterMapping(name='mem1', base=0x1000, size=0x1000))


class Target(gvsoc.runner.Target):
    gapy_description = 'router_v2 proxy backdoor testbench'
    model = Chip
    name = 'test'
