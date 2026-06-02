#
# Copyright (C) 2026 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
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

"""
Python generator for the cdc_fifo_2phase Verilator-backed CDC bridge.

Wraps the generic ``utils.io_v2_cdc_rtl_bridge`` C++ component, pointing
``plugin_path`` at the Verilator plugin built by
``hw/cdc_rtl/cdc_fifo_2phase/sim/Makefile``. The plugin wraps a
forward+reverse pair of common_cells `cdc_fifo_2phase` instances and presents
the same CdcBridgeExchange ABI as the cdc_2phase plugin, so the bridge
component itself is unchanged.
"""

import os

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2BigPacket


_PLUGIN_BASENAME = 'vlt_cdc_fifo_2phase_fast.so'


def _default_plugin_path() -> str:
    workdir = os.environ.get('GVSOC_WORKDIR')
    if workdir:
        return os.path.join(workdir, 'install', 'lib', _PLUGIN_BASENAME)
    here = os.path.dirname(os.path.abspath(__file__))
    sdk_root = os.path.abspath(os.path.join(here, '..', '..', '..', '..', '..', '..'))
    return os.path.join(sdk_root, 'install', 'lib', _PLUGIN_BASENAME)


class IoV2CdcFifo2PhaseRtl(Component):
    """Wrapper for the cdc_fifo_2phase Verilator plugin, backed by the generic
    ``utils.io_v2_cdc_rtl_bridge`` C++ component."""

    def __init__(self, parent: Component, name: str,
                 plugin_path: str | None = None):
        super().__init__(parent, name)
        self.add_sources(['io_v2_cdc_rtl_bridge.cpp'])
        here = os.path.dirname(os.path.abspath(__file__))
        sdk_root = os.path.abspath(os.path.join(here, '..', '..', '..', '..', '..', '..'))
        plugin_inc = os.path.join(sdk_root, 'gvsoc', 'core', 'models', 'utils')
        self.add_c_flags([f'-I{plugin_inc}'])
        self.add_property('plugin_path', plugin_path or _default_plugin_path())

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2BigPacket())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2BigPacket())
