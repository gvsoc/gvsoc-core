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

import gvsoc.systree as st
from vp.clock_domain import Clock_domain
from config_tree import Config, cfg_field


class I2sSpeakerConfig(Config):
    """Configuration of the I2S speaker."""

    ws_delay: int = cfg_field(default=0, dump=True, desc=(
        "Number of cycles after the ws edge before the first bit is sampled"
    ))
    width: int = cfg_field(default=0, dump=True, desc="Width of the samples, in bits")
    sample_rate: int = cfg_field(default=0, dump=True, desc="Sample rate, in Hz")
    out_mode: str = cfg_field(default="", dump=True, desc="Output mode, 'file' to dump to a file")
    outfile: str = cfg_field(default="", dump=True, desc=(
        "Output file (.hex or .wav) in 'file' mode, empty for none"
    ))

class Speaker_implem(st.Component):

    def __init__(self, parent, name):

        super(Speaker_implem, self).__init__(parent, name, config=I2sSpeakerConfig(
            ws_delay=1, width=16, out_mode="file", outfile=""))

        self.set_component('devices.sound.i2s_speaker')


class Speaker(st.Component):

    def __init__(self, parent, name):

        super(Speaker, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=50000000)

        sink = Speaker_implem(self, 'sink')


        self.bind(clock, 'out', sink, 'clock')
        self.bind(sink, 'clock_cfg', clock, 'clock_in')
        self.bind(sink, 'i2s', self, 'i2s')
