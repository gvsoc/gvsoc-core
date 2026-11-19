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

import gvsoc.systree
from config_tree import Config, cfg_field
import regmap.regmap
import regmap.regmap_md
import regmap.regmap_c_header


class I2sMicrophoneConfig(Config):
    """Configuration of the I2S microphone (devices.sound.microphone.i2s)."""

    channel: str = cfg_field(default="", dump=True, desc=(
        "Channel of the microphone, 'left' or 'right'"
    ))
    ws_delay: int = cfg_field(default=0, dump=True, desc=(
        "Number of cycles after the ws edge before the first bit is sent"
    ))
    frequency: int = cfg_field(default=0, dump=True, desc="Frequency of the I2S clock")
    width: int = cfg_field(default=0, dump=True, desc="Width of the samples, in bits")
    generated_freq: int = cfg_field(default=0, dump=True, desc=(
        "Frequency of the generated sine wave when there is no input file"
    ))
    input_filepath: str = cfg_field(default="", dump=True, desc=(
        "Input audio file, empty for none"
    ))


class Microphone_i2s(gvsoc.systree.Component):

    def __init__(self, parent, name, channel, frequency, width, generated_freq = 1000, input_file = None, ws_delay = 1):
        super(Microphone_i2s, self).__init__(parent, name, config=I2sMicrophoneConfig(
            channel=channel, ws_delay=ws_delay, frequency=frequency, width=width,
            generated_freq=generated_freq,
            input_filepath=input_file if input_file is not None else ''))

        self.set_component('devices.sound.microphone.i2s.i2s_microphone')

