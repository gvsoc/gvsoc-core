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
import gvsoc.runner

import vp.clock_domain
from model import Model
from testbench import Testbench
from gvrun.parameter import TargetParameter


class Test(gvsoc.systree.Component):

    def __init__(self, parent, name):
        super().__init__(parent, name)

        dut = Model(self, 'dut')

        testbench = Testbench(self, 'testbench')

        testbench.o_DUT_INPUT(dut.i_INPUT())
        dut.o_OUTPUT(testbench.i_DUT_OUTPUT())


# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Chip(gvsoc.systree.Component):

    def __init__(self, parent, name=None):

        super().__init__(parent, name)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)
        soc = Test(self, 'soc')
        clock.o_CLOCK    (soc.i_CLOCK    ())




# This is the top target that gvrun will instantiate
class Target(gvsoc.runner.Target):

    description = "Custom system"
    model = Chip
    name = "test"
