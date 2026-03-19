.. _component_router:

Router
======


**Python module:** ``interco.router``

**Source:** ``core/models/interco/router.py``


Description
-----------


Interconnect router

This models an AXI-like router. It is capable of routing memory-mapped requests from an input port to output ports based o the request address. It has several input ports, one for each set of independent initiators, and several output ports, one for each target memory area. In terms of timing behavior, the router makes sure the flow of requests going throught it respects a certain bandwidth and can also apply a latency to the requests. This models a full crossbar, one input port can always send to an output port without being disturbed by other paths. Read and write bursts can also go through the router in parallel. The timing behavior can be specialized by tuning the 3 following properties: - The global latency. This applies the same latency to each requests injected into the router. - The entry latency. This applies the latency only to requests going through the output port to which the latency is associated. - The bandwidth. The router will use it to delay requests so that the amount of bytes going through an output repects the global bandwidth.

The router has 2 main modes, either synchronous or asynchronous.

The synchronous mode can be used when the simulation speed should be privileged. In this case the router forwards the requests as fast as possible in a synchronous way so that everything is handled within a function call. There are some rare cases where the requests will be handled asynchronously: - The slave where the request is forwarded handled it asynchronously - The request is spread accross several entries and of the slave where part of the request was forwarded handled the request asynchronously. In this mode, the router lets the requests go through it as they arrive and use the request latency to respect the bandwidth. If we are already above the bandwidth, the latency of the request, which is the starting point of the burst, is delayed to the cycle where we get under the bandwidth. The bandwidth is also used to set the duration of the burst. The bandwidth is applied first on input ports to model the fact that requests coming from same port are serialized, and applied a second time on output port, for the same reason. The drawback of this synchronous router is that any incoming request is immediately forwarded to an output port. This means several requests can be routed in the same cycle, even to the same output port. When this happens, the next slave can handle a big amount of data in the same cycle, which means it can block activity for some time, creating some artificial holes in the execution.

The asynchronous mode can be used for more accurate simulation. In this case, the router always enqueue incoming requests into queues and use events to arbiter the requests to be forwarded to the outputs. This router always add one cycle of latency as it forwards the requests the cycle after at best. The arbitration happens in 2 steps. First it executes an event at the end of the cycles to elect which input requests go to which output. Requests arrived in the same cycle are taken into account. Then it executes another event the cycle after to send the elected requests. The router maintain a bandwidth limitation at both the input and the output sides. If specified, it can also model a FIFO on each input to limit the number of pending bytes. I this case, as soon as the limit is reached, any other incoming request on the port where the FIFO is full is denied. This can only be used for cycle-base requests, where the size of the requests are smaller or equal to the router bandwidth. The router latency is not yet implemented in this mode.

Properties
----------


.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Name
     - Type
     - Default
     - Description
   * - ``latency``
     - int
     - ``0``
     - Global latency applied to all incoming requests. This impacts the start time of the burst.
   * - ``bandwidth``
     - int
     - ``0``
     - Global bandwidth, in bytes per cycle, applied to all incoming request. This impacts the end time of the burst.
   * - ``synchronous``
     - bool
     - ``True``
     - handled as far as possible in synchronous IO mode.
   * - ``shared_rw_bandwidth``
     - bool
     - ``False``
     - 
   * - ``max_input_pending_size``
     - 
     - ``0``
     - only when input packet size is smaller or equal to the bandwidth.

Ports
-----


Input ports (slave)
~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Method
     - Signature
     - Description
   * - ``i_INPUT()``
     - ``io``
     - Returns the input port. Since the model can handle several input ports, the identifier of the input port can be specified. Incoming requests to be routed can be sent to the port. The router will forward them to the correct output port depending on the request address. It instantiates a port of type vp::IoSlave. — ``id=0``

Output ports (master)
~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Method
     - Signature
     - Description
   * - ``o_MAP()``
     - ``io``
     - Binds the output to a memory region. This ports can be used to attach a memory region to the specified slave interface. Any requests whose base address is inside the memory region is forwarded to the associated port. It instantiates a port of type vp::IoMaster.

.. note::

   All components also inherit the following ports from ``Component``:

   - ``i_CLOCK()`` — clock input (signature: ``clock``)
   - ``i_RESET()`` — reset input (signature: ``wire<bool>``)
   - ``i_POWER()`` — power supply control (signature: ``wire<int>``)
   - ``i_VOLTAGE()`` — voltage control (signature: ``wire<int>``)

Usage example
-------------


.. code-block:: python

   import interco

   # Instantiate in a parent component
   comp = router.Router(parent, 'name', bandwidth=0)

   # Connect ports
   # master.o_XXX(comp.i_INPUT())
   # comp.o_MAP(slave.i_XXX())
