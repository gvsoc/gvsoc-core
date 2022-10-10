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


class Hyperram(st.Component):
    """
    Hyperram

    This instantiates a s27ks0641 RAM (hyperram).

    Attributes
    ----------
    size : int
        Size of the RAM (default: 0x00800000).

    """

    def __init__(self, parent, name, size=0x00800000):
        super(Hyperram, self).__init__(parent, name)

        # Register all parameters as properties so that they can be overwritten from the command-line
        self.add_property('size', size)

        self.set_component('devices.hyperbus.hyperram_impl')
