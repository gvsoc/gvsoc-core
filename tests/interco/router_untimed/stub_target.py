# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree


class StubTarget(gvsoc.systree.Component):
    """io_v2 beat-mode testbench target."""
    def __init__(self, parent, name, rules=None, logname=None):
        super().__init__(parent, name)
        self.add_sources(['stub_target.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('rules', rules or [])

    def i_INPUT(self):
        return gvsoc.systree.SlaveItf(self, 'input', signature='io_v2')
