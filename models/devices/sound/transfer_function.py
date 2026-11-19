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


class TransferFunctionConfig(Config):
    """Configuration of the audio transfer function."""

    sample_rate: int = cfg_field(default=0, dump=True, desc=(
        "Sample rate of the audio stream, in Hz"
    ))



class TransferFunction(gvsoc.systree.Component):

    def __init__(self, parent, name, sample_rate=44100):
        super(TransferFunction, self).__init__(parent, name,
            config=TransferFunctionConfig(sample_rate=sample_rate))

        self.set_component('devices.sound.transfer_function')


    def i_AUDIO(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'audio_in', signature='audio')

    def o_AUDIO(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('audio_out', itf, signature='audio')

