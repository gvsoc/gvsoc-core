# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree


class StubWireSink(gvsoc.systree.Component):
    """Testbench sink for loader_v2's start/entry wires."""
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['stub_wire_sink.cpp'])
        self.add_property('logname', logname or name)

    def i_START(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'start_in', signature='wire<bool>')

    def i_ENTRY(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'entry_in',
                                       signature='wire<uint64_t>')
