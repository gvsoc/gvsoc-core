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
import regmap.regmap
import regmap.regmap_md
import regmap.regmap_c_header


class Microphone_pdm(st.Component):

    def __init__(self, parent, name, input_filepath = "microphone_in.wav", slot = None,  generated_freq = 1000):
        super(Microphone_pdm, self).__init__(parent, name)

        self.set_component('devices.sound.microphone.pdm.pdm_microphone')

        self.add_property('input_filepath', input_filepath)
        self.add_property('slot', slot)
        self.add_property('generated_freq', generated_freq)