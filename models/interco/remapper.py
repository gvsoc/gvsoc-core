# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from gvsoc.systree import Component, SlaveItf
from interco.remapper_config import RemapperConfig

class Remapper(Component):
    def __init__(self, parent: Component, name: str, config: RemapperConfig):
        super().__init__(parent, name, config=config)

        self.add_sources(['interco/remapper.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature='io')

    def o_OUTPUT(self, itf: SlaveItf):
        self.itf_bind(f'output', itf, signature='io')
