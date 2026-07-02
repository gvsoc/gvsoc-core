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
Python generators for the IoV2ClockBridge component.

One C++ component (utils.io_v2_clock_bridge) backs four kinds:

  IoV2ClockBridge       k_src=0 k_dst=0 depth=1   (sync_only, default)
  IoV2Cdc2PhaseBeh      k_src=1 k_dst=2 depth=1
  IoV2CdcFifoGrayBeh    k_src=1 k_dst=2 depth=2
  IoV2CdcFifo2PhaseBeh  k_src=3 k_dst=3 depth=2

With k_src = k_dst = 0 the C++ takes the fast pass-through path
(engine sync + forward; no event scheduling). Any non-zero k switches
to the parametric per-stage ClockEvent path with cycle-accurate
deadlines and FIFO pipelining up to ``depth`` in-flight transactions.

The four classes share an implementation; subclasses just set
per-kind defaults. The cdc_*_beh defaults are calibrated against the
matching common_cells RTL via the io_v2_clkbridge test β€” see
gvsoc/core/tests/utils/io_v2_clkbridge/rtl_calibration/.
"""

from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2BigPacket


class IoV2ClockBridge(Component):
    """v2 IO clock-domain bridge β€” default is sync_only (zero modeled latency)."""

    _DEFAULT_K_SRC = 0
    _DEFAULT_K_DST = 0
    _DEFAULT_DEPTH = 1

    def __init__(self, parent: Component, name: str, *,
                 k_src_per_dir: int | None = None,
                 k_dst_per_dir: int | None = None,
                 depth: int | None = None):
        super().__init__(parent, name)
        # Compile the bridge .cpp on demand alongside any target that uses
        # it (the framework dedupes by source-hash, so many bridges sharing
        # the same source produce a single .so). The cmake build does NOT
        # declare this as a vp_model β€” keeping the bridge self-contained
        # under utils/.
        #
        # Path is relative to the target-dir search list (gapy walks
        # GVSOC_MODULES); gvsoc/core/models is one of those, so prefixing
        # with utils/ resolves to this file's location.
        self.add_sources(['utils/io_v2_clock_bridge.cpp'])
        self.add_property('k_src_per_dir',
                          k_src_per_dir if k_src_per_dir is not None
                                        else self._DEFAULT_K_SRC)
        self.add_property('k_dst_per_dir',
                          k_dst_per_dir if k_dst_per_dir is not None
                                        else self._DEFAULT_K_DST)
        self.add_property('depth',
                          depth if depth is not None else self._DEFAULT_DEPTH)

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature=IoV2BigPacket())

    def o_OUTPUT(self, slave: SlaveItf):
        self.itf_bind('output', slave, signature=IoV2BigPacket())


class IoV2Cdc2PhaseBeh(IoV2ClockBridge):
    """Behavioural model of common_cells `cdc_2phase`. Single-beat by design."""
    _DEFAULT_K_SRC = 1
    _DEFAULT_K_DST = 2
    _DEFAULT_DEPTH = 1


class IoV2CdcFifoGrayBeh(IoV2ClockBridge):
    """Behavioural model of common_cells `cdc_fifo_gray`. Pipelines via depth."""
    _DEFAULT_K_SRC = 1
    _DEFAULT_K_DST = 2
    _DEFAULT_DEPTH = 2


class IoV2CdcFifo2PhaseBeh(IoV2ClockBridge):
    """Behavioural model of common_cells `cdc_fifo_2phase`. ~2x cdc_2phase."""
    _DEFAULT_K_SRC = 3
    _DEFAULT_K_DST = 3
    _DEFAULT_DEPTH = 2
