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

#
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#

import gsystree as st


class Aps256xxn(st.Component):
    """
    Aps256xxn

    This instantiates an aps256xxn RAM (octospi ram).

    Attributes
    ----------
    size : int
        Size of the RAM (default: 0x04000000).

    """

    def __init__(self, parent, name, size=None):
        super(Aps256xxn, self).__init__(parent, name)

        if size is None:
            size = 0x04000000

        # Register all parameters as properties so that they can be overwritten from the command-line
        self.add_property('size', size)

        self.set_component('devices.ram.aps256xxn')
