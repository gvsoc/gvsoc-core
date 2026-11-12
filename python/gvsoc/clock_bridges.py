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
Registry of clock-domain-crossing bridge IP kinds for io_v2 bindings.

A "kind" is a string tag that the systree binding pass uses to pick which
bridge component to splice in when a v2 master and its bound slave resolve
to different clock sources. Each kind maps to a factory ``(parent, name,
signature, **opts) -> Component`` that instantiates the corresponding wrapper,
``signature`` being the io_v2 signature of the crossing.

System designers can:

  - Leave the default ``'sync_only'`` (matches v1 auto-crossing: bring the
    remote engine up to date before forwarding, no modeled latency; a
    response is resynchronized on the next edge of the master clock).
  - Use ``'shared_clock'`` between two domains driven by the same clock
    (shared edges, no synchronizer on the chip): no resynchronization, only
    a register slice, a request or a response crossing in one cycle.
  - Override per binding via ``itf_bind(..., clock_bridge='async_fifo')`` or
    ``clock_bridge=('async_fifo', {'depth': 4})``.
  - Set a policy on a parent component via ``set_clock_bridge_policy`` for
    all crossings between a specific (src_clock, dst_clock) pair.
  - Register additional kinds with :func:`register`.
"""

_REGISTRY = {}


def register(tag, factory):
    """Register a bridge kind.

    ``factory`` is ``(parent, name, signature, **opts) -> Component``, with
    ``signature`` the io_v2 signature of the crossing, and must return a
    component that exposes ``i_INPUT()`` / ``o_OUTPUT(slave)`` with an io_v2
    signature.
    """
    _REGISTRY[tag] = factory


def make(tag, parent, name, signature, **opts):
    """Instantiate a registered bridge kind for a crossing of the given signature."""
    if tag not in _REGISTRY:
        raise RuntimeError(
            f"Unknown clock-bridge kind '{tag}'. "
            f"Registered: {sorted(_REGISTRY)}")
    return _REGISTRY[tag](parent, name, signature, **opts)


def _sync_only_factory(parent, name, signature, **opts):
    # Lazy import so the registry can be populated without loading the
    # bridge wrapper at module import time.
    from utils.io_v2_clock_bridge import IoV2ClockBridge
    if opts:
        raise RuntimeError(
            f"'sync_only' clock bridge takes no options, got {sorted(opts)}")
    return IoV2ClockBridge(parent, name)


def _shared_clock_factory(parent, name, signature, **opts):
    # Two domains driven by the same clock (shared edges, no synchronizer):
    # a one-cycle register slice, see utils.io_v2_shared_clock_bridge.
    # Option: write_depth, the number of write requests it holds (default 1).
    from utils.io_v2_shared_clock_bridge import IoV2SharedClockBridge
    unknown = set(opts) - {'write_depth'}
    if unknown:
        raise RuntimeError(
            f"'shared_clock' clock bridge takes only write_depth, got {sorted(unknown)}")
    return IoV2SharedClockBridge(parent, name, signature=signature, **opts)


def _cdc_2phase_beh_factory(parent, name, signature, **opts):
    from utils.io_v2_clock_bridge import IoV2Cdc2PhaseBeh
    return IoV2Cdc2PhaseBeh(parent, name, **opts)


def _cdc_fifo_gray_beh_factory(parent, name, signature, **opts):
    from utils.io_v2_clock_bridge import IoV2CdcFifoGrayBeh
    return IoV2CdcFifoGrayBeh(parent, name, **opts)


def _cdc_fifo_2phase_beh_factory(parent, name, signature, **opts):
    from utils.io_v2_clock_bridge import IoV2CdcFifo2PhaseBeh
    return IoV2CdcFifo2PhaseBeh(parent, name, **opts)


register('sync_only', _sync_only_factory)
register('shared_clock', _shared_clock_factory)
register('cdc_2phase_beh', _cdc_2phase_beh_factory)
register('cdc_fifo_gray_beh', _cdc_fifo_gray_beh_factory)
register('cdc_fifo_2phase_beh', _cdc_fifo_2phase_beh_factory)

# Note: cdc_*_rtl kinds are calibration-only and not registered here.
# Tests that need them register locally β€” see
# tests/calibration/io_v2_clkbridge/rtl_calibration/__init__.py of the SDK.
