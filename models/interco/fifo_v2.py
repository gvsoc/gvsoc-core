# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Generic io_v2 request/response FIFO buffer.

Models a small request FIFO (e.g. the ``lint_FIFO`` placed in front of a
TCDM logarithmic interconnect on silicon): it decouples an upstream master
from the cycle-by-cycle back-pressure of a downstream slave. The upstream
master is granted as soon as the FIFO has room, so the downstream's
``DENIED``/``retry()`` handshake is absorbed locally and never propagates
back to the master.
"""

from __future__ import annotations

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf
from gvsoc.signature import IoV2SingleReq


class FifoConfig(Config):
    """Configuration for the io_v2 FIFO buffer.

    Attributes
    ----------
    depth : int
        Number of outstanding requests the FIFO can hold. While the FIFO
        has fewer than ``depth`` requests buffered, the upstream master is
        granted immediately; once full, further requests are denied until a
        slot frees.
    latency : int
        Cycles between a request being accepted upstream and being driven
        downstream (the FIFO's register stage). Minimum 1.
    resp_latency : int
        Cycles between the downstream response and the response being sent
        upstream (the response registers). The request keeps its slot
        meanwhile. 0 sends it in the same cycle.
    done_latency : int
        Cycles added to ``resp_latency`` when the downstream completes the
        request inline (a synchronous target, which gives its response the
        cycle after it is granted on a real bus). 0 by default.
    done_annotated_latency : bool
        True to also add, when the downstream completes the request inline,
        the latency it annotated on it (a synchronous memory behind a
        crossbar). False by default.
    max_outstanding : int
        Number of requests which can be waiting for their downstream response
        at the same time. With 1, a request only goes downstream once the
        previous one has been answered.
    """

    depth:   int = cfg_field(default=2, dump=True, desc=(
        "Number of outstanding requests buffered before back-pressuring upstream"
    ))
    latency: int = cfg_field(default=1, dump=True, desc=(
        "Cycles between accepting a request upstream and issuing it downstream"
    ))
    resp_latency: int = cfg_field(default=0, dump=True, desc=(
        "Cycles between the downstream response and the response sent upstream"
    ))
    done_latency: int = cfg_field(default=0, dump=True, desc=(
        "Cycles added to the response latency when the downstream completes the request inline"
    ))
    done_annotated_latency: bool = cfg_field(default=False, dump=True, desc=(
        "Add the latency annotated by the downstream to the response latency when it completes "
        "the request inline"
    ))
    max_outstanding: int = cfg_field(default=1, dump=True, desc=(
        "Number of requests which can be waiting for their downstream response"
    ))


class Fifo(Component):
    """Generic io_v2 request/response FIFO buffer.

    Sits on an io_v2 path and decouples the upstream master from the
    downstream slave's back-pressure:

    - **Upstream (slave) side** — an incoming request is accepted with
      ``IO_REQ_GRANTED`` whenever fewer than ``depth`` requests are
      buffered, and ``IO_REQ_DENIED`` (with a later ``retry()``) when the
      FIFO is full. The master therefore never observes the downstream's
      per-cycle deny.
    - **Downstream (master) side** — buffered requests are driven out in
      order. A downstream ``IO_REQ_DENIED`` is handled locally: the FIFO
      holds the request and re-issues it synchronously from its
      ``retry()`` handler (so it works in front of a synchronous
      crossbar that requires same-cycle re-issue). On completion the
      response is forwarded upstream via ``resp()``.

    Requests are neither split nor reordered; the FIFO is transparent to
    the payload.
    """

    __gvsoc_doc__ = {
        'title': 'IO FIFO (v2)',
    }

    def __init__(self, parent: Component, name: str, config: FifoConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['interco/fifo_v2.cpp'])

    def i_INPUT(self) -> SlaveItf:
        """Master-facing slave port. Returns ``IO_REQ_GRANTED`` while the
        FIFO has room, ``IO_REQ_DENIED`` when full.

        Single-req contract: the FIFO completes a request on the FIRST
        downstream resp(), so it cannot sit on a multi-beat response
        stream — a beat neighbour gets the appropriate converter
        auto-inserted by the signature system.
        """
        return SlaveItf(self, 'input', signature=IoV2SingleReq())

    def o_OUTPUT(self, itf: SlaveItf):
        """Binds the downstream master port the buffered requests are
        driven through."""
        self.itf_bind('output', itf, signature=IoV2SingleReq())
