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

import gsystree as st
from vp.clock_domain import Clock_domain

class Himax_implem(st.Component):

    def __init__(self, parent, name):

        super(Himax_implem, self).__init__(parent, name)

        self.set_component('devices.camera.himax')

        self.add_properties({
            "color-mode": "gray",
            "width": 324,
            "height": 244,
            "pixel-size": 0,
            "vsync-polarity": 1,
            "hsync-polarity": 1,
            "endianness": "little",
            "image-stream": None
        })


class Himax(st.Component):

    def __init__(self, parent, name):

        super(Himax, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=20000000)

        camera = Himax_implem(self, 'camera')


        self.bind(clock, 'out', camera, 'clock')
        self.bind(camera, 'cpi', self, 'cpi')
        self.bind(self, 'i2c', camera, 'i2c')
