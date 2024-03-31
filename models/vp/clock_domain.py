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

class Clock_domain(gvsoc.systree.Component):
    """Clock domain

    This model can be used to define a clock domain.\n
    This instantiates a clock generator which can then be connected to components which are
    part of the clock domain.\n
    The clock domain starts with the specified frequency and its frequency can then be dynamically
    changed through a dedicated interface, so that all components of the frequency domain
    are clocked with the new frequency.\n

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    frequency: int
        The initial frequency of the clock generator.
    factor: int
        Multiplication factor. The actual output frequency will be multiplied by this factor.
        This can be used for example to be able to schedule events on both raising
        and falling edges.
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str, frequency: int, factor: int=1):
        super().__init__(parent, name)

        self.set_component('vp.clock_engine_module')

        self.add_properties({
            'frequency': frequency,
            'factor': factor
        })

    def gen_gtkw(self, tree, comp_traces):

        tree.add_trace(self, 'cycles', 'cycles', tag='clock')
        tree.add_trace(self, 'period', 'period', tag='overview')

    def o_CLOCK(self, itf: gvsoc.systree.SlaveItf):
        """Binds the output clock port.

        This port can be connected to any component which should be clocked by this
        clock generator.\n
        Several components can be bound on it.\n
        In case the frequency is dynamically modified, all bound components are notified and
        will  be automatically using the new frequency.\n
        It instantiates a port of type vp::ClkMaster.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('out', itf, signature='clock')

    def i_CTRL(self) -> gvsoc.systree.SlaveItf:
        """Returns the port for controlling the clock generator.

        This can be used to dynamically change the frequency.\n
        It instantiates a port of type vp::ClockMaster.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, 'clock_in', signature='clock_ctrl')

    def gen_gui(self, parent_signal):
        active = gvsoc.gui.Signal(self, parent_signal, name=self.name, path='period', groups='regmap', display=gvsoc.gui.DisplayBox())

        return active
