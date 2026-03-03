# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from config_tree import Config
from gvsoc.systree import Component, SlaveItf

class RwSplitterConfig(Config):
    pass

class RwSplitter(Component):
    def __init__(self, parent: Component, name: str, config: RwSplitterConfig):
        super().__init__(parent, name, config=config)

        self.add_sources(['interco/rw_splitter.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature='io')

    def o_READ_OUTPUT(self, itf: SlaveItf):
        self.itf_bind(f'output_read', itf, signature='io')

    def o_WRITE_OUTPUT(self, itf: SlaveItf):
        self.itf_bind(f'output_write', itf, signature='io')
