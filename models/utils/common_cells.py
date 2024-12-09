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

class And(gvsoc.systree.Component):
    def __init__(self, parent: gvsoc.systree.Component, name: str, nb_input: int=0):

        super().__init__(parent, name)

        self.add_sources(['utils/common_cells/and.cpp'])

        self.add_properties({
            'nb_input': nb_input
        })

    def i_INPUT(self, id: int) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'input_{id}', signature='wire<bool>')

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='wire<bool>')
