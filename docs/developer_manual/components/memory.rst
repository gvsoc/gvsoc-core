.. _component_memory:

Memory
======


**Python module:** ``memory.memory``

**Source:** ``core/models/memory/memory.py``


Description
-----------


Memory array

This models a simple memory model. It can be preloaded with initial data. It contains a timing model of a bandwidth, reported through latency. It can support riscv atomics.

Properties
----------


.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Name
     - Type
     - Default
     - Description
   * - ``size``
     - int
     - ``0``
     - The size of the memory in bytes.
   * - ``width_log2``
     - int
     - ``-1``
     - The log2 of the bandwidth to the memory, i.e. the number of bytes it can transfer per cycle. No timing model is applied if it is -1 and the memory is then having an infinite bandwidth.
   * - ``stim_file``
     - str
     - ``None``
     - The path to a binary file which should be preloaded at beginning of the memory. The format is a raw binary, and is loaded with an fread.
   * - ``power_trigger``
     - bool
     - ``False``
     - True if the memory should trigger power report generation based on dedicated accesses.
   * - ``align``
     - int
     - ``0``
     - Specify a required alignment for the allocated memory used for the memory model.
   * - ``atomics``
     - bool
     - ``False``
     - True if the memory should support riscv atomics. Since this is slowing down the model, it should be set to True only if needed.
   * - ``latency``
     - 
     - ``0``
     - Specify extra latency which will be added to any incoming request.
   * - ``memcheck_id``
     - int
     - ``-1``
     - If this memory is used to track buffer overflow, this gives the global memory check id.
   * - ``memcheck_base``
     - int
     - ``0``
     - Absolute base of memory where buffer overflow is tracked.
   * - ``memcheck_virtual_base``
     - int
     - ``0``
     - Absolute virtual base of allocated buffers.
   * - ``memcheck_expansion_factor``
     - int
     - ``5``
     - Extra size used to track buffer overflow.
   * - ``init``
     - 
     - ``True``
     - 

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
     - Returns the input port. Incoming requests to be handled by the memory should be sent to this port. It instantiates a port of type vp::IoSlave.

.. note::

   All components also inherit the following ports from ``Component``:

   - ``i_CLOCK()`` — clock input (signature: ``clock``)
   - ``i_RESET()`` — reset input (signature: ``wire<bool>``)
   - ``i_POWER()`` — power supply control (signature: ``wire<int>``)
   - ``i_VOLTAGE()`` — voltage control (signature: ``wire<int>``)

Usage example
-------------


.. code-block:: python

   import memory

   # Instantiate in a parent component
   comp = memory.Memory(parent, 'name', size=0)

   # Connect ports
   # master.o_XXX(comp.i_INPUT())
