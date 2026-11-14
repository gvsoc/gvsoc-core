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


class ConverterConfig(Config):
    """Configuration of the width converter."""

    output_width: int = cfg_field(default=4, desc="Width in bytes of the output accesses")
    output_align: int = cfg_field(default=4, desc="Alignment in bytes of the output accesses")


class Converter(gvsoc.systree.Component):

    def __init__(self, parent, name, output_width=4, output_align=4):
        super(Converter, self).__init__(parent, name, config=ConverterConfig(
            output_width=output_width, output_align=output_align))

        self.add_sources(['interco/converter_impl.cpp'])

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
            return gvsoc.systree.SlaveItf(self, 'input', signature='io')

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
            self.itf_bind('out', itf, signature='io')
