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
import regmap.regmap
import regmap.regmap_md
import regmap.regmap_c_header


class Microphone_i2s(gvsoc.systree.Component):

    def __init__(self, parent, name, channel, frequency, width, generated_freq = 1000, input_file = None, ws_delay = 1):
        super(Microphone_i2s, self).__init__(parent, name)

        self.set_component('devices.sound.microphone.i2s.i2s_microphone')

        self.add_property('channel', channel)
        self.add_property('ws-delay', ws_delay)
        self.add_property('frequency', frequency)
        self.add_property('width', width)
        self.add_property('generated_freq', generated_freq)
        self.add_property('input_filepath', input_file)
