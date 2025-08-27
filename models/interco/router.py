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

class Router(gvsoc.systree.Component):
    """Interconnect router

    This models an AXI-like router.
    It is capable of routing memory-mapped requests from an input port to output ports based on
    the request address.
    It has several input ports, one for each set of independent initiators, and several output
    ports, one for each target memory area.
    In terms of timing behavior, the router makes sure the flow of requests going throught it
    respects a certain bandwidth and can also apply a latency to the requests.
    This models a full crossbar, one input port can always send to an output port without being
    disturbed by other paths.
    Read and write bursts can also go through the router in parallel.
    The timing behavior can be specialized by tuning the 3 following properties:
    - The global latency. This applies the same latency to each requests injected into the router.
    - The entry latency. This applies the latency only to requests going through the output port
      to which the latency is associated.
    - The bandwidth. The router will use it to delay requests so that the amount of bytes going
      through an output repects the global bandwidth.

    The router has 2 main modes, either synchronous or asynchronous.

    The synchronous mode can be used when the simulation speed should be privileged.
    In this case the router forwards the requests as fast as possible in a synchronous way so that
    everything is handled within a function call.
    There are some rare cases where the requests will be handled asynchronously:
    - The slave where the request is forwarded handled it asynchronously
    - The request is spread accross several entries and of the slave where part of the request
      was forwarded handled the request asynchronously.
    In this mode, the router lets the requests go through it as they arrive and use the request
    latency to respect the bandwidth. If we are already above the bandwidth, the latency of the
    request, which is the starting point of the burst, is delayed to the cycle where we get under
    the bandwidth. The bandwidth is also used to set the duration of the burst.
    The bandwidth is applied first on input ports to model the fact that requests coming from same
    port are serialized, and applied a second time on output port, for the same reason.
    The drawback of this synchronous router is that any incoming request is immediately forwarded
    to an output port. This means several requests can be routed in the same cycle, even to the same
    output port. When this happens, the next slave can handle a big amount of data in the same cycle,
    which means it can block activity for some time, creating some artificial holes in the execution.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    latency: int
        Global latency applied to all incoming requests. This impacts the start time of the burst.
    bandwidth: int
        Global bandwidth, in bytes per cycle, applied to all incoming request. This impacts the
        end time of the burst.
    synchronous: True if the router should use synchronous mode where all incoming requests are
        handled as far as possible in synchronous IO mode.
    shared_rw_bandwidth: True if the read and write requests should share the bandwidth.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, latency: int=0, bandwidth: int=0,
            synchronous: bool=True, shared_rw_bandwidth: bool=False):
        super(Router, self).__init__(parent, name)

        # This will store the whole set of mappings and passed to model as a dictionary
        self.add_property('mappings', {})
        self.add_property('latency', latency)
        self.add_property('bandwidth', bandwidth)
        self.add_property('shared_rw_bandwidth', shared_rw_bandwidth)
        # The number of input port is automatically increased each time i_INPUT is called if needed.
        # Set number of input ports to 1 by default because some models do not use i_INPUT yet.
        self.add_property('nb_input_port', 1)

        self.add_sources(['interco/router_common.cpp'])
        if synchronous:
            self.add_sources(['interco/router.cpp'])
        else:
            self.add_sources(['interco/router_async.cpp'])


    def add_mapping(self, name: str, base: int=0, size: int=0, remove_offset: int=0,
            add_offset: int=0, latency: int=0):
        """Add a target port with an associated target memory map.

        The port is created with the specified name, so that the same name can be used to connect
        the router to the target for this mapping. Any incoming request whose address is inside this
        memory map is forwarded to thsi port.

        On top of the global latency, a latency specific to this mapping can be added when a request
        goes through this mapping.

        Deprecated:
            Use `o_MAP` instead.

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
        add_offset: int
            This address is added to the address of any request going through this mapping.
            This can be used to remap a request to a different network, like in a bridge.
        latency: int
            Latency applied to any request going through this mapping. This impacts the start time
            of the request.
        """

        self.get_property('mappings')[name] =  {
            'base': base,
            'size': size,
            'remove_offset': remove_offset,
            'add_offset': add_offset,
            'latency': latency,
        }

    def __alloc_input_port(self, id):
        nb_input_port = self.get_property("nb_input_port")
        if id >= nb_input_port:
            self.add_property("nb_input_port", id + 1)

    def i_INPUT(self, id: int=0) -> gvsoc.systree.SlaveItf:
        """Returns the input port.

        Since the model can handle several input ports, the identifier of the input port
        can be specified.
        Incoming requests to be routed can be sent to the port.\n
        The router will forward them to the correct output port depending on the request
        address.\n
        It instantiates a port of type vp::IoSlave.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        self.__alloc_input_port(id)
        if id == 0:
            return gvsoc.systree.SlaveItf(self, f'input', signature='io')
        else:
            return gvsoc.systree.SlaveItf(self, f'input_{id}', signature='io')

    def o_MAP(self, itf: gvsoc.systree.SlaveItf, name: str=None, base: int=0, size: int=0,
            rm_base: bool=True, remove_offset: int=0, latency: int=0):
        """Binds the output to a memory region.

        This ports can be used to attach a memory region to the specified slave interface.\n
        Any requests whose base address is inside the memory region is forwarded to the
        associated port.\n
        It instantiates a port of type vp::IoMaster.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface where requests matching the mapping must be forwarded.
        name: str
            Name of the mapping, which is used for connecting to output port interface. Specifying
            the name is normally not needed, but might be in case several mappings are connected
            to same target.
        base: int
            Base address of the target memory area.
        size: int
            Size of the target memory area.
        rm_base: bool
            if True, the base address is substracted to the address of any request going through
            this mapping. This can be used to convert an address into a local offset.
        remove_offset: int
            This address is substracted to the address of any request going through this mapping.
            This can be used to convert an address into a local offset.
        latency: int
            Latency applied to any request going through this mapping. This impacts the start time
            of the request.
        """
        # We remove the base if specified, but only if remove_offset is not already specified as
        # they are redundant
        if rm_base and remove_offset == 0:
            remove_offset = base
        # Normal case when name is not specified, we take the target component name
        if name is None:
            name = itf.component.name
        self.add_mapping(name, base=base, remove_offset=remove_offset, size=size, latency=latency)
        self.itf_bind(name, itf, signature='io')
