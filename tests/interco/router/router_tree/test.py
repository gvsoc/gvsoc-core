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
from interco.traffic.receiver import Receiver
from memory.memory import Memory
from gvrun.target import TargetParameter

import vp.clock_domain
import math


class RouterTest(gvsoc.systree.Component):

    def __init__(self, parent, name, access_type, shared_rw, bandwidth_0, bandwidth_1, packet_size, input_fifo_size, use_memory,
            target_bw, nb_router_in=1, nb_router_out=1,
        nb_gen_per_router_in=1):
        super().__init__(parent, name)

        self.add_sources(['test.cpp'])
        self.add_sources(['test0.cpp'])

        self.add_property('target_bw', target_bw)
        self.add_property('bandwidth_0', bandwidth_0)
        self.add_property('bandwidth_1', bandwidth_1)
        self.add_property('packet_size', packet_size)
        self.add_property('input_fifo_size', input_fifo_size)
        self.add_property('nb_router_in', nb_router_in)
        self.add_property('nb_router_out', nb_router_out)
        self.add_property('nb_gen_per_router_in', nb_gen_per_router_in)
        self.add_property('access_type', access_type)
        self.add_property('shared_rw', shared_rw)
        self.add_property('use_memory', use_memory)

    def o_GENERATOR_CONTROL(self, x, y, z, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'generator_control_{x}_{y}_{z}', itf, signature='wire<TrafficGeneratorConfig>')

    def o_RECEIVER_CONTROL(self, x, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'receiver_control_{x}', itf, signature='wire<TrafficReceiverConfig>')

class Testbench(gvsoc.systree.Component):

    def __init__(self, parent, name, access_type, shared_rw, synchronous, bandwidth_0, bandwidth_1, packet_size,
            input_fifo_size, use_memory, target_bw):
        super().__init__(parent, name)

        nb_router_in = 4
        nb_router_out = 4
        nb_gen_per_router_in = 2

        test = RouterTest(self, 'test', nb_router_in=nb_router_in, nb_router_out=nb_router_out,
            nb_gen_per_router_in=nb_gen_per_router_in, access_type=access_type, shared_rw=shared_rw,
            bandwidth_0=bandwidth_0, bandwidth_1=bandwidth_1, packet_size=packet_size, input_fifo_size=input_fifo_size,
            use_memory=use_memory, target_bw=target_bw)

        router = Router(self, 'router', bandwidth=bandwidth_0, synchronous=synchronous,
            shared_rw_bandwidth=shared_rw, max_input_pending_size=input_fifo_size)

        router_0 = Router(self, 'router_0', bandwidth=bandwidth_1, synchronous=synchronous,
            shared_rw_bandwidth=shared_rw, max_input_pending_size=input_fifo_size)

        router_1 = Router(self, 'router_1', bandwidth=bandwidth_1, synchronous=synchronous,
            shared_rw_bandwidth=shared_rw, max_input_pending_size=input_fifo_size)

        router.o_MAP(router_0.i_INPUT(), base=0x1000_0000, size=nb_router_out / 2 * 0x10_0000, rm_base=False)
        router.o_MAP(router_1.i_INPUT(), base=0x1000_0000 + nb_router_out / 2 * 0x10_0000, size=nb_router_out / 2 * 0x10_0000, rm_base=False)


        for i in range(0, nb_router_out):
            if use_memory:
                mem = Memory(self, f'mem_{i}', size=0x10_0000, width_log2=-1 if target_bw == 0 else math.log2(target_bw))
            else:
                mem = Receiver(self, f'rcv_{i}')
                test.o_RECEIVER_CONTROL(i, mem.i_CONTROL())

            if i < nb_router_out / 2:
                router_0.o_MAP(mem.i_INPUT(), base=0x1000_0000 + i * 0x10_0000, size=0x10_0000)
            else:
                router_1.o_MAP(mem.i_INPUT(), base=0x1000_0000 + i * 0x10_0000, size=0x10_0000)

        for i in range(0, nb_router_in):
            for j in range(0, nb_gen_per_router_in):
                for k in range(0, 2):
                    gen = Generator(self, f'gen_{i}_{j}_{k}', nb_pending_reqs=8)
                    test.o_GENERATOR_CONTROL(i, j, k, gen.i_CONTROL())
                    gen.o_OUTPUT(router.i_INPUT(i))


# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Chip(gvsoc.systree.Component):

    def __init__(self, parent, name=None):

        super().__init__(parent, name)

        use_memory = TargetParameter(
            self, name='use_memory', value=True, description='Use memory as targets. Otherwise it will use traffic receiver',
            cast=bool
        ).get_value()

        target_bw = TargetParameter(
            self, name='target_bw', value=0, description='Target bandwidth',
            cast=int
        ).get_value()

        synchronous = TargetParameter(
            self, name='synchronous', value=True, description='Use synchronous router',
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

        bandwidth_0 = TargetParameter(
            self, name='bandwidth_0', value=4, description='Router bandwidth',
            cast=int
        ).get_value()

        bandwidth_1 = TargetParameter(
            self, name='bandwidth_1', value=4, description='Router bandwidth',
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
            bandwidth_0=bandwidth_0, bandwidth_1=bandwidth_1, packet_size=packet_size, input_fifo_size=input_fifo_size, use_memory=use_memory,
            target_bw=target_bw)
        clock.o_CLOCK    (soc.i_CLOCK    ())




# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    gapy_description = "Router test"
    model = Chip
    name = "test"
