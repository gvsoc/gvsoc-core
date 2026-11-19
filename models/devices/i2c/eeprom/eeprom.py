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

import gvsoc.systree as st
from vp.clock_domain import Clock_domain
from config_tree import Config, cfg_field


class I2cEepromConfig(Config):
    """Configuration of the I2C eeprom."""

    address: int = cfg_field(default=80, dump=True, desc=(
        "I2C address of the eeprom"
    ))
    page_size: int = cfg_field(default=8, dump=True, desc=(
        "Size of a page in bytes"
    ))
    number_of_pages: int = cfg_field(default=128, dump=True, desc=(
        "Number of pages"
    ))


class Eeprom(st.Component):

    def __init__(self, parent, name, address=80):
        super(Eeprom, self).__init__(parent, name)

        eeprom = Eeprom.Eeprom_implem(self, 'eeprom', address=address)

        clock = Clock_domain(self, 'clock', frequency=10000000)

        self.bind(clock, 'out', eeprom, 'clock')
        self.bind(eeprom, 'i2c', self, 'i2c')
        self.bind(eeprom, 'clock_cfg', clock, 'clock_in')


    class Eeprom_implem(st.Component):

        def __init__(self, parent, name, address):
            super(Eeprom.Eeprom_implem, self).__init__(parent, name, config=I2cEepromConfig(
                address=address, page_size=16, number_of_pages=256))

            self.set_component('devices.i2c.eeprom.i2c_eeprom')
