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

from typing import Annotated

import gvsoc.systree as st
import utils.uart.uart_adapter
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class UartVipConfig(Config):
    """Configuration of the UART VIP."""

    # Runtime: these come from target parameters, which can be set for each run
    baudrate: Annotated[int, Runtime] = cfg_field(default=0, dump=True, desc=(
        "UART baudrate in bps"
    ))
    data_bits: Annotated[int, Runtime] = cfg_field(default=0, dump=True, desc=(
        "Number of data bits"
    ))
    stop_bits: Annotated[int, Runtime] = cfg_field(default=0, dump=True, desc=(
        "Number of stop bits"
    ))
    parity: Annotated[bool, Runtime] = cfg_field(default=False, dump=True, desc=(
        "True if parity is enabled"
    ))
    ctrl_flow: Annotated[bool, Runtime] = cfg_field(default=False, dump=True, desc=(
        "True to enable the control flow"
    ))
    ctrl_flow_limiter: Annotated[int, Runtime] = cfg_field(default=0, dump=True, desc=(
        "Control flow limiter, 0 for none"
    ))
    loopback: bool = cfg_field(default=False, dump=True, desc=(
        "True to send back what is received"
    ))
    stdout: bool = cfg_field(default=False, dump=True, desc=(
        "True to print what is received on the standard output"
    ))
    rx_file: Annotated[str, Runtime] = cfg_field(default="", dump=True, desc=(
        "File where what is received is dumped, empty for none"
    ))


class UartVip(st.Component):

    def __init__(self, parent, name, baudrate: int=None, loopback: bool=False,
            stdout: bool=False, rx_file: str=None, data_bits: int=None, stop_bits: int=None,
            parity: bool=None, ctrl_flow: bool=None):
        config = UartVipConfig()

        super(UartVip, self).__init__(parent, name, config=config)

        utils.uart.uart_adapter.UartAdapter(self)

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

        config.baudrate = baudrate
        config.loopback = loopback
        config.stdout = stdout
        config.rx_file = rx_file if rx_file is not None else ''
        config.data_bits = data_bits
        config.stop_bits = stop_bits
        config.parity = parity
        config.ctrl_flow = ctrl_flow
        config.ctrl_flow_limiter = ctrl_flow_limiter

    def i_UART(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'input', signature='io')
