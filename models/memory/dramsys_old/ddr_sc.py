#
# Copyright (C) 2018 ETH Zurich and University of Bologna
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
from config_tree import Config, cfg_field


class DdrScConfig(Config):
    """Configuration of the old SystemC DDR bridge (memory/dramsys_old/ddr_sc_impl.cpp)."""

    size: int = cfg_field(default=0, fmt="hex", dump=True, desc="Size of the DDR in bytes")
    gem5_config: str = cfg_field(default="", dump=True, desc=(
        "gem5 config file, relative to the gem5 path (gem5 bridge)"
    ))
    dramsys_config: str = cfg_field(default="", dump=True, desc=(
        "DRAMSys simulation file, relative to the DRAMSys simulations (DRAMSys bridge)"
    ))


class DdrSc(st.Component):
    """Old DDR model bridged to SystemC TLM (gem5 or DRAMSys)."""

    def __init__(self, parent: st.Component, name: str, size: int=0, gem5_config: str='',
            dramsys_config: str=''):
        super().__init__(parent, name, config=DdrScConfig(size=size, gem5_config=gem5_config,
            dramsys_config=dramsys_config))

        self.add_sources(['memory/dramsys_old/ddr_sc_impl.cpp'])
        self.add_properties({'require_systemc': True})

    def i_INPUT(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'input', signature='io')
