# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""io_v2 clock-domain-bridge testbench.

Two clock domains, each holding one io_v2 memory and one io_v2 driver. Each
driver's master output is wired to the *other* domain's memory so every
request crosses the clock boundary. The systree binding pass
(``Component.__expand_signature_bridges``) detects the crossing and splices
in a ``utils.io_v2_clock_bridge`` component (default ``sync_only`` kind)
per direction. The test verifies that both directions complete with correct
data despite arbitrary frequency ratios.

The ``case`` :class:`TargetParameter` selects a scenario; each scenario
overrides ``freq_a`` / ``freq_b`` to cover the common ratio regimes.
"""

from __future__ import annotations

import gvsoc.systree
import gvsoc.runner
import vp.clock_domain
from memory.memory_v3 import Memory, MemoryV3Config
from gvrun.parameter import TargetParameter

from cdc_tester import CDCTester
# Register the cdc_*_rtl calibration kinds locally (they are not in the
# shipped simulator model library β€” see rtl_calibration/__init__.py).
import rtl_calibration  # noqa: F401


# Memories: one in each domain. Sized to comfortably fit any of the cases.
MEM_A_SIZE = 0x10000
MEM_B_SIZE = 0x10000


def build_case(case_name: str) -> dict:
    """Return the per-case parameters for the two clock domains."""
    common = dict(nb_accesses=16, access_size=4, pattern_seed_a=0xa5,
                  pattern_seed_b=0x5a)

    if case_name == 'equal':
        # Same frequency on both sides β€” the bridge inserts but its sync()
        # calls are no-ops because both engines tick together.
        return {**common, 'freq_a': 100_000_000, 'freq_b': 100_000_000}

    if case_name == 'a_fast':
        # clk_a is 4Γ— faster than clk_b. tester_a issues at the fast clock;
        # the bridge must sync clk_b before each forwarded req so mem_b is
        # not seen at a stale time.
        return {**common, 'freq_a': 400_000_000, 'freq_b': 100_000_000}

    if case_name == 'a_slow':
        # Mirror image: clk_a 4Γ— slower than clk_b. Stresses the reverse
        # sync (on resp() / retry() the master_engine is the slow side).
        return {**common, 'freq_a': 100_000_000, 'freq_b': 400_000_000}

    if case_name == 'odd_ratio':
        # Non-integer ratio (3:1) and large gap, so sync() bring-up cycles
        # are typically partial. Also bumps nb_accesses for more traffic.
        return {**common, 'freq_a': 300_000_000, 'freq_b': 100_000_000,
                'nb_accesses': 32}

    if case_name == 'wide_access':
        # 8-byte accesses to confirm the bridge is opaque to access size.
        return {**common, 'freq_a': 200_000_000, 'freq_b': 100_000_000,
                'access_size': 8}

    raise ValueError(f'Unknown case: {case_name}')


class Chip(gvsoc.systree.Component):
    def __init__(self, parent, name=None):
        super().__init__(parent, name)
        case = TargetParameter(
            self, name='case', value='equal',
            description='Which io_v2_clkbridge scenario to run', cast=str,
        ).get_value()
        bridge = TargetParameter(
            self, name='bridge', value='sync_only',
            description='CDC bridge kind: sync_only (C++ behavioural, default), '
                        'cdc_2phase_rtl (Verilator-wrapped common_cells block)',
            cast=str,
        ).get_value()
        # Optional per-test pipelining knobs. Default 1 keeps the existing
        # 35-case matrix unchanged (testers are serial). Higher values let
        # the FIFO kinds (cdc_fifo_gray_*, cdc_fifo_2phase_*) actually
        # pipeline traffic; the matching bridge depth must be raised too.
        pipeline_burst = TargetParameter(
            self, name='pipeline_burst', value=1,
            description='Max in-flight transactions per tester (1=serial)',
            cast=int,
        ).get_value()
        bridge_depth = TargetParameter(
            self, name='bridge_depth', value=0,
            description='Override the bridge kind\'s default FIFO depth (0=keep default)',
            cast=int,
        ).get_value()

        spec = build_case(case)

        # Two clock domains with independent frequencies.
        clk_a = vp.clock_domain.Clock_domain(self, 'clk_a', frequency=spec['freq_a'])
        clk_b = vp.clock_domain.Clock_domain(self, 'clk_b', frequency=spec['freq_b'])

        # Pick the bridge kind for both crossings (A->B and B->A). Pass the
        # `depth` opt only when the user actually overrides it β€” the bridge
        # factory rejects unknown opts for the depth-less kinds (sync_only,
        # *_rtl) so we keep the default path opt-free.
        bridge_opts = {'depth': bridge_depth} if bridge_depth > 0 else {}
        self.set_clock_bridge_policy(src_clock=clk_a, dst_clock=clk_b,
                                     kind=bridge, opts=bridge_opts)
        self.set_clock_bridge_policy(src_clock=clk_b, dst_clock=clk_a,
                                     kind=bridge, opts=bridge_opts)

        # Memories, one in each domain. memory_v3 is IoV2Sync β€” a strict
        # synchronous slave that the auto-bridge will splice transparently.
        mem_a = Memory(self, 'mem_a', config=MemoryV3Config(
            size=MEM_A_SIZE, latency=1))
        mem_b = Memory(self, 'mem_b', config=MemoryV3Config(
            size=MEM_B_SIZE, latency=1))
        clk_a.o_CLOCK(mem_a.i_CLOCK())
        clk_b.o_CLOCK(mem_b.i_CLOCK())

        # Drivers: tester_a in domain A targets mem_b (A->B crossing).
        #         tester_b in domain B targets mem_a (B->A crossing).
        # memory_v3 carries base=0 internally; passing base=0 puts requests
        # at the start of each memory window.
        tester_a = CDCTester(self, 'tester_a',
            base=0,
            access_size=spec['access_size'],
            nb_accesses=spec['nb_accesses'],
            pattern_seed=spec['pattern_seed_a'],
            pipeline_burst=pipeline_burst,
            logname='tester_a')
        tester_b = CDCTester(self, 'tester_b',
            base=0,
            access_size=spec['access_size'],
            nb_accesses=spec['nb_accesses'],
            pattern_seed=spec['pattern_seed_b'],
            pipeline_burst=pipeline_burst,
            logname='tester_b')
        clk_a.o_CLOCK(tester_a.i_CLOCK())
        clk_b.o_CLOCK(tester_b.i_CLOCK())

        # Cross-domain bindings β€” the auto-bridge pass splices in a
        # utils.io_v2_clock_bridge on each of these.
        tester_a.o_OUTPUT(mem_b.i_INPUT())
        tester_b.o_OUTPUT(mem_a.i_INPUT())

        # Cross-tester barrier wires (the existing wire-side freq-cross
        # stubs in vp::WireMaster/WireSlave handle the domain crossing for
        # these single-bit signals).
        tester_a.o_DONE(tester_b.i_DONE())
        tester_b.o_DONE(tester_a.i_DONE())


class Target(gvsoc.runner.Target):
    gapy_description = 'io_v2 clock-domain-bridge testbench'
    model = Chip
    name = 'test'
