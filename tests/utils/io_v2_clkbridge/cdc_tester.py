# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from gvsoc.signature import IoV2BigPacket


class CDCTester(gvsoc.systree.Component):
    """Bidirectional io_v2 master driving the clock-bridge testbench.

    Each instance issues ``nb_accesses`` writes followed by ``nb_accesses``
    reads against ``base`` on its single ``output`` master port. The Chip
    wires the two testers' output ports to memories in the *other* clock
    domain so the IoV2ClockBridge is exercised on both crossings.

    Quit coordination is handled by two single-bit wires: each tester
    pulses ``done_out`` when its local PASS line is printed; receiving the
    partner's pulse on ``done_in`` plus being locally done causes
    ``engine->quit()`` to fire.
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str, *,
                 base: int,
                 access_size: int = 4,
                 nb_accesses: int = 16,
                 pattern_seed: int = 0xa5,
                 quit_after_cycles: int = 100_000,
                 pipeline_burst: int = 1,
                 logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['cdc_tester.cpp'])
        self.add_property('base',              base)
        self.add_property('access_size',       access_size)
        self.add_property('nb_accesses',       nb_accesses)
        self.add_property('pattern_seed',      pattern_seed)
        self.add_property('quit_after_cycles', quit_after_cycles)
        self.add_property('pipeline_burst',    pipeline_burst)
        self.add_property('logname',           logname or name)

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        # Class-based signature so the systree auto-bridge pass sees the
        # binding and inserts an utils.io_v2_clock_bridge across the two
        # clock domains. IoV2BigPacket is the most permissive v2 master
        # signature β€” it accepts any of the three slave response forms.
        self.itf_bind('output', itf, signature=IoV2BigPacket())

    def o_DONE(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('done_out', itf, signature='wire<bool>')

    def i_DONE(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'done_in', signature='wire<bool>')
