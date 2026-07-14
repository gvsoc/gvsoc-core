# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""memory.ramulator testbench.

Hooks a stub io_v2 master (reused from the memory_v3 test) to the
Ramulator-backed memory. Each case picks a schedule of io_v2 requests; the
master logs SEND / GRANTED / RESP lines with the issue and completion cycles,
which the checkers in testset.cfg use to validate both functional data and
that Ramulator's DRAM latency is actually applied (RESP strictly later than the
GRANTED cycle).

The clock is set to the DDR4-2400 command clock (1.2 GHz) so one model cycle is
one DRAM cycle; the master shares this clock so logged cycle counts read
directly as DRAM cycles.
"""

from __future__ import annotations

import os
import yaml

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from memory.ramulator import Ramulator, RamulatorConfig
from gvrun.parameter import TargetParameter

from beat_master import BeatMaster


CONFIG_YAML = os.path.join(os.path.dirname(__file__), 'config.yaml')


def build_case(case_name: str) -> dict:
    if case_name == 'read_basic':
        # 4-byte read at 0x0 from a freshly-initialised (0x57) memory.
        return {
            'size': 0x10000,
            'schedule': [
                dict(cycle=10, addr=0x0, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'write_then_read':
        # Write a known pattern, read it back.
        return {
            'size': 0x10000,
            'schedule': [
                dict(cycle=10, addr=0x80, size=4, is_write=True,  name='w',
                     data_hex='deadbeef'),
                dict(cycle=200, addr=0x80, size=4, is_write=False, name='r'),
            ],
        }

    if case_name == 'multi_tx':
        # 128-byte read: with a 64-byte DRAM tx granularity this spans 2
        # Ramulator transactions but must surface as a single io_v2 resp.
        return {
            'size': 0x10000,
            'schedule': [
                dict(cycle=10, addr=0x0, size=128, is_write=False, name='r'),
            ],
        }

    if case_name == 'bw_read':
        # One large sequential read burst (4 KiB = 64 beats). Streams back
        # beats whose rate is throttled by Ramulator's DDR4 timing — used to
        # measure sustained read bandwidth.
        return {
            'size': 0x10000,
            'quit_after_cycles': 2000,
            'schedule': [
                dict(cycle=10, addr=0x0, size=4096, is_write=False, name='r'),
            ],
        }

    if case_name == 'transfer_2d':
        # 2D transfer: each 1D line is a 1 KiB burst, and the line pitch lands
        # each line on a different DRAM bank. In this DDR4 RoBaRaCoCh mapping
        # (column = byte bits 6-12, bankgroup = bits 13-14, bank = bits 15-16),
        # a 0x2000 line pitch walks the full bank/bankgroup field, so the 16
        # lines hit 16 distinct banks; a 1 KiB line stays within one bank/row.
        line_size = 1024
        line_pitch = 0x2000
        n_lines = 16
        return {
            'size': n_lines * line_pitch,
            'quit_after_cycles': 5000,
            'schedule': [
                dict(cycle=10 + i * 20, addr=i * line_pitch, size=line_size,
                     is_write=False, name=f'line{i}')
                for i in range(n_lines)
            ],
        }

    if case_name == 'speed':
        # Host-simulation-speed benchmark: re-issue a 4 KiB read burst 'repeat'
        # times (one in flight, chained), quietly (no per-beat logging), to get
        # a long dense run whose wall time measures the simulator's speed.
        return {
            'size': 0x10000,
            'chain': True,
            'quiet': True,
            'quit_after_cycles': 100,
            'schedule': [
                dict(cycle=10, addr=0x0, size=4096, is_write=False, name='r'),
            ],
        }

    if case_name == 'bw_write':
        # One large sequential write burst (4 KiB). Write data is consumed one
        # beat per cycle, paced by Ramulator — used to measure write bandwidth.
        return {
            'size': 0x10000,
            'quit_after_cycles': 2000,
            'schedule': [
                dict(cycle=10, addr=0x0, size=4096, is_write=True, name='w'),
            ],
        }

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='read_basic',
            description='Which memory.ramulator test case to run', cast=str,
        ).get_value()

        spec = build_case(case)

        # Which DRAM timing model to drive with the same beat master + schedule,
        # so Ramulator and DRAMSys can be compared on the same DDR4 workload.
        which = TargetParameter(
            self, name='mem', value='ramulator',
            description='DRAM timing model: ramulator | dramsys', cast=str,
        ).get_value()

        # Number of times the speed benchmark re-issues its burst (tune for a
        # given wall time). Ignored by non-speed cases.
        repeat = TargetParameter(
            self, name='repeat', value=1,
            description='speed-case burst repeat count', cast=int,
        ).get_value()

        # DDR4 transaction granularity (== one response beat) for both models
        # (Ramulator tx_bytes, DRAMSys ddr4 access_size).
        beat_width = 64

        if which == 'dramsys':
            import memory.dramsys
            mem = memory.dramsys.Dramsys(self, 'mem', version=2,
                                         dram_type='ddr4-example.json')
            clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=1_200_000_000)
        else:
            mem = Ramulator(self, 'mem', config=RamulatorConfig(
                size=spec['size'], config_yaml=CONFIG_YAML, beat_width=beat_width))
            tck_ps = yaml.safe_load(open(CONFIG_YAML))["memory_system"]["tCK_ps"]
            clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=round(1e12 / tck_ps))

        # DDR4 command clock: 1.2 GHz (matches Ramulator's DDR4-2400 tCK; for
        # DRAMSys it is only the harness clock — DRAMSys keeps its own SC time).
        clock.o_CLOCK(mem.i_CLOCK())

        master = BeatMaster(self, 'master', schedule=spec['schedule'],
                            beat_width=beat_width, logname='master',
                            quit_after_cycles=spec.get('quit_after_cycles', 200),
                            quiet=spec.get('quiet', False),
                            chain=spec.get('chain', False), repeat=repeat)
        clock.o_CLOCK(master.i_CLOCK())
        master.o_OUTPUT(mem.i_INPUT())


class Target(gvsoc.runner.Target):
    gapy_description = 'memory.ramulator testbench'
    model = Chip
    name = 'test'
