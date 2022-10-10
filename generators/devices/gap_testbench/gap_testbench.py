#
# Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

import gsystree as st
from gap.gap9.gap9_evk import Gap9_evk


class Gap_testbench(st.Component):

    def __init__(self, parent, name):
        super(Gap_testbench, self).__init__(parent, name)

        gap = Gap9_evk(self, 'gap0')

        firmware = 'devices/gap_testbench/gap_firmware.img'
        firmware_img = self.get_file_path(firmware)

        if firmware_img is None:
            raise RuntimeError('Could not find gap firmware: ' + firmware)

        self.add_property('gap0/flash/content/image', firmware_img)

        self.bind(gap, 'i2s0', self, 'i2s0')
        self.bind(gap, 'i2s1', self, 'i2s1')
        self.bind(gap, 'i2s2', self, 'i2s2')

