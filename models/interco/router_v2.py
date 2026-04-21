# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from __future__ import annotations

import gvsoc.systree
from interco.router_config import RouterMapping, RouterConfig


# Router implementation kinds. Each maps to a different C++ source.
KIND_UNTIMED      = 'untimed'
KIND_BANDWIDTH    = 'bandwidth'
KIND_BACKPRESSURE = 'backpressure'
KIND_BEAT         = 'beat'

_SOURCES = {
    KIND_UNTIMED:      'interco/router/router_v2_untimed.cpp',
    KIND_BANDWIDTH:    'interco/router/router_v2_bandwidth.cpp',
    KIND_BACKPRESSURE: 'interco/router/router_v2_backpressure.cpp',
    KIND_BEAT:         'interco/router/router_v2_beat.cpp',
}


class Router(gvsoc.systree.Component):
    """Address-decoding router on the io_v2 protocol.

    Four implementations are available via the `kind` parameter:

    - **'untimed'**: zero timing. Address decode + forward, nothing else. Fastest.
      Use for pure functional simulation or when timing is modeled elsewhere.
    - **'bandwidth'** (default): rate-limited, non-blocking. Requests are always
      accepted; latency grows with the per-input/per-output bandwidth backlog and
      is reported via `req->latency` (no ClockEvent scheduling). A master can
      pipeline an arbitrary number of outstanding requests without handshaking.
    - **'backpressure'**: rate-limited, blocking. Same bandwidth model, but the
      router returns `IO_REQ_DENIED` while a previous request from the same input
      is still in its delayed-forward window. The master must wait for retry().
    - **'beat'**: beat-streaming, AXI-like. Each IoReq is one beat of size <=
      `width` bytes. Bursts are tracked by `burst_id` (remapped through a fixed
      per-router burst table). Multiple inputs compete for outputs via round-robin
      arbitration with burst atomicity.

    Relevant parameters by kind:

    .. list-table::
       :header-rows: 1

       * - Parameter
         - untimed
         - bandwidth
         - backpressure
         - beat
       * - ``latency``
         - \\-
         - yes
         - yes
         - yes
       * - ``bandwidth``
         - \\-
         - yes
         - yes
         - \\-
       * - ``shared_rw_bandwidth``
         - \\-
         - yes
         - yes
         - \\-
       * - ``width``
         - \\-
         - \\-
         - \\-
         - yes
       * - ``max_input_pending_size``
         - \\-
         - \\-
         - \\-
         - yes
       * - ``max_pending_bursts``
         - \\-
         - \\-
         - \\-
         - yes

    Ignored parameters are passed through to the C++ side as properties but the
    model simply doesn't read them.
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 config: RouterConfig,
                 kind: str = KIND_BANDWIDTH,
                 latency: int = 0,
                 width: int = 0,
                 max_pending_bursts: int = 0):
        super(Router, self).__init__(parent, name, config=config)

        if kind not in _SOURCES:
            raise ValueError(f"Router kind must be one of {list(_SOURCES)}, got {kind!r}")

        self.kind = kind
        self.shared_rw_bandwidth = config.shared_rw_bandwidth

        self.add_property('mappings', {})
        self.add_property('latency', latency)
        self.add_property('bandwidth', config.bandwidth)
        self.add_property('shared_rw_bandwidth', config.shared_rw_bandwidth)
        self.add_property('width', width)
        self.add_property('max_input_pending_size', config.max_input_pending_size)
        self.add_property('max_pending_bursts', max_pending_bursts)
        self.add_property('nb_input_port', 1)

        self.add_sources([_SOURCES[kind]])

    def add_mapping(self, name: str, base: int = 0, size: int = 0,
                    remove_offset: int = 0, add_offset: int = 0, latency: int = 0):
        self.get_property('mappings')[name] = {
            'base': base,
            'size': size,
            'remove_offset': remove_offset,
            'add_offset': add_offset,
            'latency': latency,
        }

    def __alloc_input_port(self, id):
        nb = self.get_property('nb_input_port')
        if id >= nb:
            self.add_property('nb_input_port', id + 1)

    def i_INPUT(self, id: int = 0) -> gvsoc.systree.SlaveItf:
        self.__alloc_input_port(id)
        if id == 0:
            return gvsoc.systree.SlaveItf(self, 'input', signature='io_v2')
        return gvsoc.systree.SlaveItf(self, f'input_{id}', signature='io_v2')

    def o_MAP(self, itf: gvsoc.systree.SlaveItf, name: str = None, base: int = 0,
              size: int = 0, rm_base: bool = True, remove_offset: int = 0,
              latency: int = 0, mapping: RouterMapping = None):
        if mapping is not None:
            rm_base = mapping.remove_base
            base = mapping.base
            size = mapping.size
            name = mapping.name
        if rm_base and remove_offset == 0:
            remove_offset = base
        if name is None:
            name = itf.component.name
        self.add_mapping(name, base=base, remove_offset=remove_offset, size=size,
                         latency=latency)
        self.itf_bind(name, itf, signature='io_v2')
