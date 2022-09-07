#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
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

from devices.flash.flash import Flash


class Mx25uw6445g(Flash):
    """
    Mx25uw6445g

    This instantiates an mx25uw6445g flash (octospi flash).

    Attributes
    ----------
    size : int
        Size of the flash (default: 0x08000000).

    """

    def __init__(self, parent, name, size=None):
        super().__init__(parent, name)

        if size is None:
            size = 0x08000000

        self.set_component('devices.flash.mx25uw6445g')

        self.add_property('writeback', True)
        self.add_property('size', size)
