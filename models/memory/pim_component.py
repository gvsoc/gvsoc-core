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

class PimComponent(st.Component):

    def __init__(self, parent, name):

        super(PimComponent, self).__init__(parent, name)

        self.set_component('memory.pim_component')

    def i_RCVMEMSPEC(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'rcv_memspec', signature="wire<GvsocMemspec>")

    def i_PIMNOTIFY(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'pim_notify', signature="wire<PimStride*>")
    
    def o_PIMDATA(self, itf: st.SlaveItf):
        self.itf_bind('pim_data', itf, signature="wire<PimStride*>")