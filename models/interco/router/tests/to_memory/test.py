#
# Copyright (C) 2025 ETH Zurich and University of Bologna
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
from interco.router import Router
from interco.traffic.generator import Generator
from memory.memory import Memory
from gvrun.target import TargetParameter

import vp.clock_domain


class RouterTest(gvsoc.systree.Component):

    def __init__(self, parent, name, access_type, shared_rw, bandwidth, packet_size, input_fifo_size,
            nb_router_in=1, nb_router_out=1,
        nb_gen_per_router_in=1):
        super().__init__(parent, name)

        self.add_sources(['test.cpp'])
        self.add_sources(['test0.cpp'])
        self.add_sources(['test1.cpp'])
        self.add_sources(['test2.cpp'])
        self.add_sources(['test3.cpp'])
        self.add_sources(['test4.cpp'])
        self.add_sources(['test5.cpp'])

        self.add_property('bandwidth', bandwidth)
        self.add_property('packet_size', packet_size)
        self.add_property('input_fifo_size', input_fifo_size)
        self.add_property('nb_router_in', nb_router_in)
        self.add_property('nb_router_out', nb_router_out)
        self.add_property('nb_gen_per_router_in', nb_gen_per_router_in)
        self.add_property('access_type', access_type)
        self.add_property('shared_rw', shared_rw)

    def o_GENERATOR_CONTROL(self, x, y, z, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'generator_control_{x}_{y}_{z}', itf, signature='wire<TrafficGeneratorConfig>')

class Testbench(gvsoc.systree.Component):

    def __init__(self, parent, name, access_type, shared_rw, synchronous, bandwidth, packet_size, input_fifo_size):
        super().__init__(parent, name)

        nb_router_in = 2
        nb_router_out = 2
        nb_gen_per_router_in = 2

        test = RouterTest(self, 'test', nb_router_in=nb_router_in, nb_router_out=nb_router_out,
            nb_gen_per_router_in=nb_gen_per_router_in, access_type=access_type, shared_rw=shared_rw,
            bandwidth=bandwidth, packet_size=packet_size, input_fifo_size=input_fifo_size)
        router = Router(self, 'router', bandwidth=bandwidth, synchronous=synchronous,
            shared_rw_bandwidth=shared_rw, max_input_pending_size=input_fifo_size)

        for i in range(0, nb_router_out):
            mem = Memory(self, f'mem_{i}', size=0x10_0000)
            router.o_MAP(mem.i_INPUT(), base=0x1000_0000 + i * 0x10_0000, size=0x10_0000)

        for i in range(0, nb_router_in):
            for j in range(0, nb_gen_per_router_in):
                for k in range(0, 2):
                    gen = Generator(self, f'gen_{i}_{j}_{k}')
                    test.o_GENERATOR_CONTROL(i, j, k, gen.i_CONTROL())
                    gen.o_OUTPUT(router.i_INPUT(i))


# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Chip(gvsoc.systree.Component):

    def __init__(self, parent, name=None):

        super().__init__(parent, name)

        synchronous = TargetParameter(
            self, name='synchronous', value=False, description='Use synchronous router',
            cast=bool
        ).get_value()

        access_type = TargetParameter(
            self, name='access_type', value="r", description='Types of accesses to be done',
            allowed_values=['r', 'w', 'rw'], cast=str
        ).get_value()

        shared_rw = TargetParameter(
            self, name='shared_rw', value=False, description='Bandwidth is shared between read and write',
            cast=bool
        ).get_value()

        bandwidth = TargetParameter(
            self, name='bandwidth', value=4, description='Router bandwidth',
            cast=int
        ).get_value()

        packet_size = TargetParameter(
            self, name='packet_size', value=4, description='Traffic generator packet size',
            cast=int
        ).get_value()

        input_fifo_size = TargetParameter(
            self, name='input_fifo_size', value=4, description='Router input fifo size',
            cast=int
        ).get_value()

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)
        soc = Testbench(self, 'soc', access_type=access_type, shared_rw=shared_rw, synchronous=synchronous,
            bandwidth=bandwidth, packet_size=packet_size, input_fifo_size=input_fifo_size)
        clock.o_CLOCK    (soc.i_CLOCK    ())




# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    gapy_description = "Router test"
    model = Chip
    name = "test"
