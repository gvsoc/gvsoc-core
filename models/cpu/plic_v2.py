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
from gvsoc.signature import IoV2Sync

class PlicV2(gvsoc.systree.Component):
    """io_v2 variant of Plic, answering every access inline."""

    def __init__(self, parent, name, ndev=0):

        super(PlicV2, self).__init__(parent, name)

        self.add_sources(['cpu/plic_v2.cpp'])

        self.add_property('ndev', ndev)


    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2Sync())

    def i_IRQ(self, device) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'irq{device+1}', signature='wire<bool>')

    def o_S_IRQ(self, core: int, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f's_irq_{core}', itf, signature=f'wire<bool>')

    def o_M_IRQ(self, core: int, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'm_irq_{core}', itf, signature=f'wire<bool>')
