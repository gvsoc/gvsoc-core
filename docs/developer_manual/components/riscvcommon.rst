.. _component_riscvcommon:

RiscvCommon
===========


**Python module:** ``cpu.iss.riscv``

**Source:** ``core/models/cpu/iss/riscv.py``


Description
-----------


Riscv instruction set simulator

Properties
----------


.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Name
     - Type
     - Default
     - Description
   * - ``isa``
     - 
     - ``required``
     - A string describing the list of ISA groups that the ISS should simulate (default: "rv32imfc").
   * - ``misa``
     - int
     - ``0``
     - The initial value of the MISA CSR (default: 0).
   * - ``first_external_pcer``
     - int
     - ``0``
     - The index of the first PCER which is retrieved externally (default: 0).
   * - ``riscv_dbg_unit``
     - bool
     - ``False``
     - True if a riscv debug unit should be included, False otherwise (default: False).
   * - ``debug_binaries``
     - list
     - ``[]``
     - A list of path to riscv binaries debug info which can be used to get debug symbols for the assembly trace (default: []).
   * - ``binaries``
     - list
     - ``[]``
     - A list of path to riscv binaries (default: []).
   * - ``debug_handler``
     - int
     - ``0``
     - The address where the core should jump when switching to debug mode (default: 0).
   * - ``power_models``
     - dict
     - ``{}``
     - A dictionnay describing all the power models used to estimate power consumption in the ISS (default: {})
   * - ``power_models_file``
     - str
     - ``None``
     - A path to a file describing all the power models used to estimate power consumption in the ISS (default: None)
   * - ``cluster_id``
     - int
     - ``0``
     - The cluster ID of the core simulated by the ISS (default: 0).
   * - ``core_id``
     - int
     - ``0``
     - The core ID of the core simulated by the ISS (default: 0).
   * - ``fetch_enable``
     - bool
     - ``False``
     - True if the ISS should start executing instructins immediately, False if it will start after the fetch_enable signal starts it (default: False).
   * - ``boot_addr``
     - int
     - ``0``
     - Address of the first instruction (default: 0)
   * - ``mmu``
     - bool
     - ``False``
     - 
   * - ``pmp``
     - bool
     - ``False``
     - 
   * - ``riscv_exceptions``
     - bool
     - ``False``
     - 
   * - ``core``
     - 
     - ``'riscv'``
     - 
   * - ``supervisor``
     - 
     - ``False``
     - 
   * - ``user``
     - 
     - ``False``
     - 
   * - ``internal_atomics``
     - 
     - ``False``
     - 
   * - ``timed``
     - 
     - ``True``
     - 
   * - ``scoreboard``
     - 
     - ``False``
     - 
   * - ``cflags``
     - 
     - ``None``
     - 
   * - ``prefetcher_size``
     - 
     - ``None``
     - 
   * - ``wrapper``
     - 
     - ``'pulp/cpu/iss/default_iss_wrapper.cpp'``
     - 
   * - ``memory_start``
     - 
     - ``None``
     - 
   * - ``memory_size``
     - 
     - ``None``
     - 
   * - ``handle_misaligned``
     - 
     - ``False``
     - 
   * - ``external_pccr``
     - 
     - ``False``
     - 
   * - ``htif``
     - 
     - ``False``
     - 
   * - ``custom_sources``
     - 
     - ``False``
     - 
   * - ``float_lib``
     - 
     - ``'flexfloat'``
     - 
   * - ``stack_checker``
     - 
     - ``False``
     - 
   * - ``nb_outstanding``
     - 
     - ``1``
     - 
   * - ``single_regfile``
     - bool
     - ``False``
     - 
   * - ``zfinx``
     - bool
     - ``False``
     - 
   * - ``zdinx``
     - bool
     - ``False``
     - 
   * - ``fp_width``
     - Union
     - ``None``
     - 
   * - ``modules``
     - list
     - ``[]``
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
   * - ``i_ENTRY()``
     - ``wire<uint64_t>``
     - Returns the boot address port. This can be used to set the address of the first instruction to be executed, i.e. when the core executes instructions for the first time, or after reset. It instantiates a port of type vp::WireSlave<uint64_t>.
   * - ``i_FETCHEN()``
     - ``wire<bool>``
     - Returns the fetch enable port. This can be used to control whether the core should execute instructions or not. It instantiates a port of type vp::WireSlave<bool>.
   * - ``i_FLUSH_CACHE_ACK()``
     - ``wire<bool>``
     - 
   * - ``i_IRQ()``
     - ``wire<bool>``
     - 
   * - ``i_IRQ_REQ()``
     - ``wire<int>``
     - 
   * - ``i_OFFLOAD_GRANT()``
     - ``wire<IssOffloadInsnGrant<uint*_t>*>``
     - 

Output ports (master)
~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Method
     - Signature
     - Description
   * - ``o_DATA()``
     - ``io``
     - Binds the data port. This port is used for issuing data accesses to the memory. It instantiates a port of type vp::IoMaster. It is mandatory to bind it.
   * - ``o_DATA_DEBUG()``
     - ``io``
     - Binds the data debug port. This port is used for issuing data accesses from gdb server to the memory. It instantiates a port of type vp::IoMaster. If gdbserver is used It is mandatory to bind it.
   * - ``o_FETCH()``
     - ``io``
     - Binds the fetch port. This port is used for fetching instructions from memory. It instantiates a port of type vp::IoMaster. It is mandatory to bind it.
   * - ``o_FLUSH_CACHE()``
     - ``wire<bool>``
     - 
   * - ``o_IRQ_ACK()``
     - ``wire<int>``
     - 
   * - ``o_MEMINFO()``
     - ``io``
     - 
   * - ``o_OFFLOAD()``
     - ``wire<IssOffloadInsn<uint*_t>*>``
     - 
   * - ``o_TIME()``
     - ``wire<uint64_t>``
     - 

.. note::

   All components also inherit the following ports from ``Component``:

   - ``i_CLOCK()`` — clock input (signature: ``clock``)
   - ``i_RESET()`` — reset input (signature: ``wire<bool>``)
   - ``i_POWER()`` — power supply control (signature: ``wire<int>``)
   - ``i_VOLTAGE()`` — voltage control (signature: ``wire<int>``)

Usage example
-------------


.. code-block:: python

   import cpu.iss

   # Instantiate in a parent component
   comp = riscv.RiscvCommon(parent, 'name', isa=...)

   # Connect ports
   # master.o_XXX(comp.i_ENTRY())
   # master.o_XXX(comp.i_FETCHEN())
   # master.o_XXX(comp.i_FLUSH_CACHE_ACK())
