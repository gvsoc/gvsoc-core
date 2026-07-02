#
# Copyright (C) 2026 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Python generator for the IoV2BeatCollapseAdapter component.

The inverse of :class:`io_v2_beat_adapter.IoV2BeatAdapter`: it makes a
beat-streaming slave (e.g. a KIND_BEAT router) look like a plain big-packet
slave to a big-packet master, collapsing a multi-beat read response into a
single response. Place it between a big-packet master that must not see beats
(a functional/untimed router, a CPU LSU) and a beat slave.

  input  (slave)  : IoV2BigPacket — faces the big-packet master.
  output (master) : IoV2Beat      — faces the beat slave.
"""

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2BigPacket, IoV2Beat


class IoV2BeatCollapseAdapter(Component):

    def __init__(self, parent: Component, name: str, beat_width: int):
        super().__init__(parent, name)
        self.set_component('utils.io_v2_beat_collapse_adapter')
        self._beat_width = beat_width

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2BigPacket())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2Beat(self._beat_width))
