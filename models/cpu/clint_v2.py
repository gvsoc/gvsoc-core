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

class ClintV2(gvsoc.systree.Component):
    """io_v2 variant of Clint, answering every access inline."""

    def __init__(self, parent, name, nb_cores=1, frequency=10000000):
        """
        Parameters
        ----------
        nb_cores: int
            Number of harts (one msip / mtimecmp pair each).
        frequency: int
            Frequency of the mtime counter in Hz (e.g. the RTC frequency when the
            CLINT time base is a real-time clock).
        """

        super(ClintV2, self).__init__(parent, name)

        self.add_sources(['cpu/clint_v2.cpp'])

        self.add_property('nb_cores', nb_cores)
        self.add_property('frequency', frequency)

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2Sync())

    def i_TIME(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'time', signature='wire<uint64_t>')

    def o_SW_IRQ(self, core: int, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'sw_irq_{core}', itf, signature=f'wire<bool>')

    def o_TIMER_IRQ(self, core: int, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'timer_irq_{core}', itf, signature=f'wire<bool>')
