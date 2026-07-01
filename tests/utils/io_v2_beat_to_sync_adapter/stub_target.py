# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from gvsoc.signature import IoV2Sync


class StubTarget(gvsoc.systree.Component):
    """io_v2 sync (inline-DONE) testbench target."""
    def __init__(self, parent, name, latency=0, duration=0, error=False,
                 base=0, size=0, logname=None):
        super().__init__(parent, name)
        self.add_sources(['stub_target.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('latency', latency)
        self.add_property('duration', duration)
        self.add_property('error', error)
        self.add_property('base', base)
        self.add_property('size', size)

    def i_INPUT(self):
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2Sync())
