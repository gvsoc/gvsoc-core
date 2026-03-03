# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from config_tree import Config, cfg_field
from gvsoc.systree import Component, SlaveItf

class DemuxConfig(Config):

    offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "First bit used to extract the target index"
    ))
    width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Number of bit used to extract the target index"
    ))

class Demux(Component):
    def __init__(self, parent: Component, name: str, config: DemuxConfig):
        super().__init__(parent, name, config=config)

        self.add_sources(['interco/demux.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature='io')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        self.itf_bind(f'output_{id}', itf, signature='io')
