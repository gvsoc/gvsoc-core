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
import gapylib.target
import devices.utils.uart_adapter


class UartVip(st.Component):

    def __init__(self, parent, name, baudrate: int=None, loopback: bool=False,
            stdout: bool=False, rx_file: str=None, data_bits: int=None, stop_bits: int=None,
            parity: bool=None, ctrl_flow: bool=None):
        super(UartVip, self).__init__(parent, name)

        devices.utils.uart_adapter.UartAdapter(self)

        self.add_sources(['devices/uart/uart_vip.cpp'])

        if baudrate is None:
            baudrate = self.declare_user_property(
                name='baudrate', value=115200, cast=int,
                description='Specifies uart baudrate in bps'
            )

        if parity is None:
            parity = self.declare_user_property(
                name='parity', value=False, cast=bool,
                description='Specifies if parity is enabled'
            )

        if data_bits is None:
            data_bits = self.declare_user_property(
                name='data_bits', value=8, cast=int,
                description='Specifies number of data bits'
            )

        if stop_bits is None:
            stop_bits = self.declare_user_property(
                name='stop_bits', value=1, cast=int,
                description='Specifies number of stop bits'
            )

        if ctrl_flow is None:
            ctrl_flow = self.declare_user_property(
                name='ctrl_flow', value=False, cast=bool,
                description='Enables control flow'
            )

        ctrl_flow_limiter = self.declare_user_property(
            name='ctrl_flow_limiter', value=0, cast=int,
            description='Enables control flow limiter'
        )

        self.add_property('baudrate', baudrate)
        self.add_property('loopback', loopback)
        self.add_property('stdout', stdout)
        self.add_property('rx_file', rx_file)
        self.add_property('data_bits', data_bits)
        self.add_property('stop_bits', stop_bits)
        self.add_property('parity', parity)
        self.add_property('ctrl_flow', ctrl_flow)
        self.add_property('ctrl_flow_limiter', ctrl_flow_limiter)
