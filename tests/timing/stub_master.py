# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree


class StubMaster(gvsoc.systree.Component):
    """io_v2 testbench initiator.

    Issues a pre-programmed schedule of requests. Each schedule entry is a dict with
    keys: cycle, addr, size, is_write, name.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['stub_master.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('schedule', schedule or [])

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='io_v2')
