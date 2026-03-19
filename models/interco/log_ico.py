# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from gvsoc.systree import Component, SlaveItf
from interco.log_ico_config import LogIcoConfig

class LogIco(Component):
    def __init__(self, parent: Component, name: str, config: LogIcoConfig):
        super().__init__(parent, name, config=config)

        self.add_sources(['interco/log_ico.cpp'])

    def i_INPUT(self, id: int) -> SlaveItf:
        return SlaveItf(self, f'input_{id}', signature='io')

    def o_OUTPUT(self, id: int, itf: SlaveItf):
        self.itf_bind(f'output_{id}', itf, signature='io')
