# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""
Python generator for the IoV2SharedClockBridge component: the io_v2 bridge
between two clock domains driven by the same clock (one clock behind two gates
or dividers, no synchronizer on the chip). Nothing is resynchronized, but the
crossing is registered, as the AXI slices of a cluster interface: each
direction has a list of pending requests (or responses), whose head is sent
every cycle, at least one cycle after it was taken. Selected with the ``'shared_clock'``
bridge kind (``gvsoc.clock_bridges``), never as a default: a real crossing keeps
its synchronizer latency whatever the phase of the two clocks.
"""

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import Signature


class IoV2SharedClockBridge(Component):

    def __init__(self, parent: Component, name: str, *, signature: Signature = None):
        super().__init__(parent, name)
        self._port_signature = signature
        self.add_sources(['utils/io_v2_shared_clock_bridge.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=self._signature())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=self._signature())

    def _signature(self) -> Signature:
        # The bridge relays the crossing's own protocol 1:1: an explicit
        # instantiation must give the concrete protocol of that crossing (the
        # auto-splice path re-uses the spliced binding's signatures).
        if not isinstance(self._port_signature, Signature):
            raise RuntimeError(
                f'{self.get_path()}: an explicitly instantiated '
                f'IoV2SharedClockBridge requires a concrete signature= '
                f'matching its crossing')
        return self._port_signature
