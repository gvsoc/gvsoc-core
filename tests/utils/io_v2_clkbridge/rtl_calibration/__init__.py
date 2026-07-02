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
Calibration-only RTL bridge kinds.

Importing this package registers ``cdc_2phase_rtl``, ``cdc_fifo_gray_rtl``
and ``cdc_fifo_2phase_rtl`` in :mod:`gvsoc.clock_bridges`. The kinds are
deliberately not registered globally (in
``gvsoc/engine/python/gvsoc/clock_bridges.py``) because the RTL bridges
are *not* part of the shipped simulator model library β€” they exist only
to calibrate the C++ behavioural bridges against real common_cells RTL.

The Verilator plugins they dlopen (``vlt_cdc_*_fast.so``) must be built
separately via ``sim/<kind>/Makefile``. The test's top-level Makefile
invokes those builds before each test run.
"""

from gvsoc.clock_bridges import register


def _cdc_2phase_rtl_factory(parent, name, **opts):
    from rtl_calibration.io_v2_cdc_2phase_rtl import IoV2Cdc2PhaseRtl
    plugin_path = opts.pop('plugin_path', None)
    if opts:
        raise RuntimeError(
            f"'cdc_2phase_rtl' takes only 'plugin_path', got extra: {sorted(opts)}")
    return IoV2Cdc2PhaseRtl(parent, name, plugin_path=plugin_path)


def _cdc_fifo_gray_rtl_factory(parent, name, **opts):
    from rtl_calibration.io_v2_cdc_fifo_gray_rtl import IoV2CdcFifoGrayRtl
    plugin_path = opts.pop('plugin_path', None)
    if opts:
        raise RuntimeError(
            f"'cdc_fifo_gray_rtl' takes only 'plugin_path', got extra: {sorted(opts)}")
    return IoV2CdcFifoGrayRtl(parent, name, plugin_path=plugin_path)


def _cdc_fifo_2phase_rtl_factory(parent, name, **opts):
    from rtl_calibration.io_v2_cdc_fifo_2phase_rtl import IoV2CdcFifo2PhaseRtl
    plugin_path = opts.pop('plugin_path', None)
    if opts:
        raise RuntimeError(
            f"'cdc_fifo_2phase_rtl' takes only 'plugin_path', got extra: {sorted(opts)}")
    return IoV2CdcFifo2PhaseRtl(parent, name, plugin_path=plugin_path)


register('cdc_2phase_rtl', _cdc_2phase_rtl_factory)
register('cdc_fifo_gray_rtl', _cdc_fifo_gray_rtl_factory)
register('cdc_fifo_2phase_rtl', _cdc_fifo_2phase_rtl_factory)
