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


class GdbserverConfig(Config):
    """Configuration of the GDB server."""

    enabled: Annotated[bool, Runtime] = cfg_field(default=False, dump=True, desc=(
        "True if the GDB server is opened (the --gdbserver option)"
    ))
    port: Annotated[int, Runtime] = cfg_field(default=12345, dump=True, desc=(
        "Port of the GDB server (the --gdbserver-port option)"
    ))
    default_hartid: str = cfg_field(default='', dump=True, desc=(
        "Name of the core active when GDB connects"
    ))


class Gdbserver(st.Component):

    def __init__(self, parent, name, enabled=False, port=12345, default_hartid=""):

        super(Gdbserver, self).__init__(parent, name, config=GdbserverConfig(enabled=enabled,
            port=port, default_hartid=default_hartid))

        self.add_sources([
            'gdbserver/gdbserver.cpp',
            'gdbserver/rsp.cpp',
            'gdbserver/rsp-packet-codec.cpp',
            'gdbserver/circular-buffer.cpp',
        ])

    def enable(self, port: int):
        """Open the GDB server on the given port for this run."""
        self._component_config.enabled = True
        self._component_config.port = port