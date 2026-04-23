# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""log_ico_v2 testbench.

Instantiates the log_ico crossbar with a case-dependent (nb_masters,
nb_slaves) topology and connects one stub io_v2 master per input and one
stub io_v2 target per output. Each test case is selected via the
``case`` TargetParameter, which picks a build_case dict with:

  - log_ico_config:  LogIcoConfig for the DUT
  - masters:         list of {'name', 'schedule'} dicts, one per master
  - targets:         list of {'name', 'rules'} dicts, one per bank
"""

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from interco.log_ico_v2 import LogIco, LogIcoConfig
from gvrun.parameter import TargetParameter

from stub_master import StubMaster
from stub_target import StubTarget


_mem_ok = [dict(addr_min=0, addr_max=0xFFFF_FFFF_FFFF_FFFF, behavior='done',
                resp_delay=0, retry_delay=0)]


def _ok_targets(n):
    return [{'name': f'mem{i}', 'rules': _mem_ok} for i in range(n)]


def build_case(case_name: str) -> dict:
    if case_name == 'single_master_interleave':
        # One master, four banks, 16-byte interleaving (iw=4). The master
        # issues four requests at 0x00, 0x10, 0x20, 0x30. All four should
        # hit bank 0..3 at bank-local address 0.
        cfg = LogIcoConfig(nb_masters=1, nb_slaves=4, interleaving_width=4,
                           remove_offset=0)
        return {
            'log_ico_config': cfg,
            'masters': [{'name': 'm0', 'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='a0'),
                dict(cycle=20, addr=0x10, size=4, is_write=False, name='a1'),
                dict(cycle=30, addr=0x20, size=4, is_write=False, name='a2'),
                dict(cycle=40, addr=0x30, size=4, is_write=False, name='a3'),
            ]}],
            'targets': _ok_targets(4),
        }

    if case_name == 'interleave_roll_over':
        # Same topology, but requests spread past the first four granules.
        # Bytes 0x00..0x0F go to bank 0 local 0x00, bytes 0x40..0x4F to
        # bank 0 local 0x10, etc. Verifies that the high-bit compaction
        # works: each bank's local address advances by one granule (16)
        # after a full sweep across banks.
        cfg = LogIcoConfig(nb_masters=1, nb_slaves=4, interleaving_width=4,
                           remove_offset=0)
        return {
            'log_ico_config': cfg,
            'masters': [{'name': 'm0', 'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='g0b0'),
                dict(cycle=20, addr=0x40, size=4, is_write=False, name='g1b0'),
                dict(cycle=30, addr=0x50, size=4, is_write=False, name='g1b1'),
                dict(cycle=40, addr=0x81, size=4, is_write=False, name='g2b0off1'),
            ]}],
            'targets': _ok_targets(4),
        }

    if case_name == 'remove_offset':
        # remove_offset=0x1000. A master issuing at 0x1000+0x00 should
        # reach bank 0 at local 0, and at 0x1000+0x10 should reach bank 1.
        cfg = LogIcoConfig(nb_masters=1, nb_slaves=4, interleaving_width=4,
                           remove_offset=0x1000)
        return {
            'log_ico_config': cfg,
            'masters': [{'name': 'm0', 'schedule': [
                dict(cycle=10, addr=0x1000, size=4, is_write=False, name='base'),
                dict(cycle=20, addr=0x1010, size=4, is_write=False, name='off16'),
            ]}],
            'targets': _ok_targets(4),
        }

    if case_name == 'multi_master_no_conflict':
        # 2 masters, 2 banks, each master hits a different bank — no
        # contention. Both complete without DENIEDs.
        cfg = LogIcoConfig(nb_masters=2, nb_slaves=2, interleaving_width=4,
                           remove_offset=0)
        return {
            'log_ico_config': cfg,
            'masters': [
                {'name': 'm0', 'schedule': [
                    dict(cycle=10, addr=0x00, size=4, is_write=False, name='to_b0'),
                ]},
                {'name': 'm1', 'schedule': [
                    dict(cycle=10, addr=0x10, size=4, is_write=False, name='to_b1'),
                ]},
            ],
            'targets': _ok_targets(2),
        }

    if case_name == 'multi_master_indep_deny':
        # 2 masters, 2 banks. Bank 0 denies (retry_delay=5), bank 1 is
        # normal. Master 0 hits bank 0 (gets DENIED). Master 1 hits bank 1
        # (must succeed independently, NOT be held up by bank 0's stall).
        cfg = LogIcoConfig(nb_masters=2, nb_slaves=2, interleaving_width=4,
                           remove_offset=0)
        rules_deny = [dict(addr_min=0, addr_max=0xFFFF, behavior='denied',
                           resp_delay=0, retry_delay=5)]
        return {
            'log_ico_config': cfg,
            'masters': [
                {'name': 'm0', 'schedule': [
                    dict(cycle=10, addr=0x00, size=4, is_write=False, name='to_b0'),
                ]},
                {'name': 'm1', 'schedule': [
                    dict(cycle=11, addr=0x10, size=4, is_write=False, name='to_b1'),
                ]},
            ],
            'targets': [
                {'name': 'mem0', 'rules': rules_deny},
                {'name': 'mem1', 'rules': _mem_ok},
            ],
        }

    if case_name == 'multi_master_async_resp':
        # 2 masters sending requests to two banks that reply asynchronously
        # with different delays. Validates that responses return on the
        # correct master's input port (the InFlight must carry input_id
        # correctly).
        cfg = LogIcoConfig(nb_masters=2, nb_slaves=2, interleaving_width=4,
                           remove_offset=0)
        rules_slow = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                           resp_delay=20, retry_delay=0)]
        rules_fast = [dict(addr_min=0, addr_max=0xFFFF, behavior='granted',
                           resp_delay=5,  retry_delay=0)]
        return {
            'log_ico_config': cfg,
            'masters': [
                {'name': 'm0', 'schedule': [
                    dict(cycle=10, addr=0x00, size=4, is_write=False, name='m0_slow'),
                ]},
                {'name': 'm1', 'schedule': [
                    dict(cycle=10, addr=0x10, size=4, is_write=False, name='m1_fast'),
                ]},
            ],
            'targets': [
                {'name': 'mem0', 'rules': rules_slow},
                {'name': 'mem1', 'rules': rules_fast},
            ],
        }

    if case_name == 'single_bank':
        # nb_slaves=1 → slave_bits=0, bank decode collapses to bank 0 for
        # everything, bank_offset == offset (minus remove_offset). Good edge
        # case to catch any off-by-one in the shift.
        cfg = LogIcoConfig(nb_masters=1, nb_slaves=1, interleaving_width=4,
                           remove_offset=0)
        return {
            'log_ico_config': cfg,
            'masters': [{'name': 'm0', 'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=False, name='a0'),
                dict(cycle=20, addr=0x40, size=4, is_write=False, name='a1'),
            ]}],
            'targets': _ok_targets(1),
        }

    if case_name == 'write_through':
        # Writes are forwarded verbatim — same addressing as reads, opcode
        # preserved.
        cfg = LogIcoConfig(nb_masters=1, nb_slaves=2, interleaving_width=4,
                           remove_offset=0)
        return {
            'log_ico_config': cfg,
            'masters': [{'name': 'm0', 'schedule': [
                dict(cycle=10, addr=0x00, size=4, is_write=True, name='w0'),
                dict(cycle=20, addr=0x10, size=4, is_write=True, name='w1'),
            ]}],
            'targets': _ok_targets(2),
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='single_master_interleave',
            description='Which log_ico_v2 test case to run', cast=str,
        ).get_value()

        spec = build_case(case)
        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100_000_000)

        # DUT
        xbar = LogIco(self, 'xbar', config=spec['log_ico_config'])
        clock.o_CLOCK(xbar.i_CLOCK())

        # One master per input
        for i, m_spec in enumerate(spec['masters']):
            m = StubMaster(self, m_spec['name'], schedule=m_spec['schedule'],
                           logname=m_spec['name'])
            clock.o_CLOCK(m.i_CLOCK())
            m.o_OUTPUT(xbar.i_INPUT(i))

        # One bank per output
        for i, t_spec in enumerate(spec['targets']):
            t = StubTarget(self, t_spec['name'], rules=t_spec['rules'],
                           logname=t_spec['name'])
            clock.o_CLOCK(t.i_CLOCK())
            xbar.o_OUTPUT(i, t.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'log_ico_v2 testbench'
    model = Chip
    name = 'test'
