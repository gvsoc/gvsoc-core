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


class Ns16550V2Config(Config):
    """Configuration of the ns16550 UART."""

    offset_shift: int = cfg_field(default=0, dump=True, desc=(
        "Shift applied to the register offsets (log2 of the register stride)"
    ))

from gvsoc.signature import IoV2Sync

class Ns16550V2(gvsoc.systree.Component):
    """io_v2 variant of Ns16550, answering every access inline."""

    def __init__(self, parent, name, offset_shift=0):

        super(Ns16550V2, self).__init__(parent, name, config=Ns16550V2Config(offset_shift=offset_shift))

        self.add_sources(['devices/uart/ns16550_v2.cpp'])
        

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2Sync())

    def o_IRQ(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('irq', itf, signature='wire<bool>')
