# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree


class StubWires(gvsoc.systree.Component):
    """Pulses cache control wires on a schedule.

    Each schedule entry is a dict with keys: cycle, signal, value.
    signal is one of: 'enable', 'flush', 'flush_line', 'flush_line_addr'.
    Also receives flush_ack pulses from the cache and logs them.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, logname: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['stub_wires.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('schedule', schedule or [])

    def o_ENABLE(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('enable', itf, signature='wire<bool>')

    def o_FLUSH(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('flush', itf, signature='wire<bool>')

    def o_FLUSH_LINE(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('flush_line', itf, signature='wire<bool>')

    def o_FLUSH_LINE_ADDR(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('flush_line_addr', itf, signature='wire<uint32_t>')

    def i_FLUSH_ACK(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'flush_ack', signature='wire<bool>')
