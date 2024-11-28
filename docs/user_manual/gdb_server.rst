GDB Server
----------

Introduction
............

GVSOC can be launched with a GDB server to allow a GDB client to connect and debug the 
simulated software.

The GDB server does not simulate any real hardware like debug IPs but directly interacts with models
such as the ISS and memories to implement the RSP protocol.

Watchpoints and breakpoints function like hardware breakpoints because they directly interact with
the models to ensure they are caught.

Usage
.....

GVSOC can be launched with the GDB server using the *--gdb-server* option: ::

    gvsoc --target=gap.gap9.evk --binary=test image flash run --gdbserver

This command starts GVSOC and opens an RSP port for the GDB client to connect to, which is printed
on the console: ::

    GDB server listening on port 12345

The port can be changed using the *--gdbserver-port* option, which is useful if multiple simulations
are launched in parallel: ::

    gvsoc --target=gap.gap9.evk --binary=test image flash run --gdbserver --gdbserver-port=10000

The simulation has not started yet and is waiting for the client to connect before continuing.

To connect, launch GDB from a different terminal. Ensure you use GDB from a recent toolchain. Debug
symbols may not be handled correctly if using an older toolchain, leading to invalid accesses. For
the Gap SDK, use GDB from a generic RISC-V toolchain.

Launch GDB with the application test, which must include debug symbols (compiled with -g): ::

    riscv64-unknown-elf-gdb test

GDB should then print the prompt, and you can connect to GVSOC as if it was a remote target, using
the port specified on the GVSOC side: ::

    (gdb) target remote :12345
    Remote debugging using :12345
    0x1a000080 in ?? ()

Since the simulation has not started, GDB will see the very first instruction of the target, which
might be inside the ROM on targets like Gap. In this case, set a breakpoint at `main` and continue,
since the ROM is not suitable for GDB: ::

    (gdb) break main
    Breakpoint 1 at 0x1c010ab4: file test.c, line 133.
    (gdb) continue
    Continuing.

    Thread 11 hit Breakpoint 1, main () at test.c:215
    215 return pmsis_kickoff((void *)test_kickoff);

From this point, most of GDB's features can be used.

Multi-Core View
...............

In GVSOC, cores are seen from GDB as threads within the same process. The state of the cores can be
displayed using the *info threads* command: ::

    (gdb) info threads
    Id Target Id Frame
    1 Thread 1 (pe0) 0x00000000 in ?? ()
    2 Thread 2 (pe1) 0x00000000 in ?? ()
    3 Thread 3 (pe2) 0x00000000 in ?? ()
    4 Thread 4 (pe3) 0x00000000 in ?? ()
    5 Thread 5 (pe4) 0x00000000 in ?? ()
    6 Thread 6 (pe5) 0x00000000 in ?? ()
    7 Thread 7 (pe6) 0x00000000 in ?? ()
    8 Thread 8 (pe7) 0x00000000 in ?? ()
    9 Thread 9 (pe8) 0x00000000 in ?? ()
    10 Thread 10 (mchan) 0x00000000 in ?? ()
    *11 Thread 11 (fc) main () at test.c:215

There is one thread per core. Most of them have an invalid program counter because they have not
been powered up yet. Only the fabric controller is at a valid location.

Once the cluster is up, all the cores have a valid state: ::

    (gdb) info threads
    Id Target Id Frame
    1 Thread 1 (pe0) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    2 Thread 2 (pe1) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    3 Thread 3 (pe2) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    4 Thread 4 (pe3) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    5 Thread 5 (pe4) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    6 Thread 6 (pe5) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    *7 Thread 7 (pe6) pe_entry (arg=0x1000085c) at test.c:23
    8 Thread 8 (pe7) evt_read32 (offset=192, base=2113536) at rtos/pmsis/archi/include/archi/chips/gap9_v2/event_unit/event_unit.h:50
    9 Thread 9 (pe8) pi_cl_team_nb_cores () at rtos/pmsis_gap/kernel/chips/gap9/cluster/cluster_task.h:65
    10 Thread 10 (mchan) 0x00000000 in ?? ()
    11 Thread 11 (fc) __pi_task_sleep_loop () at rtos/pmsis_gap/os/pulpos/kernel/task_asm.S:60

There is also an additional thread called `mchan`, which represents the cluster DMA. The only
supported command for `mchan` is the watchpoint, which can be used to check when a DMA is accessing
a specific location. Other commands like stepping and breakpoints do not have any effect on this
thread.

The star next to the thread number in the thread info view indicates the active thread (or core)
where commands like backtrace or stepping will be applied.

Watchpoint and Breakpoint Support
.................................

Cores support any number of both breakpoints and watchpoints.

The DMA supports any number of watchpoints. Watchpoints can be caught in the DMA when the address
of a watchpoint is hit by a DMA transfer. In this case, the DMA model is put on hold in the middle
of the transfer until execution is resumed.

Timing Behavior
...............

As soon as GDB starts execution for the first time, the simulation starts and stops only when the
simulated application is over.

When execution stops from GDB's point of view, the simulation is still running, like a real board.
Only some models like cores and DMAs are stalled because GDB stopped them.

This can happen, for example, when a breakpoint is reached, a control-C is hit, or after a step.
The cores are then seen as stopped from GDB because their program counter is no longer changing,
but the simulation time is still increasing.

The GDB server works in a synchronized way. As soon as a stop condition is detected, all threads
are stalled so they don't do anything. For example, if a breakpoint is hit on one core, the others
will stop as well. Conversely, when execution is resumed, all threads are unstalled and will have
some activity for some time. For example, when executing the *stepi* command to step one assembly
instruction, all the cores will continue execution. The active core will stop right after the first
instruction executed, and other cores might have time to execute more than one instruction or none
at all, depending on instruction latency.