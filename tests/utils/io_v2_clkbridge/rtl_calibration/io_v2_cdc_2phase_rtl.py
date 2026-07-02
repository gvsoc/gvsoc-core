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
Python generator for the IoV2Cdc2PhaseRtl bridge (calibration-only).

Wraps the locally-built `io_v2_cdc_rtl_bridge.cpp` C++ component (compiled
via ``add_sources`` so it lives only with the test, not in
``gvsoc/core/models/``). The component dlopens
``install/lib/vlt_cdc_2phase_fast.so`` β€” the Verilator-built plugin
produced by ``sim/cdc_2phase/Makefile`` β€” which wraps two common_cells
``cdc_2phase`` instances (forward + reverse).

The cdc_*_rtl bridge kinds exist solely to calibrate the C++ behavioural
models against the real RTL; they do not ship as part of the simulator
model library.
"""

import os

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2BigPacket


_PLUGIN_BASENAME = 'vlt_cdc_2phase_fast.so'


def _default_plugin_path() -> str:
    """Compute the install/lib path of the Verilator plugin .so.

    ``$GVSOC_WORKDIR`` overrides; otherwise we walk up from this file's
    location to the SDK root (rtl_calibration -> io_v2_clkbridge -> utils
    -> tests -> core -> gvsoc -> sdk) and append install/lib.
    """
    workdir = os.environ.get('GVSOC_WORKDIR')
    if workdir:
        return os.path.join(workdir, 'install', 'lib', _PLUGIN_BASENAME)
    here = os.path.dirname(os.path.abspath(__file__))
    sdk_root = os.path.abspath(os.path.join(here, '..', '..', '..', '..', '..', '..'))
    return os.path.join(sdk_root, 'install', 'lib', _PLUGIN_BASENAME)


class IoV2Cdc2PhaseRtl(Component):
    """Calibration-only wrapper for the cdc_2phase Verilator plugin."""

    def __init__(self, parent: Component, name: str,
                 plugin_path: str | None = None):
        super().__init__(parent, name)
        # Compile the generic RTL-bridge C++ locally alongside this test.
        # All three rtl wrappers (cdc_2phase, cdc_fifo_gray, cdc_fifo_2phase)
        # add the same source; the framework dedupes by (sources, c_flags)
        # hash, so only one shared .so gets built.
        self.add_sources(['io_v2_cdc_rtl_bridge.cpp'])
        # The bridge .cpp needs verilator_plugin.h from gvsoc/core/models/utils.
        # add_c_flags is the only knob exposed to test-local compilation.
        here = os.path.dirname(os.path.abspath(__file__))
        sdk_root = os.path.abspath(os.path.join(here, '..', '..', '..', '..', '..', '..'))
        plugin_inc = os.path.join(sdk_root, 'gvsoc', 'core', 'models', 'utils')
        self.add_c_flags([f'-I{plugin_inc}'])
        self.add_property('plugin_path', plugin_path or _default_plugin_path())

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2BigPacket())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2BigPacket())
