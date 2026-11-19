#
# Copyright (C) 2022 ETH Zurich and University of Bologna
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
import os.path
from config_tree import Config, cfg_field


class CoreEmulationConfig(Config):
    """Configuration of the core emulation model."""

    fetch_enable: bool = cfg_field(default=False, dump=True, desc=(
        "True if the core starts fetching at reset"
    ))
    core_id: int = cfg_field(default=0, dump=True, desc=(
        "Core identifier"
    ))
    cluster_id: int = cfg_field(default=0, dump=True, desc=(
        "Cluster identifier"
    ))


class CoreEmulation(st.Component):
    def __init__(self,
            parent,
            name,
            cluster_id: int=0,
            core_id: int=0,
            fetch_enable: bool=False,
            *kargs, **kwargs):

        # core_id and cluster_id were never given to the model, which then
        # always saw 0: keep it so.
        super().__init__(parent, name, config=CoreEmulationConfig(fetch_enable=fetch_enable))

        self.set_component('cpu.emulation.emulation')

