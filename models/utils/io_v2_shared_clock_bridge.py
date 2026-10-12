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

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import Signature, IoV2Beat, IoV2SingleReq, IoV2Sync


# io_v2 protocols the bridge can carry, by signature class
PROTOCOLS = {IoV2Beat: 'beat', IoV2SingleReq: 'single_req', IoV2Sync: 'sync'}


class IoV2SharedClockBridgeConfig(Config):
    """Configuration for the io_v2 shared-clock bridge."""

    protocol: str = cfg_field(default='beat', dump=True, desc=(
        "io_v2 protocol of the crossing: 'beat', 'single_req' or 'sync'. On the beat "
        "protocol, a write burst answered on the other side goes back as a data-less "
        "acknowledgement. On the others, a write goes back to its master as itself, as a "
        "read."
    ))


class IoV2SharedClockBridge(Component):

    def __init__(self, parent: Component, name: str, *, signature: Signature):
        protocol = next((p for c, p in PROTOCOLS.items() if isinstance(signature, c)), None)
        if protocol is None:
            raise RuntimeError(
                f'{parent.get_path()}/{name}: IoV2SharedClockBridge carries the '
                f'{", ".join(c.__name__ for c in PROTOCOLS)} protocols, got {signature!r}')
        super().__init__(parent, name, config=IoV2SharedClockBridgeConfig(protocol=protocol))
        self._port_signature = signature
        self.add_sources(['utils/io_v2_shared_clock_bridge.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=self._signature())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=self._signature())

    def _signature(self) -> Signature:
        # The bridge relays the crossing's own protocol 1:1.
        return self._port_signature
