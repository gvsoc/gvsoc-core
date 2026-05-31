# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from gvsoc.signature import IoV2Sync


class StubTarget(gvsoc.systree.Component):
    """IoV2Sync testbench target.

    Behaves per a list of rules. Each rule is a dict with keys:
    addr_min, addr_max, behavior, resp_delay, retry_delay.
    behavior must be one of {done, done_invalid} — the async behaviours
    (granted, denied) are not compatible with the IoV2Sync contract.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 rules: list | None = None, logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['stub_target.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('rules', rules or [])

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2Sync())
