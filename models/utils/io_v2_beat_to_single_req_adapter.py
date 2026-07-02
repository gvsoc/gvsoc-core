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
Python generator for the IoV2BeatToSingleReqAdapter component.

Auto-inserted by the gvrun2 systree binding pass when a master port declares
signature ``IoV2Beat`` and the bound slave port declares signature
``IoV2SingleReq`` (see ``IoV2Beat.bridge_to`` in ``gvsoc/signature.py``). A
single-req slave answers each request with a single-beat response — inline
``IO_REQ_DONE``, async ``IO_REQ_GRANTED`` + one ``resp()``, or ``IO_REQ_DENIED``
+ ``retry()`` — but never a multi-beat stream. The adapter splits a read burst
into beat-sized sub-reads (issued one per cycle), forwards writes as-is, and
normalises the single-beat responses into the upstream per-beat ``resp()``
stream. It is a per-combination specialisation of ``IoV2BeatAdapter`` (same
machinery, with the single-beat-response contract checked in asserts builds).
"""

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2Beat, IoV2SingleReq


class IoV2BeatToSingleReqAdapterConfig(Config):
    """Configuration for the io_v2 beat -> single-req adapter."""

    beat_width: int = cfg_field(default=0, dump=True, desc=(
        "Beat width in bytes for the upstream per-beat response stream."
    ))
    max_read_bursts: int = cfg_field(default=4, dump=True, desc=(
        "Maximum number of read bursts in flight (accepted but not yet fully "
        "delivered upstream). The read request channel is back-pressured beyond "
        "this — the HW r_id-FIFO analogue."
    ))


class IoV2BeatToSingleReqAdapter(Component):

    def __init__(self, parent: Component, name: str, beat_width: int,
                 max_read_bursts: int = 4):
        super().__init__(parent, name, config=IoV2BeatToSingleReqAdapterConfig(
            beat_width=beat_width, max_read_bursts=max_read_bursts))
        self.set_component('utils.io_v2_beat_to_single_req_adapter')
        self._beat_width = beat_width

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2Beat(self._beat_width))

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2SingleReq())
