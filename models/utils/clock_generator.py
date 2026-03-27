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
from gvsoc.systree import Component, SlaveItf
from config_tree import Config, cfg_field

class ClockGeneratorConfig(Config):
    powered_on: bool = cfg_field(default=True, dump=True, desc=(
        "True if the generator is powered-up at startup"
    ))
    powerup_time: int = cfg_field(default=0, dump=True, desc=(
        "Time required for the clock generator to be powered-up"
    ))

class ClockGenerator(Component):

    def __init__(self, parent: Component, name: str, config: ClockGeneratorConfig):
        super(ClockGenerator, self).__init__(parent, name, config=config)

        self.add_sources(['utils/clock_impl.cpp'])

    def o_CLOCK_SYNC(self, itf: SlaveItf):
        self.itf_bind('clock_sync', itf, signature='clock')

    def o_CLOCK_CTRL(self, itf: SlaveItf):
        self.itf_bind('clock_ctrl', itf, signature='clock')


class Clock_generator(st.Component):

    def __init__(self, parent, name, powered_on=True, powerup_time=0):

        config = ClockGeneratorConfig(powered_on=powered_on, powerup_time=powerup_time)

        super(Clock_generator, self).__init__(parent, name, config=config)

        self.add_sources(['utils/clock_impl.cpp'])

        self.add_properties({
            'powered_on': powered_on,
            'powerup_time': powerup_time
        })
