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

import gsystree

class Router(gsystree.Component):
    """Interconnect router

    This models a simple interconnect.
    It has a single input port and several output ports, one for each target memory area.
    It routes incoming requests to the right output ports, depending on the address of
    the requests.
    It can apply a latency to each, either global, or depending on the output port.
    It can also apply a global bandwidth, which impacts the duration of the request.
    The timing model is based on bursts. The latency can delay the starting point of the burst,
    while the bandwidth impacts the end point of the burst.

    Attributes
    ----------
    parent: gsystree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    latency: int
        Global latency applied to all incoming requests. This impacts the start time of the burst.
    bandwidth: int
        Global bandwidth, in bytes per cycle, applied to all incoming request. This impacts the
        end time of the burst.
    """
    def __init__(self, parent: gsystree.Component, name: str, latency: int=0, bandwidth: int=0):
        super(Router, self).__init__(parent, name)

        self.set_component('interco.router_impl')

        self.add_property('mappings', {})
        self.add_property('latency', latency)
        self.add_property('bandwidth', bandwidth)

    def add_mapping(self, name: str, base: int=None, size: int=None, remove_offset: int=None,
            add_offset: int=None, id: int=None, latency: int=None):
        """Add a target port with an associated target memory map.

        The port is created with the specified name, so that the same name can be used to connect
        the router to the target for this mapping. Any incoming request whose address is inside this
        memory map is forwarded to thsi port.

        On top of the global latency, a latency specific to this mapping can be added when a request
        goes through this mapping.

        Parameters
        ----------
        name: str
            Name of the mapping. An interface of the same name will be created, and so a binding
            with the same name for the master can be created.
        base: int
            Base address of the target memory area.
        size: int
            Size of the target memory area.
        remove_offset: int
            This address is substracted to the address of any request going through this mapping.
            This can be used to convert an address into a local offset.
        id: int
            Counter id where this mapping is reporting statistics. All mappings with same id
            are cumulated together, which is a way to collect statistics fro several mappings.
        latency: int
            Latency applied to any request going through this mapping. This impacts the start time
            of the request.
        """

        mapping = {}

        if base is not None:
            mapping['base'] = base

        if size is not None:
            mapping['size'] = size

        if remove_offset is not None:
            mapping['remove_offset'] = remove_offset

        if add_offset is not None:
            mapping['add_offset'] = add_offset

        if latency is not None:
            mapping['latency'] = latency

        if id is not None:
            mapping['id'] = id

        self.get_property('mappings')[name] =  mapping

    def i_INPUT(self) -> gsystree.SlaveItf:
        """Returns the input port.

        Incoming requests to be routed can be sent to the port.\n
        The router will forward them to the correct output port depending on the request
        address.\n
        It instantiates a port of type vp::IoSlave.\n

        Returns
        ----------
        gsystree.SlaveItf
            The slave interface
        """
        return gsystree.SlaveItf(self, 'input', signature='io')

    def o_MAP(self, itf: gsystree.SlaveItf, name:str, base: int=None, size: int=None,
            rm_base: bool=True, remove_offset: int=None, id: int=None, latency: int=None):
        """Binds the output to a memory region.

        This ports can be used to attach a memory region to the specified slave interface.\n
        Any requests whose base address is inside the memory region is forwarded to the
        associated port.\n
        Requests should not cross several regions. In this case, they are forwarded to the
        region containing the base address of the request.
        It instantiates a port of type vp::IoMaster.\n

        Parameters
        ----------
        slave: gsystree.SlaveItf
            Slave interface
        name: str
            Name of the mapping. An interface of the same name will be created.
        base: int
            Base address of the target memory area.
        size: int
            Size of the target memory area.
        rm_base: int
            The base address is substracted to the address of any request going through this mapping.
            This can be used to convert an address into a local offset.
        remove_offset: int
            This address is substracted to the address of any request going through this mapping.
            This can be used to convert an address into a local offset.
        id: int
            Counter id where this mapping is reporting statistics. All mappings with same id
            are cumulated together, which is a way to collect statistics fro several mappings.
        latency: int
            Latency applied to any request going through this mapping. This impacts the start time
            of the request.
        """
        if rm_base and remove_offset is None:
            remove_offset = base
        self.add_mapping(name, base=base, remove_offset=remove_offset, size=size, id=id, latency=latency)
        self.itf_bind(name, itf, signature='io')
