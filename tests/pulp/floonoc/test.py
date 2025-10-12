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

import math

import gvsoc.systree
import gvsoc.runner

import vp.clock_domain
import pulp.floonoc.floonoc
from pulp.floonoc.floonoc import FlooNocDirection
import interco.traffic.generator
from interco.traffic.receiver import Receiver
from memory.memory import Memory
from gvrun.parameter import TargetParameter


class FloonocTest(gvsoc.systree.Component):

    def __init__(self, parent, name, nb_cluster_x, nb_cluster_y, cluster_base, cluster_size):
        super().__init__(parent, name)

        self.add_property('nb_cluster_x', nb_cluster_x)
        self.add_property('nb_cluster_y', nb_cluster_y)
        self.add_property('cluster_base', cluster_base)
        self.add_property('cluster_size', cluster_size)

        self.add_sources(['test.cpp'])
        self.add_sources(['test0.cpp'])

    def o_NOC_NI(self, x, y, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'noc_ni_{x}_{y}', itf, signature='io')

    def o_GENERATOR_CONTROL(self, x, y, is_wide: bool, itf: gvsoc.systree.SlaveItf):
        is_wide_str = 'w' if is_wide else 'n'
        self.itf_bind(f'generator_control_{x}_{y}_{is_wide_str}', itf, signature='wire<TrafficGeneratorConfig>')

    def o_RECEIVER_CONTROL(self, x, y, is_wide: bool, itf: gvsoc.systree.SlaveItf):
        is_wide_str = 'w' if is_wide else 'n'
        self.itf_bind(f'receiver_control_{x}_{y}_{is_wide_str}', itf, signature='wire<TrafficReceiverConfig>')

    def i_CLUSTER(self, x, y) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'cluster_{x}_{y}', signature='io')

class Testbench(gvsoc.systree.Component):

    def __init__(self, parent, name, use_memory=True, target_bw=0):
        super().__init__(parent, name)

        nb_cluster_x = 3
        nb_cluster_y = 3
        cluster_base = 0x80000000
        cluster_size = 0x01000000
        mem_base = 0x9000_0000
        mem_size = 0x10_0000
        mem_group_size = 0x1000_0000

        noc = pulp.floonoc.floonoc.FlooNocClusterGridNarrowWide(self, 'noc', 64, 8, nb_cluster_x, nb_cluster_y)

        test = FloonocTest(self, 'test', nb_cluster_x, nb_cluster_y, cluster_base, cluster_size)

        for x in range(0, nb_cluster_x):
            for y in range(0, nb_cluster_y):
                generator_w = interco.traffic.generator.Generator(self, f'generator_{x}_{y}_w')
                generator_w.o_OUTPUT(noc.i_CLUSTER_WIDE_INPUT(x, y))
                generator_n = interco.traffic.generator.Generator(self, f'generator_{x}_{y}_n')
                generator_n.o_OUTPUT(noc.i_CLUSTER_NARROW_INPUT(x, y))

                receiver_w = Receiver(self, f'receiver_{x}_{y}_w', mem_size=1<<20)
                receiver_n = Receiver(self, f'receiver_{x}_{y}_n', mem_size=1<<20)

                test.o_NOC_NI(x, y, noc.i_CLUSTER_WIDE_INPUT(x, y))
                test.o_GENERATOR_CONTROL(x, y, True, generator_w.i_CONTROL())
                test.o_GENERATOR_CONTROL(x, y, False, generator_n.i_CONTROL())
                test.o_RECEIVER_CONTROL(x, y, True, receiver_w.i_CONTROL())
                test.o_RECEIVER_CONTROL(x, y, False, receiver_n.i_CONTROL())

                noc.o_MAP(cluster_base + cluster_size * (y * nb_cluster_x + x),
                    cluster_size, x+1, y+1, rm_base=True)
                noc.o_WIDE_BIND(receiver_w.i_INPUT(), x+1, y+1)
                noc.o_NARROW_BIND(receiver_n.i_INPUT(), x+1, y+1)

        mem_group_base = mem_base
        # Add targets on the borders
        for i in range(0, 4):
            target_mem_base = mem_group_base

            nb_targets = nb_cluster_x if i < 2 else nb_cluster_y

            bound_name = ['up', 'down', 'left', 'right'][i]

            for x in range(0, nb_targets):
                coord = [(x+1, 0), (x+1, nb_cluster_y+1), (0, x+1), (nb_cluster_x+1, x+1)][i]

                if use_memory:
                    mem = Memory(self, f'mem_{bound_name}_{x}', size=mem_size,
                        width_log2=-1 if target_bw == 0 else math.log2(target_bw))
                else:
                    mem = Receiver(self, f'rcv_{bound_name}_{x}')
                    # test.o_RECEIVER_CONTROL(i, mem.i_CONTROL())
                if bound_name != 'up':
                    noc.o_MAP(target_mem_base, mem_size, coord[0], coord[1],
                        rm_base=True)
                noc.o_WIDE_BIND(mem.i_INPUT(), coord[0], coord[1])
                target_mem_base += mem_size

            if bound_name == 'up':
                noc.o_MAP_DIR(mem_group_base, mem_group_size, FlooNocDirection.UP,
                    name=f'mem_up', rm_base=True)
            mem_group_base += mem_group_size

# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Chip(gvsoc.systree.Component):

    def __init__(self, parent, name=None):

        super().__init__(parent, name)

        use_memory = TargetParameter(
            self, name='use_memory', value=True, description='Use memory as targets on the corner. Otherwise it will use traffic receiver',
            cast=bool
        ).get_value()

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)
        soc = Testbench(self, 'soc', use_memory=use_memory)
        clock.o_CLOCK    (soc.i_CLOCK    ())




# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    gapy_description = "Floonoc test"
    model = Chip
    name = "test"
