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

import gvsoc.systree

class Generator(gvsoc.systree.Component):

    def __init__(self, parent, name, nb_pending_reqs=64):

        super(Generator, self).__init__(parent, name)

        self.add_property('nb_pending_reqs', nb_pending_reqs)

        self.add_sources(['interco/traffic/generator.cpp'])

    def i_CONTROL(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'control', signature='wire<TrafficGeneratorConfig>')

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='io')

    def gen_gui(self, parent_signal):
        top = gvsoc.gui.Signal(self, parent_signal, name=self.name, path="req_addr", groups=['regmap'])
        gvsoc.gui.Signal(self, top, "req_size", path="req_size", groups=['regmap'])
        gvsoc.gui.Signal(self, top, "req_is_write", path="req_is_write", groups=['regmap'])
        gvsoc.gui.Signal(self, top, "send_size", path="send_size", groups=['regmap'])
        gvsoc.gui.Signal(self, top, "pending_size", path="pending_size", groups=['regmap'])
        gvsoc.gui.Signal(self, top, "stalled", path="stalled", display=gvsoc.gui.DisplayPulse(), groups=['regmap'])
        gvsoc.gui.Signal(self, top, "busy", path="busy", display=gvsoc.gui.DisplayPulse(), groups=['regmap'])
