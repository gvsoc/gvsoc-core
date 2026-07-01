# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from gvsoc.signature import IoV2SingleReq


class StubTarget(gvsoc.systree.Component):
    """io_v2 single-req testbench target.

    Answers each request with a SINGLE-BEAT response in one of three forms:
      - inline ``IO_REQ_DONE`` (default),
      - async ``IO_REQ_GRANTED`` + one ``resp()`` (``async_resp=True``),
      - ``IO_REQ_DENIED`` + ``retry()`` for the first ``deny_count`` requests
        (request-path back-pressure).
    Never a multi-beat stream — that is the IoV2SingleReq contract the adapter
    checks in asserts builds.
    """
    def __init__(self, parent, name, latency=0, duration=0, error=False,
                 async_resp=False, deny_count=0, retry_delay=2,
                 reorder_resp=False, base=0, size=0, logname=None):
        super().__init__(parent, name)
        self.add_sources(['stub_target.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('latency', latency)
        self.add_property('duration', duration)
        self.add_property('error', error)
        self.add_property('async_resp', async_resp)
        self.add_property('deny_count', deny_count)
        self.add_property('retry_delay', retry_delay)
        # Negative-test knob: deliberately answer async requests out of issue
        # order (swap adjacent pairs) to prove the adapter's in-order assert fires.
        self.add_property('reorder_resp', reorder_resp)
        self.add_property('base', base)
        self.add_property('size', size)

    def i_INPUT(self):
        return gvsoc.systree.SlaveItf(self, 'input', signature=IoV2SingleReq())
