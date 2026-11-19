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
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class BusWatchpointArgConfig(Config):
    """One argument given to the application through the getmainvars syscall."""

    value: str = cfg_field(default='', desc=(
        "Argument string"
    ))


class BusWatchpointConfig(Config):
    """Configuration of the bus watchpoint (riscv fesvr syscall proxy)."""

    # Runtime: computed in configure() from the binary given for the run
    riscv_fesvr_tohost_addr: Annotated[int, Runtime] = cfg_field(default=0, fmt="hex",
        dump=True, desc=(
        "Address of the tohost variable"
    ))
    riscv_fesvr_fromhost_addr: Annotated[int, Runtime] = cfg_field(default=0, fmt="hex",
        dump=True, desc=(
        "Address of the fromhost variable"
    ))
    args: list[BusWatchpointArgConfig] = cfg_field(default_factory=list, desc=(
        "Arguments given to the application, after the application name"
    ))


class Bus_watchpoint(st.Component):

    def __init__(self, parent, name, riscv_fesvr_tohost_addr, riscv_fesvr_fromhost_addr, word_size, args=None):

        if args is None:
            args = []

        super(Bus_watchpoint, self).__init__(parent, name, config=BusWatchpointConfig(
            riscv_fesvr_tohost_addr=riscv_fesvr_tohost_addr,
            riscv_fesvr_fromhost_addr=riscv_fesvr_fromhost_addr,
            args=[BusWatchpointArgConfig(value=arg) for arg in args]))

        if word_size == 64:
            self.set_component('interco.bus_watchpoint_64')
        else:
            self.set_component('interco.bus_watchpoint_32')

    def set_fesvr_addresses(self, tohost_addr: int, fromhost_addr: int):
        """Set the tohost/fromhost addresses, e.g. from the binary of the run."""
        self.get_config().riscv_fesvr_tohost_addr = tohost_addr
        self.get_config().riscv_fesvr_fromhost_addr = fromhost_addr