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
Python generator for the IoV2SingleReqToBeatAdapter component.

Auto-inserted by the gvrun2 systree binding pass when a master port declares
signature ``IoV2SingleReq`` and the bound slave port declares signature
``IoV2Beat`` (see ``IoV2SingleReq.bridge_to`` in ``gvsoc/signature.py``). The
per-combination replacement of the general ``IoV2BeatCollapseAdapter`` for
single-req masters (the ISS LSU, functional routers): its only substantive job
is translating the two allocation conventions — downstream read data arrives
in allocator-backed beats the adapter copies out and frees, while the upstream
response hands the master back its **own** request object (the single-req
identity contract). Because a single-req master carries its per-request state
itself, the adapter is stateless on the flow-control axis: any number of
accesses may be outstanding concurrently (no single-outstanding serialisation
like the collapse adapter), downstream DENYs propagate straight upstream and
retries are forwarded as-is. Pure writes are forwarded as size-0 pool beats
aliasing the master's payload (the beat target consumes and frees them; the
burst's single data-less ack resolves the access — per-burst write
acknowledgement, see io_v2.hpp); atomics are forwarded as the master's own
object, which keeps the classic round-trip as the ack.
"""

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2SingleReq, IoV2Beat


class IoV2SingleReqToBeatAdapterConfig(Config):
    """Configuration for the io_v2 single-req -> beat adapter."""

    beat_width: int = cfg_field(default=0, dump=True, desc=(
        "Beat width in bytes of the downstream beat slave (used for tracing "
        "and the single-beat contract checks in asserts builds)."
    ))


class IoV2SingleReqToBeatAdapter(Component):

    def __init__(self, parent: Component, name: str, beat_width: int):
        super().__init__(parent, name, config=IoV2SingleReqToBeatAdapterConfig(
            beat_width=beat_width))
        self.set_component('utils.io_v2_single_req_to_beat_adapter')
        self._beat_width = beat_width

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2SingleReq())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2Beat(self._beat_width))
