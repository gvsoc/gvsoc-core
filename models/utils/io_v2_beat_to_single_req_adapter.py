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
    read_bursts_until_issued: bool = cfg_field(default=False, dump=True, desc=(
        "Count a read burst against max_read_bursts only until its last sub-read "
        "is issued, not until its last beat is delivered, as the RTL axi2mem, whose "
        "read channel takes the next address once it has sent the memory requests of "
        "a burst while the data of that burst is still coming back."
    ))
    max_sub_reads: int = cfg_field(default=32, dump=True, desc=(
        "Maximum number of beat-sized sub-reads outstanding downstream. The default "
        "covers the round-trip latency so reads stream at one beat per cycle. 1 "
        "models a bridge holding a single request per port, like the RTL axi2mem, "
        "which also keeps the responses in order behind a fabric reaching targets "
        "of different latencies."
    ))
    read_latency: int = cfg_field(default=0, dump=True, desc=(
        "Cycles added to every read response beat, on top of the latency reported by the "
        "downstream. It is a pipeline delay: it shifts the response stream without "
        "limiting how many accesses are in flight."
    ))
    write_latency: int = cfg_field(default=0, dump=True, desc=(
        "Cycles added to every write acknowledgement, on top of the latency reported by "
        "the downstream. It is a pipeline delay, like read_latency."
    ))
    write_burst_half_cycles: int = cfg_field(default=0, dump=True, desc=(
        "Cost of accepting one write burst, in half cycles (3 = a burst every 1.5 cycles: "
        "two accepted back to back, then one cycle of back-pressure). 0 (default) accepts "
        "a burst every cycle. Only the first beat of a burst pays it, the other beats "
        "stream at one per cycle."
    ))
    read_lane_width: int = cfg_field(default=0, dump=True, desc=(
        "Width in bytes of the memory lanes a read beat is spread over (8 = two 64-bit "
        "lanes for a 128-bit beat), as in the PULP axi2mem with one outstanding read per "
        "lane: every read beat puts an entry in the 2-entry queue of each lane, real on "
        "the lanes it uses, empty on the others; a lane takes its next entry one cycle "
        "after a real one and two after an empty one, and a beat is accepted only when "
        "both queues have room. So narrow reads on one lane go one every two cycles, "
        "full beats one per cycle. 0 (default) issues a read beat every cycle whatever "
        "its size."
    ))


class IoV2BeatToSingleReqAdapter(Component):

    def __init__(self, parent: Component, name: str, beat_width: int,
                 max_read_bursts: int = 4, max_sub_reads: int = 32, read_latency: int = 0, write_latency: int = 0,
                 write_burst_half_cycles: int = 0, read_lane_width: int = 0,
                 read_bursts_until_issued: bool = False):
        super().__init__(parent, name, config=IoV2BeatToSingleReqAdapterConfig(
            beat_width=beat_width, max_read_bursts=max_read_bursts,
            max_sub_reads=max_sub_reads,
            read_latency=read_latency, write_latency=write_latency,
            write_burst_half_cycles=write_burst_half_cycles,
            read_lane_width=read_lane_width,
            read_bursts_until_issued=read_bursts_until_issued))
        self.set_component('utils.io_v2_beat_to_single_req_adapter')
        self._beat_width = beat_width

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2Beat(self._beat_width))

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2SingleReq())
