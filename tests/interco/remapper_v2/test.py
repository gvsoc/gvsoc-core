# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""remapper_v2 testbench.

Hooks a stub io_v2 master to remapper.i_INPUT and a stub io_v2 target to
remapper.o_OUTPUT. Each test case is selected via the ``case``
TargetParameter, which picks a build_case dict with:

  - remapper_config: RemapperConfig for the DUT
  - schedule:        list of CPU-side io_v2 requests to send
  - rules:           list of stub_target behaviours
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.remapper_v2 import Remapper, RemapperConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


_mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                resp_delay=0, retry_delay=0)]


def build_case(case_name: str) -> dict:
    if case_name == 'remap_in_window':
        # Window [0x1000, 0x1100), target_base=0x2000. A request at 0x1040
        # must reach mem at 0x2040.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1040, size=4, is_write=False, name='in'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'at_base':
        # Edge: request at exactly ``base`` is inside the window and is
        # rewritten to ``target_base``.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1000, size=4, is_write=False, name='at_base'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'at_top':
        # Edge: last byte inside the window (addr == base + size - 1). Must
        # be rewritten to target_base + size - 1.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x10FF, size=1, is_write=False, name='top'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'just_past_end':
        # Edge: request at exactly ``base + size`` is OUTSIDE the window
        # (half-open interval). Must be forwarded unchanged.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1100, size=4, is_write=False, name='past'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'just_below_base':
        # Edge: request at ``base - 1`` is OUTSIDE the window. Forwarded
        # unchanged.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x0FFF, size=1, is_write=False, name='below'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'size_zero':
        # size=0 disables the remap entirely. Every request is a
        # passthrough, regardless of its address.
        cfg = RemapperConfig(base=0x1000, size=0x0, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1000, size=4, is_write=False, name='in_nominal'),
                dict(cycle=20, addr=0x2000, size=4, is_write=False, name='tgt'),
                dict(cycle=30, addr=0x0FFF, size=4, is_write=False, name='below'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'no_op_map':
        # target_base == base. In-window requests are rewritten but to the
        # same value. Validates that the DENY/RETRY plumbing still runs
        # correctly through a degenerate remap.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x1000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1040, size=4, is_write=False, name='noop'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'write_remap':
        # Write into the window. Opcode and data must reach the downstream
        # at the rewritten address.
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1010, size=4, is_write=True, name='w'),
            ],
            'rules': _mem_ok,
        }

    if case_name == 'async_resp':
        # Downstream returns GRANTED + deferred resp. The remapper must
        # propagate GRANTED upstream and later forward the resp.
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='granted',
                      resp_delay=12, retry_delay=0)]
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1010, size=4, is_write=False, name='r'),
            ],
            'rules': rules,
        }

    if case_name == 'deny_retry':
        # Downstream denies with retry_delay=5. Remapper propagates DENIED
        # then RETRY. Also implicitly validates that the original address
        # is restored before the DENIED status reaches the master (so the
        # master's resend sees the same address it first sent).
        rules = [dict(addr_min=0, addr_max=0xFFFF_FFFF, behavior='denied',
                      resp_delay=0, retry_delay=5)]
        cfg = RemapperConfig(base=0x1000, size=0x100, target_base=0x2000)
        return {
            'remapper_config': cfg,
            'schedule': [
                dict(cycle=10, addr=0x1040, size=4, is_write=False, name='r'),
            ],
            'rules': rules,
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='remap_in_window',
            description='Which remapper_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        remapper = Remapper(self, 'remapper', config=spec['remapper_config'])
        clock.o_CLOCK(remapper.i_CLOCK())

        # Upstream io_v2 master
        master = StubMaster(self, 'master', schedule=spec['schedule'], logname='master')
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(remapper.i_INPUT())

        # Downstream io_v2 target
        target = StubTarget(self, 'mem', rules=spec['rules'], logname='mem')
        clock.o_CLOCK(target.i_CLOCK())
        remapper.o_OUTPUT(target.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'remapper_v2 testbench'
    model = Chip
    name = 'test'
