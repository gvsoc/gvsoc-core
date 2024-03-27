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


class UartVip(st.Component):

    def __init__(self, parent, name, baudrate: int=115200, loopback: bool=False,
            stdout: bool=False, rx_file: str=None, data_bits: int=8, stop_bits: int=1,
            parity: bool=False):
        super(UartVip, self).__init__(parent, name)

        self.add_sources(['devices/uart/uart_vip.cpp'])

        self.add_property('baudrate', baudrate)
        self.add_property('loopback', loopback)
        self.add_property('stdout', stdout)
        self.add_property('rx_file', rx_file)
        self.add_property('data_bits', data_bits)
        self.add_property('stop_bits', stop_bits)
        self.add_property('parity', parity)

