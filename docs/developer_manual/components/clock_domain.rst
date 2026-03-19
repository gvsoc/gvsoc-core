.. _component_clock_domain:

Clock_domain
============


**Python module:** ``vp.clock_domain``

**Source:** ``core/models/vp/clock_domain.py``


Description
-----------


Clock domai

This model can be used to define a clock domain.

This instantiates a clock generator which can then be connected to components which are part of the clock domain.

The clock domain starts with the specified frequency and its frequency can then be dynamically changed through a dedicated interface, so that all components of the frequency domai are clocked with the new frequency.

Properties
----------


.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Name
     - Type
     - Default
     - Description
   * - ``frequency``
     - int
     - ``required``
     - The initial frequency of the clock generator.
   * - ``factor``
     - int
     - ``1``
     - Multiplication factor. The actual output frequency will be multiplied by this factor. This can be used for example to be able to schedule events on both raising and falling edges.

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
   * - ``i_CTRL()``
     - ``clock_ctrl``
     - Returns the port for controlling the clock generator. This can be used to dynamically change the frequency. It instantiates a port of type vp::ClockMaster.

Output ports (master)
~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Method
     - Signature
     - Description
   * - ``o_CLOCK()``
     - ``clock``
     - Binds the output clock port. This port can be connected to any component which should be clocked by this clock generator. Several components can be bound on it. In case the frequency is dynamically modified, all bound components are notified and will  be automatically using the new frequency. It instantiates a port of type vp::ClkMaster.

.. note::

   All components also inherit the following ports from ``Component``:

   - ``i_CLOCK()`` — clock input (signature: ``clock``)
   - ``i_RESET()`` — reset input (signature: ``wire<bool>``)
   - ``i_POWER()`` — power supply control (signature: ``wire<int>``)
   - ``i_VOLTAGE()`` — voltage control (signature: ``wire<int>``)

Usage example
-------------


.. code-block:: python

   import vp

   # Instantiate in a parent component
   comp = clock_domain.Clock_domain(parent, 'name', frequency=0x1000)

   # Connect ports
   # master.o_XXX(comp.i_CTRL())
   # comp.o_CLOCK(slave.i_XXX())
