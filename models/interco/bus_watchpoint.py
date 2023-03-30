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

import gsystree as st

class Bus_watchpoint(st.Component):

    def __init__(self, parent, name, riscv_fesvr_tohost_addr, riscv_fesvr_fromhost_addr, word_size, args=None):

        super(Bus_watchpoint, self).__init__(parent, name)

        if args is None:
            args = []

        if word_size == 64:
            self.set_component('interco.bus_watchpoint_64')
        else:
            self.set_component('interco.bus_watchpoint_32')

        self.add_properties({
            "riscv_fesvr_tohost_addr": riscv_fesvr_tohost_addr,
            "riscv_fesvr_fromhost_addr": riscv_fesvr_fromhost_addr,
            "args": args
        })