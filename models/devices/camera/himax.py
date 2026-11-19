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

from typing import Annotated

import gvsoc.systree as st
from vp.clock_domain import Clock_domain
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class HimaxConfig(Config):
    """Configuration of the Himax camera model."""

    color_mode: str = cfg_field(default="gray", dump=True, desc=(
        "Color mode, 'gray' or 'raw'"
    ))
    width: int = cfg_field(default=324, dump=True, desc=(
        "Image width, in pixels"
    ))
    height: int = cfg_field(default=244, dump=True, desc=(
        "Image height, in pixels"
    ))
    pixel_size: int = cfg_field(default=0, dump=True, desc=(
        "Pixel size in bytes, 0 to take it from the color mode"
    ))
    vsync_polarity: int = cfg_field(default=1, dump=True, desc=(
        "Polarity of the vsync signal"
    ))
    hsync_polarity: int = cfg_field(default=1, dump=True, desc=(
        "Polarity of the hsync signal"
    ))
    endianness: str = cfg_field(default="little", dump=True, desc=(
        "Endianness of the pixels, 'little' or 'big'"
    ))
    # Runtime: a file which can change from one run to another
    image_stream: Annotated[str, Runtime] = cfg_field(default="", dump=True, desc=(
        "Path of the images streamed by the camera, empty for none"
    ))

class Himax_implem(st.Component):

    def __init__(self, parent, name):

        super(Himax_implem, self).__init__(parent, name, config=HimaxConfig())

        self.set_component('devices.camera.himax')


class Himax(st.Component):

    def __init__(self, parent, name):

        super(Himax, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=20000000)

        camera = Himax_implem(self, 'camera')


        self.bind(clock, 'out', camera, 'clock')
        self.bind(camera, 'cpi', self, 'cpi')
        self.bind(self, 'i2c', camera, 'i2c')
