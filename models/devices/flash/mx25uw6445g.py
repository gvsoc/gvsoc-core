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

import gvsoc.systree as st
from config_tree import Config, cfg_field

from devices.flash.flash import Flash


class Mx25uw6445gConfig(Config):
    """Configuration of the mx25uw6445g octospi flash."""

    size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Size of the flash in bytes"
    ))
    writeback: bool = cfg_field(default=False, dump=True, desc=(
        "True to map the preload file so that the flash content is written back to it"
    ))
    preload_file: str = cfg_field(default="", dump=True, desc=(
        "File preloaded into the flash, empty for none"
    ))


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
        if size is None:
            size = 0x08000000

        config = Mx25uw6445gConfig(size=size, writeback=True)

        super().__init__(parent, name, config=config)

        self.set_component('devices.flash.mx25uw6445g')

        config.preload_file = self.get_image_path()
