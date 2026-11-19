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


class DdrConfig(Config):
    """Configuration of the simple DDR model (memory/ddr_impl.cpp)."""

    size: int = cfg_field(default=0, fmt="hex", dump=True, desc="Size of the DDR in bytes")


class Ddr(st.Component):
    """Simple DDR model, without any timing."""

    def __init__(self, parent: st.Component, name: str, size: int=0):
        super().__init__(parent, name, config=DdrConfig(size=size))

        self.add_sources(['memory/ddr_impl.cpp'])

    def i_INPUT(self) -> st.SlaveItf:
        return st.SlaveItf(self, 'input', signature='io')
