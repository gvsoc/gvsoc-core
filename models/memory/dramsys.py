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

import gvsoc.systree as st
import gvsoc.signature


class Dramsys(st.Component):

    def __init__(self, parent, name, pim_support=False, version=1):

        super(Dramsys, self).__init__(parent, name)

        if version not in (1, 2):
            raise ValueError(f"Dramsys: version must be 1 or 2, got {version}")

        self.version = version

        # Two separate C++ models: io.hpp (v1) and io_v2.hpp cannot
        # coexist in one translation unit (both define vp::IoReq), so the
        # v2 variant lives in dramsys_v2.cpp as its own .so. Pick which
        # one to load based on the requested protocol version.
        if version == 1:
            self.set_component('memory.dramsys')
        else:
            self.set_component('memory.dramsys_v2')

        self.add_properties({
            'require_systemc': True,
            'dram-type': 'hbm2-example.json',
            'pim-support': pim_support,
        })

    def i_INPUT(self) -> st.SlaveItf:
        if self.version == 1:
            return st.SlaveItf(self, 'input', signature='io')
        # v2: beat-protocol slave. IoV2BigPacket is the most permissive
        # signature — a beat-aware master (declaring IoV2Beat(beat_width)
        # on its side) gets the framework-inserted IoV2BeatAdapter for
        # free; a vanilla v2 master tolerates the beat-stream response
        # form per the protocol contract.
        return st.SlaveItf(self, 'input', signature=gvsoc.signature.IoV2BigPacket())

    # PIM ports are v1-only. The v2 wrapper does not implement them.
    def o_SENDMEMSPEC(self, itf: st.SlaveItf):
        assert self.version == 1, "PIM is not supported on the v2 dramsys wrapper"
        self.itf_bind('send_memspec', itf, signature="wire<GvsocMemspec>")

    def i_PIMTOGGLE(self) -> st.SlaveItf:
        assert self.version == 1, "PIM is not supported on the v2 dramsys wrapper"
        return st.SlaveItf(self, 'pim_toggle')

    def o_PIMNOTIFY(self, itf: st.SlaveItf):
        assert self.version == 1, "PIM is not supported on the v2 dramsys wrapper"
        self.itf_bind('pim_notify', itf, signature="wire<PimStride*>")

    def i_PIMDATA(self) -> st.SlaveItf:
        assert self.version == 1, "PIM is not supported on the v2 dramsys wrapper"
        return st.SlaveItf(self, 'pim_data', signature="wire<PimStride*>")
