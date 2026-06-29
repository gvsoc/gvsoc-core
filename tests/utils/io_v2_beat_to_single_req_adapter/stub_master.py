# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from gvsoc.signature import IoV2Beat


class StubMaster(gvsoc.systree.Component):
    """io_v2 beat-master testbench initiator.

    Declares signature ``IoV2Beat`` on its output so binding it to an
    ``IoV2SingleReq`` target makes the framework auto-insert
    IoV2BeatToSingleReqAdapter.
    """
    def __init__(self, parent, name, schedule=None, beat_width=8, logname=None,
                 quit_after_cycles=200):
        super().__init__(parent, name)
        self.add_sources(['stub_master.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('schedule', schedule or [])
        self.add_property('beat_width', beat_width)
        self.add_property('quit_after_cycles', quit_after_cycles)
        self._beat_width = beat_width

    def o_OUTPUT(self, itf):
        self.itf_bind('output', itf, signature=IoV2Beat(self._beat_width))
