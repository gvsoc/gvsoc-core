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

#
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#

import gvsoc.systree as st
from config_tree import Config, cfg_field


class Aps256xxnConfig(Config):
    """Configuration of the aps256xxn octospi RAM."""

    size: int = cfg_field(default=0x04000000, fmt="hex", dump=True, desc=(
        "Size of the RAM in bytes"
    ))

    rbx: bool = cfg_field(default=False, dump=True, desc=(
        "Row boundary crossing supported (MR3[7]): with it enabled in MR8, a linear burst read "
        "crosses the 2 KB rows instead of wrapping inside its row"
    ))

    legacy: bool = cfg_field(default=False, dump=True, desc=(
        "Behave as the first version of this model, which the OctoSPI interface model of "
        "gvsoc/gap (udma_hyper_v3) and its driver expect: data one clock later (the latency "
        "counted from the end of the address), the start address taken as is and the bursts "
        "not wrapping in their row"
    ))


class Aps256xxn(st.Component):
    """
    Aps256xxn

    AP Memory APS256XXN octal DDR PSRAM (Xccela, x8), driven edge by edge through an OctoSPI
    interface (``hyper`` signature: a chip-select sync then one call per useful CK edge, the
    read data going back on the same edges). It follows the datasheet (APS256XXN-OBx9):
    commands 00h/20h/40h (read), 80h/A0h/C0h (write), FFh (global reset), mode registers 0-4
    and 8, read and write latency codes, fixed latency, bursts wrapping in their 2 KB row,
    sync bursts following MR8, byte mask on bit 8 of the write data.

    Attributes
    ----------
    size : int
        Size of the RAM (default: 0x04000000), when no config is given. Without a config, the
        RAM is created in legacy mode (see Aps256xxnConfig.legacy), for the boards of gvsoc/gap.
    config : Aps256xxnConfig
        Configuration of the RAM.

    """

    def __init__(self, parent, name, config: Aps256xxnConfig = None, size=None):
        if config is None:
            config = Aps256xxnConfig(legacy=True)
            if size is not None:
                config.size = size

        super(Aps256xxn, self).__init__(parent, name, config=config)

        self.add_sources(['devices/ram/aps256xxn.cpp'])

    def i_INPUT(self) -> st.SlaveItf:
        return st.SlaveItf(self, itf_name='input', signature='hyper')
