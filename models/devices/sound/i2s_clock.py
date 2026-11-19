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
from config_tree import Config, cfg_field


class I2sClockConfig(Config):
    """Configuration of the I2S clock generator."""

    is_ws: bool = cfg_field(default=False, dump=True, desc=(
        "True to generate the ws signal instead of the sck one"
    ))
    frequency: int = cfg_field(default=0, dump=True, desc=(
        "Frequency of the generated clock, 0 for none"
    ))


class I2sClock(st.Component):
    """I2S clock generator, driving the sck (or ws) of an I2S bus."""

    def __init__(self, parent: st.Component, name: str, frequency: int=0, is_ws: bool=False):
        super().__init__(parent, name, config=I2sClockConfig(frequency=frequency, is_ws=is_ws))

        self.set_component('devices.sound.i2s_clock')

    def o_I2S(self, itf: st.SlaveItf):
        self.itf_bind('i2s', itf, signature='i2s')

    def o_CLOCK_CFG(self, itf: st.SlaveItf):
        self.itf_bind('clock_cfg', itf, signature='clock')
