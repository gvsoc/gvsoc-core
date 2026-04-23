# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree


class StubTarget(gvsoc.systree.Component):
    """io_v2 testbench target.

    Behaves per a list of rules. Each rule is a dict with keys:
    addr_min, addr_max, behavior, resp_delay, retry_delay.
    behavior in {done, done_invalid, granted, denied}.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 rules: list | None = None, logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['stub_target.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('rules', rules or [])

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io_v2')
