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
from config_tree import Config, cfg_field


class DramsysConfig(Config):
    """Configuration of the DRAMSys wrapper (memory.dramsys and memory.dramsys_v2)."""

    dram_type: str = cfg_field(default="", dump=True, desc=(
        "DRAMSys simulation config file, in dramsys_configs. Empty for hbm2-example.json"
    ))
    pim_support: bool = cfg_field(default=False, dump=True, desc=(
        "True to enable PIM support (v1 only)"
    ))
    access_size: int = cfg_field(default=0, dump=True, desc=(
        "Access size in bytes of the memspec, checked against it (v2 only)"
    ))


class Dramsys(st.Component):

    def __init__(self, parent, name, pim_support=False, version=1,
                 dram_type='hbm2-example.json', access_size: int = 0):

        super(Dramsys, self).__init__(parent, name, config=DramsysConfig(
            dram_type=dram_type, pim_support=pim_support, access_size=access_size))

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

        # v2: the beat width of the input port. The true value comes from
        # the DRAMSys memspec, which only the DRAMSys library can compute, so
        # the target description must state it here and the C++ constructor
        # fatals if it does not match the memspec (fail loudly, no silent
        # width mismatch).
        if version == 2 and access_size <= 0:
            raise ValueError(
                "Dramsys v2 requires access_size= (the memspec access size "
                "in bytes) for its IoV2Beat input signature; the C++ model "
                "validates it against the DRAMSys memspec at startup")
        self.access_size = access_size

        # Read by the runner, which then starts the engine with SystemC
        self.add_properties({
            'require_systemc': True,
        })

    def i_INPUT(self) -> st.SlaveItf:
        if self.version == 1:
            return st.SlaveItf(self, 'input', signature='io')
        # v2: beat-protocol slave at the declared access_size (validated
        # against the DRAMSys memspec by the C++ constructor).
        return st.SlaveItf(self, 'input',
                           signature=gvsoc.signature.IoV2Beat(self.access_size))

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
