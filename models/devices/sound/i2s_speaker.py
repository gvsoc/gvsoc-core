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

class Speaker_implem(st.Component):

    def __init__(self, parent, name):

        super(Speaker_implem, self).__init__(parent, name)

        self.set_component('devices.sound.i2s_speaker')

        self.add_properties({
            "verbose": False,
            "ws-delay": 1,
            "width": 16,
            "out_mode": "file",
            "outfile": None
        })


class Speaker(st.Component):

    def __init__(self, parent, name):

        super(Speaker, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=50000000)

        sink = Speaker_implem(self, 'sink')


        self.bind(clock, 'out', sink, 'clock')
        self.bind(sink, 'clock_cfg', clock, 'clock_in')
        self.bind(sink, 'i2s', self, 'i2s')
