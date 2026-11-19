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
from vp.clock_domain import Clock_domain
from config_tree import Config, cfg_field


class SoundI2sMicrophoneConfig(Config):
    """Configuration of the I2S microphone (devices.sound.i2s_microphone)."""

    channel: str = cfg_field(default="", dump=True, desc=(
        "Channel of the microphone, 'left' or 'right'"
    ))
    ws_delay: int = cfg_field(default=0, dump=True, desc=(
        "Number of cycles after the ws edge before the first bit is sent"
    ))
    width: int = cfg_field(default=0, dump=True, desc="Width of the samples, in bits")
    frequency: int = cfg_field(default=0, dump=True, desc=(
        "Frequency of the generated clock, 0 if the clock comes from outside"
    ))
    enabled: bool = cfg_field(default=False, dump=True, desc=(
        "True if the microphone is enabled at startup"
    ))
    pdm: int = cfg_field(default=0, dump=True, desc=(
        "Non-zero for a PDM microphone"
    ))
    stim_mode: str = cfg_field(default="", dump=True, desc=(
        "Stimuli mode, 'file' or 'incr'"
    ))
    stim: str = cfg_field(default="", dump=True, desc=(
        "Stimuli file (.hex, .raw or .wav) in 'file' mode, empty for none"
    ))
    stim_incr_start: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "First sample in 'incr' mode"
    ))
    stim_incr_value: int = cfg_field(default=0, dump=True, desc=(
        "Increment between samples in 'incr' mode"
    ))

class Microphone_implem(st.Component):

    def __init__(self, parent, name):

        # pdm was given as a boolean that the model read as an integer, which always gave 0:
        # keep the model seeing 0.
        super(Microphone_implem, self).__init__(parent, name, config=SoundI2sMicrophoneConfig(
            width=16, pdm=0, stim_mode="file", stim_incr_start=0x55555555, stim_incr_value=1,
            stim="", frequency=0, channel="right", ws_delay=1, enabled=False))

        self.set_component('devices.sound.i2s_microphone')


class Microphone(st.Component):

    def __init__(self, parent, name):

        super(Microphone, self).__init__(parent, name)

        clock = Clock_domain(self, 'clock', frequency=50000000)

        mic = Microphone_implem(self, 'mic')


        self.bind(clock, 'out', mic, 'clock')
        self.bind(mic, 'clock_cfg', clock, 'clock_in')
        self.bind(mic, 'i2s', self, 'i2s')
        self.bind(mic, 'ws_out', self, 'ws_out')
        self.bind(self, 'ws_in', mic, 'ws_in')
