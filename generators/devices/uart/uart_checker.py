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
from ips.clock.clock_domain import Clock_domain


class Uart_checker(st.Component):

    def __init__(self, parent, name):
        super(Uart_checker, self).__init__(parent, name)

        uart_checker = Uart_checker.Uart_checker_implem(self, 'uart_checker')

        clock = Clock_domain(self, 'clock', frequency=50000000)

        self.bind(self, 'input', uart_checker, 'input')
        self.bind(clock, 'out', uart_checker, 'clock')
        self.bind(uart_checker, 'clock_cfg', clock, 'clock_in')


    class Uart_checker_implem(st.Component):

        def __init__(self, parent, name):
            super(Uart_checker.Uart_checker_implem, self).__init__(parent, name)

            self.set_component('devices.uart.uart_checker')

            self.add_property('verbose', False)
            self.add_property('baudrate', 115200)
            self.add_property('loopback', False)
            self.add_property('stdout', True)
            self.add_property('stdin', False)
            self.add_property('telnet', False)
            self.add_property('tx_file', 'tx_uart.log')

