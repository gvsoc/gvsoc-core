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


class OcdJtagBitbangConfig(Config):
    """Configuration of the OpenOCD remote-bitbang JTAG proxy."""

    enabled: Annotated[bool, Runtime] = cfg_field(default=False, dump=True, desc=(
        "True to open the proxy socket at startup"
    ))
    port: Annotated[int, Runtime] = cfg_field(default=0, dump=True, desc=(
        "TCP port of the proxy socket"
    ))


class OcdJtagBitbang(st.Component):
    """JTAG master driven by OpenOCD through its remote-bitbang protocol."""

    def __init__(self, parent: st.Component, name: str, enabled: bool=False, port: int=0):
        super().__init__(parent, name, config=OcdJtagBitbangConfig(enabled=enabled, port=port))

        self.set_component('devices.jtag.ocd_jtag_bitbang')

    def o_JTAG(self, itf: st.SlaveItf):
        self.itf_bind('jtag', itf, signature='jtag')
