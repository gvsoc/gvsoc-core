Memory Checker
--------------

Introduction
++++++++++++

The memory checker, referred to as memcheck, is a feature that detects invalid accesses made by
the simulated software.

It currently detects two kinds of invalid accesses:

- Uninitialized accesses
- Buffer underflows and overflows

Usage
+++++

Memcheck can be enabled by adding the *--memcheck* option to the gapy or gvsoc command: ::

    gvsoc --target=gap.gap9.evk --binary=test image flash run --memcheck

This is the only option available for this feature.

Once a fault is detected, a warning is dumped by GVSOC: ::

  515864014: 19271: [/chip/soc/fc/regfile] Conditional jump depends on uninitialized register (reg: 0)
  Input error: Platform returned an error (exitcode: 1)

By default, GVSOC exits when such a warning is raised. To receive the warning but prevent GVSOC
from exiting, add the *--no-werror* option.

Possible errors are explained in the following sections.

Buffer Underflow/Overflow
+++++++++++++++++++++++++

Behavior
........

To detect if an access falls before or after a buffer, GVSOC inserts free space before and after
the buffer. This space is declared in a special way in the memory model, and any access to this
space triggers a warning.

Since memory in the simulated system can be small, GVSOC remaps buffers to a different location
using a kind of virtual address in order to get more space and be able to insert space between
and after the buffers. This explains why addresses may look strange when the memcheck
feature is enabled.

Buffers are remapped by the memory allocator using two special semi-hosting calls:

- To inform the memory model about the valid buffers
- To get the virtual address where the buffer has been remapped

The memory allocator does the allocation with the real address to still see fragmentation and
exhaustion issues but returns the virtual address to the caller, allowing the memory model to detect
faults.

The semi-hosting allocation call translates the real address into a virtual one and inserts free
space before and after.

The deallocation call translates back into the real address.

Since this mechanism is based on dynamic allocations, no fault can be detected on static variables.
Any faulty access to a buffer declared as a global variable will not be detected.

Faults
......

The buffer under/overflow checks only detect access outside the declared buffer that has been
dynamically allocated.

Such a fault can be triggered with the following code:

.. code-block:: C

    char *buff = pi_malloc(1024);
    buff[1024] = 0;

It is reported like this:

.. code-block:: shell

    525310849: 35826: [/chip/soc/l2_virtual/trace] Write access outside buffer (offset: 0x7618)
    525310849: 35826: [/chip/soc/l2_virtual/trace] Write access is 1 byte(s) after buffer (buffer_offset: 0x7218, buffer_size: 0x400)
    525310849: 35826: [/chip/soc/fc/lsu] Invalid access (pc: 0x1c010370, offset: 0x1c197618, size: 0x1, is_write: 1)
    Input error: Platform returned an error (exitcode: 1)

The memory first informs about the fault and specifies whether the access is before or after the
buffer, and how far in terms of bytes.

The fault is then reported to the faulting core as an invalid bus access, triggering the warning
and exit.

The reported buffer is the one closest to the access, either its last byte when searching before
the access or its first byte when searching after.

Since buffer under/overflows are detected by inserting empty space between buffers, it is possible
that the wrong buffer is reported if the access is far away from a buffer. Even worse, it can fall
into another buffer and no fault is reported in this case.

Uninitialized Accesses
+++++++++++++++++++++++

Behavior
........

When memcheck is enabled, memory models instantiate a dedicated valid array next to the classic
data array, with the same size. This valid array is initialized so that any data bit is considered
uninitialized. There is one valid bit for each data bit, so the granularity of the check is a bit.

Any write access to the memory model turns the corresponding valid bits into initialized bits.

Any read access to the memory model reports the valid bits to the initiator, which takes them into
account.

The whole memory is set as uninitialized when the simulation starts. Memory deallocation also sets
the entire chunk being freed as uninitialized, since it is not valid to access this memory until it
is written again.

Currently, only the core model checks them. It allows read accesses to uninitialized locations but
raises a fault if an uninitialized value is used for:

- A branch, since it can lead to a jump to an invalid address
- A memory access, since it leads to an invalid address

Faults
......

Many uninitialized accesses are not reported because it is legal to load an uninitialized location.
The compiler can do this for speculation. For example, it can load a value in advance while not
being sure it is valid because it depends on the result of a check.

The most common fault for uninitialized accesses is loading a value from an uninitialized location
and using it for a check. This fault can make the core randomly jump to a branch or another.

This can be triggered with the following code:

.. code-block:: C

    char *buff = pi_malloc(1024);
    if (buff[256])
    {
        exit(0);
    }

This will report the following warning:

.. code-block:: shell

    559544241: 36866: [/chip/soc/fc/regfile] Conditional jump depends on uninitialized register (reg: 0)
    Input error: Platform returned an error (exitcode: 1)

The second kind of error occurs when the core tries to use an uninitialized value to build an
address and accesses it. 

This can happen with the following code:

.. code-block:: C

    uint32_t *buff = pi_malloc(1024);

    buff[128] = 0x1c040000;

    pi_free(buff, 1024);

    buff = pi_malloc(1024);
    *(uint32_t *)buff[128] = 0;

To trigger it, the idea is to read a pointer from an uninitialized location and use it to perform
an access.

The goal is to access a valid location using a valid pointer retrieved from an uninitialized
location. This type of bug is tricky to debug because it seems valid at first since the address
looks good. Memcheck detects that the pointer is still invalid because it is built from an
uninitialized location.

This happens when a buffer is allocated, valid values are written into it, the buffer is freed,
then the same buffer is allocated, and data is read from it without initializing it.

This use-case is reproduced by the code. A buffer is allocated, a valid address is written, the
buffer is freed, then reallocated, and an access is made from the pointer written earlier. A valid
pointer is obtained, but the code is invalid.

Note that this code works if the memory allocator returns the same chunk, which may not always be
true.

This code will trigger the following warning:

.. code-block:: shell

    511737517: 34945: [/chip/soc/fc/regfile] Access address depends on uninitialized register (reg: 0)
    Input error: Platform returned an error (exitcode: 1)

Runtime Support
+++++++++++++++

As mentioned earlier, memory allocations and deallocations must be declared using two special
semi-hosting calls.

These calls are accessible using the gvsoc target header (gvsoc.h) by calling these functions:

.. code-block:: C

    static inline void *gv_memcheck_mem_alloc(int mem_id, void *ptr, size_t size);

    static inline void *gv_memcheck_mem_free(int mem_id, void *ptr, size_t size);

The first argument is the memory identifier where the operation is performed. Each memory in the
simulated system is assigned an identifier that must be provided here.

The other arguments describe the chunk being allocated or deallocated by the memory allocator in
the simulated software.

GDB Support
+++++++++++

The memcheck warnings provide information about the invalid access but not about the code that
triggered the fault.

To get more information, GDB can be connected. Any memcheck fault triggers a bus error, which is
caught by GDB. GDB then shows the source code where the bus error occurred and gives control back
to the user.

Once GDB is connected, the code shown in the first buffer under/overflow error will make GDB print
the following message:

.. code-block:: shell

    Thread 11 received signal SIGBUS, Bus error.
    0x1c010376 in main (argc=<optimized out>, argv=<optimized out>) at test.c:8
    8        buff[1024] = 10;
    (gdb)

From there, the backtrace can be shown, and variables can be dumped to understand what led to this
fault.
