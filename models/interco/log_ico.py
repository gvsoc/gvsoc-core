# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from gvsoc.systree import Component, SlaveItf
from config_tree import Config, cfg_field

class LogIcoConfig(Config):

    nb_masters: int = cfg_field(default=0, dump=True, desc=(
        "Number of masters"
    ))
    nb_slaves: int = cfg_field(default=0, dump=True, desc=(
        "Number of slaves"
    ))
    interleaving_width: int = cfg_field(default=0, dump=True, desc=(
        "Number of bits for the interleaving"
    ))
    remove_offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Offset to be removed to the incoming requests"
    ))

class LogIco(Component):
    def __init__(self, parent: Component, name: str, config: LogIcoConfig):
        super().__init__(parent, name, config=config)

        self.add_sources(['interco/log_ico.cpp'])

    def i_INPUT(self, id: int) -> SlaveItf:
        return SlaveItf(self, f'input_{id}', signature='io')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        self.itf_bind(f'output_{id}', itf, signature='io')
