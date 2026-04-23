# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""rw_splitter_v2 testbench.

Hooks a stub io_v2 master to rw_splitter.i_INPUT, a stub io_v2 target to
rw_splitter.o_READ_OUTPUT, and another stub io_v2 target to
rw_splitter.o_WRITE_OUTPUT. Each test case is selected via the ``case``
TargetParameter, which picks a build_case dict with:

  - schedule:     list of CPU-side io_v2 requests
  - read_rules:   list of stub_target rules for the read-side downstream
  - write_rules:  list of stub_target rules for the write-side downstream
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.rw_splitter_v2 import RwSplitter, RwSplitterConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


_mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                resp_delay=0, retry_delay=0)]


def build_case(case_name: str) -> dict:
    if case_name == 'read_only':
        # One read only: reaches mem_read, mem_write is untouched.
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
            ],
            'read_rules': _mem_ok,
            'write_rules': _mem_ok,
        }

    if case_name == 'write_only':
        # One write only: reaches mem_write, mem_read is untouched.
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=True, name='w0'),
            ],
            'read_rules': _mem_ok,
            'write_rules': _mem_ok,
        }

    if case_name == 'mixed':
        # Interleaved reads and writes go to the correct output.
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r0'),
                dict(cycle=20, addr=0x04, size=4, is_write=True,  name='w0'),
                dict(cycle=30, addr=0x08, size=4, is_write=False, name='r1'),
                dict(cycle=40, addr=0x0c, size=4, is_write=True,  name='w1'),
            ],
            'read_rules': _mem_ok,
            'write_rules': _mem_ok,
        }

    if case_name == 'async_read':
        # Read downstream replies GRANTED + resp_delay=12. Write is inline.
        # Validates that async RESP from the read side reaches the master.
        async_rules = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                            resp_delay=12, retry_delay=0)]
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r_async'),
                dict(cycle=20, addr=0x04, size=4, is_write=True,  name='w_sync'),
            ],
            'read_rules': async_rules,
            'write_rules': _mem_ok,
        }

    if case_name == 'async_write':
        # Write downstream replies GRANTED + resp_delay=12. Read is inline.
        # Validates that async RESP from the write side reaches the master.
        async_rules = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                            resp_delay=12, retry_delay=0)]
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=True,  name='w_async'),
                dict(cycle=20, addr=0x04, size=4, is_write=False, name='r_sync'),
            ],
            'read_rules': _mem_ok,
            'write_rules': async_rules,
        }

    if case_name == 'deny_read_only':
        # Read side DENYs (retry_delay=5). Only one read request; the
        # master sees DENIED then a RETRY once the read side retries.
        deny_rules = [dict(addr_min=0, addr_max=0xFFFF, behavior='denied',
                           resp_delay=0, retry_delay=5)]
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='r_deny'),
            ],
            'read_rules': deny_rules,
            'write_rules': _mem_ok,
        }

    if case_name == 'deny_write_only':
        # Write side DENYs (retry_delay=5). Symmetric to deny_read_only.
        deny_rules = [dict(addr_min=0, addr_max=0xFFFF, behavior='denied',
                           resp_delay=0, retry_delay=5)]
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=True, name='w_deny'),
            ],
            'read_rules': _mem_ok,
            'write_rules': deny_rules,
        }

    if case_name == 'atomic_counts_as_write':
        # An SC (store-conditional, opcode index 3) has is_write == true.
        # Validates that atomics are routed through the write output.
        # Using the StubMaster schedule, is_write=True covers any non-READ.
        return {
            'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=True, name='atomic'),
            ],
            'read_rules': _mem_ok,
            'write_rules': _mem_ok,
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='read_only',
            description='Which rw_splitter_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        split = RwSplitter(self, 'rwsplit', config=RwSplitterConfig())
        clock.o_CLOCK(split.i_CLOCK())

        # Upstream io_v2 master
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(split.i_INPUT())

        # Two downstream targets
        mem_read  = StubTarget(self, 'mem_read',  rules=spec['read_rules'],
                                logname='mem_read')
        mem_write = StubTarget(self, 'mem_write', rules=spec['write_rules'],
                                logname='mem_write')
        clock.o_CLOCK(mem_read.i_CLOCK())
        clock.o_CLOCK(mem_write.i_CLOCK())
        split.o_READ_OUTPUT(mem_read.i_INPUT())
        split.o_WRITE_OUTPUT(mem_write.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'rw_splitter_v2 testbench'
    model = Chip
    name = 'test'
