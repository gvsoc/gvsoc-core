#
# Copyright (C) 2022 ETH Zurich and University of Bologna
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
import os.path

class CoreEmulation(st.Component):
    def __init__(self,
            parent,
            name,
            cluster_id: int=0,
            core_id: int=0,
            fetch_enable: bool=False,
            *kargs, **kwargs):

        super().__init__(parent, name)

        self.set_component('cpu.emulation.emulation')

        self.add_property('fetch_enable', fetch_enable)

