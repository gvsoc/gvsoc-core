
# SPDX-FileCopyrightText: 2026 ETH Zurich and University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from gvsoc.systree import Component, SlaveItf
from config_tree import Config, cfg_field

class ClockMuxConfig(Config):
    nb_clocks: int = cfg_field(default=1, dump=True, desc=(
        "Number of input clocks"
    ))
    selected_clock: int = cfg_field(default=0, dump=True, desc=(
        "Selected input clock"
    ))

class ClockMux(Component):

    def __init__(self, parent: Component, name: str, config: ClockMuxConfig):
        super(ClockMux, self).__init__(parent, name, config=config)

        self.add_sources(['utils/clock_mux.cpp'])

    def i_CLOCK_IN(self, id: int) -> SlaveItf:
        return SlaveItf(self, itf_name=f'clock_in_{id}', signature='clock')

    def o_CLOCK_OUT(self, itf: SlaveItf):
        self.itf_bind('clock_out', itf, signature='clock')

    def i_CLOCK_CTRL(self) -> SlaveItf:
        return SlaveItf(self, itf_name='clock_ctrl', signature='wire<int>')
