#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

import gvsoc.systree
import gvsoc.signature


class GeneratorV2(gvsoc.systree.Component):
    """v2 traffic generator (io_v2 output)."""

    def __init__(self, parent, name, nb_pending_reqs=64, max_burst_size=0,
                 width: int = 0, beat_stream: bool = False):

        super().__init__(parent, name)

        # Beat width of the plane this generator drives, used for the output
        # port's IoV2Beat signature. Must match the bound slave port's width
        # so the bind is direct: the generator measures the path's bandwidth
        # and must see the raw response beat stream, unmodified by adapters.
        if width <= 0:
            raise RuntimeError(
                f'{name}: GeneratorV2 requires width= (the beat width of '
                f'the port it drives) for its IoV2Beat output signature')
        self._width = width

        self.add_property('nb_pending_reqs', nb_pending_reqs)
        # Legalize generated bursts like an AXI DMA: no burst exceeds
        # max_burst_size or crosses a max_burst_size boundary (the 4KB rule).
        # 0 (default) keeps the raw packet_size chunking.
        self.add_property('max_burst_size', max_burst_size)
        self.add_property('width', width)
        # Write bursts streamed as beats of the port width, one per cycle,
        # like an AXI DMA master (is_first/is_last/burst_id). False sends each
        # burst as one request of any size.
        self.add_property('beat_stream', beat_stream)

        self.add_sources(['interco/traffic/generator_v2.cpp'])

    def i_CONTROL(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'control', signature='wire<TrafficGeneratorConfig>')

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        # Beat-plane terminal master: the generator natively consumes raw
        # per-beat response streams (it measures the path's bandwidth, so the
        # stream must reach it unmodified) and its pool-backed write requests
        # follow the per-burst acknowledgement contract (consumed/freed by
        # the target, one data-less ack back). Its runtime packet_size chunks
        # are one-beat bursts of any size, or, with beat_stream, write bursts
        # of beats of `width`; `width` also names the plane so the bind
        # against the driven beat port is direct and adapter-free.
        self.itf_bind('output', itf, signature=gvsoc.signature.IoV2Beat(self._width))
