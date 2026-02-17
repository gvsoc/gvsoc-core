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

class Dramsys(st.Component):

    def __init__(self, parent, name, pim_support=False):

        super(Dramsys, self).__init__(parent, name)

        self.set_component('memory.dramsys')

        self.add_properties({
            'require_systemc': True,
            'dram-type': 'hbm2-example.json',
            'pim-support': pim_support,
        })

    def i_INPUT(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'input')
    
    def o_SENDMEMSPEC(self, itf: st.SlaveItf):
        self.itf_bind('send_memspec', itf, signature="wire<GvsocMemspec>")
    
    def i_PIMTOGGLE(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'pim_toggle')
    
    def o_PIMNOTIFY(self, itf: st.SlaveItf):
        self.itf_bind('pim_notify', itf, signature="wire<PimStride*>")
    
    def i_PIMDATA(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'pim_data', signature="wire<PimStride*>")
    
