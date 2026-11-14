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
from vp.clock_domain import Clock_domain
from gvrun.parameter import TargetParameter
from typing import Annotated
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class UartCheckerConfig(Config):
    """Configuration of the UART checker."""

    baudrate: int = cfg_field(default=115200, desc="UART baudrate")
    loopback: Annotated[bool, Runtime] = cfg_field(default=False, desc=(
        "Send back what is received"))
    stdout: bool = cfg_field(default=True, desc="Print what is received on the standard output")
    stdin: bool = cfg_field(default=False, desc="Send what is typed on the standard input")
    tx_file: str = cfg_field(default='tx_uart.log', desc=(
        "File where what is received is dumped, empty for none"))



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
            config = UartCheckerConfig()
            super(Uart_checker.Uart_checker_implem, self).__init__(parent, name, config=config)

            self.add_sources(['devices/uart/uart_checker.cpp'])

            loopback = TargetParameter(
                self, name='loopback', value=False, description='Enable uart loopback',
                cast=bool
            )
            config.loopback = loopback.value
