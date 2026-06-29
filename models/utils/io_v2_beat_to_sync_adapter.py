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
Python generator for the IoV2BeatToSyncAdapter component.

Auto-inserted by the gvrun2 systree binding pass when a master port declares
signature ``IoV2Beat`` and the bound slave port declares signature
``IoV2Sync`` (see ``IoV2Beat.bridge_to`` in ``gvsoc/signature.py``). It is a
specialisation of ``IoV2BeatAdapter`` for the sync case: the slave always
replies inline with ``IO_REQ_DONE`` and serves any size in one call, so the
adapter forwards the whole burst as a single request and spreads the inline
response into a per-beat ``resp()`` stream.
"""

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2Beat, IoV2Sync


class IoV2BeatToSyncAdapter(Component):

    def __init__(self, parent: Component, name: str, beat_width: int):
        super().__init__(parent, name)
        self.set_component('utils.io_v2_beat_to_sync_adapter')
        self.add_property('beat_width', beat_width)
        self._beat_width = beat_width

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2Beat(self._beat_width))

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2Sync())
