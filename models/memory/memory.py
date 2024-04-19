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

class Memory(gvsoc.systree.Component):
    """Memory array

    This models a simple memory model.
    It can be preloaded with initial data.
    It contains a timing model of a bandwidth, reported through latency.
    It can support riscv atomics.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    size: int
        The size of the memory in bytes.
    width_log2: int
        The log2 of the bandwidth to the memory, i.e. the number of bytes it can transfer per cycle.
        No timing model is applied if it is zero and the memory is then having an infinite
        bandwidth.
    stim_file: str
        The path to a binary file which should be preloaded at beginning of the memory. The format
        is a raw binary, and is loaded with an fread.
    power_trigger: bool
        True if the memory should trigger power report generation based on dedicated accesses.
    align: int
        Specify a required alignment for the allocated memory used for the memory model.
    atomics: bool
        True if the memory should support riscv atomics. Since this is slowing down the model, it
        should be set to True only if needed.
    latency: int
        Specify extra latency which will be added to any incoming request.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, size: int, width_log2: int=2,
            stim_file: str=None, power_trigger: bool=False,
            align: int=0, atomics: bool=False, latency=0):

        super().__init__(parent, name)

        self.add_sources(['memory/memory.cpp'])

        # Since atomics are slowing down the model, this is better to compile the support only
        # if needed. Note that the framework will take care of compiling this model twice
        # if both memories with and without atomics are instantiated.
        if atomics:
            self.add_c_flags(['-DCONFIG_ATOMICS=1'])

        self.add_properties({
            'size': size,
            'stim_file': stim_file,
            'power_trigger': power_trigger,
            'width_bits': width_log2,
            'align': align,
            'latency': latency
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        """Returns the input port.

        Incoming requests to be handled by the memory should be sent to this port.\n
        It instantiates a port of type vp::IoSlave.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')
