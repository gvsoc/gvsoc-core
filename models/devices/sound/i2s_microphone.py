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

class Microphone_implem(st.Component):

    def __init__(self, parent, name):

        super(Microphone_implem, self).__init__(parent, name)

        self.set_component('devices.sound.i2s_microphone')

        self.add_properties({
            "verbose": False,
            "width": 16,
            "pdm": True,
            "stim_mode": "file",
            "stim_incr_start": "0x55555555",
            "stim_incr_value": "1",
            "stim": None,
            "frequency": 0,
            "channel": "right",
            "ws-delay": 1,
            "sample-rate": 44100,
            "enabled": False
        })


class Microphone(st.Component):

    def __init__(self, parent, name):

        super(Microphone, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=50000000)

        mic = Microphone_implem(self, 'mic')


        self.bind(clock, 'out', mic, 'clock')
        self.bind(mic, 'clock_cfg', clock, 'clock_in')
        self.bind(mic, 'i2s', self, 'i2s')
        self.bind(mic, 'ws_out', self, 'ws_out')
        self.bind(self, 'ws_in', mic, 'ws_in')
