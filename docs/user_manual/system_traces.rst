System Traces
-------------

Description
...........

The virtual platform allows dumping architecture events to help developers debug their applications
by providing detailed insights into system activities.

For example, it can show instructions being executed, DMA transfers, events generated, memory
accesses, and more.

This feature can be enabled and configured using the *--trace* option. This option takes an
argument specifying a regular expression for the path in the architecture where traces must be
enabled and, optionally, a file where the traces should be dumped. All components whose path matches
the specified expression will dump traces. Several paths can be specified by using the option
multiple times. Here is an example that activates instruction traces for core 0 and core 1: ::

  gvsoc --target=gap.gap9.evk --binary=test run --trace=pe0/insn --trace=pe1/insn

The trace file should look like this: ::

  720264540: 103619: [/chip/cluster/pe1/insn] itc_status_set_set:65 M 1c0114da c.sw a4, 16(a5) a4:00000001 a5:1a109000 PA:1a109010
  720264540: 103619: [/chip/cluster/pe0/insn] pi_cluster_event_notify:302 M 1c0114d0 c.sw a4, 4(a0) a4:1c0016ac a0:1c001690 PA:1c001694

Each event is usually represented by one line, though some events may take multiple lines to
display more information.

The number on the left is the event's timestamp in picoseconds, followed by the number of cycles.
Different blocks, like clusters, can have different frequencies. The timestamp is absolute and
increases linearly, while the cycle count is local to the frequency domain.

The second part, a string, indicates the path in the architecture where the event occurred, helping
differentiate blocks of the same kind generating the same event. This path can also be used with
the *--trace* option to filter the events.

The third part, also a string, contains the event-specific information. For instance, the core
simulator prints information about the executed instruction.

Trace Paths
...........

Finding the correct paths to activate for necessary information can be challenging. One method is
to dump all events using *--trace=.**, identify the interesting ones, and then refine the command
line with a more restrictive regular expression. Here are the paths for the main components on Gap9 (note that this can vary between chips):

========================================= ===============================
Path                                      Description
========================================= ===============================
/chip/cluster/pe0                         Processing element; useful to see the IOs made by the core and the instructions it executes. Use */iss* to get only instruction events.
/chip/cluster/event_unit                  Hardware synchronizer events; useful for debugging inter-core synchronization mechanisms.
/chip/cluster/icache                      Shared program cache accesses.
/chip/cluster/l1/bankX                    L1 memory banks (replace X with the bank number).
/chip/soc/l2                              L2 memory accesses.
/chip/cluster/dma                         DMA events.
========================================= ===============================

Initially, core instruction traces are the most interesting, as they show not only the executed
instructions but also the accessed registers, their content, and memory accesses, making them very
useful for debugging issues like memory corruption.

Trace Levels
............

Each trace is dumped with a certain level, allowing users to specify the amount of trace information
to be dumped based on their level using the *--trace-level* option. The available levels are
*error*, *warning*, *info*, *debug*, and *trace*.

The default level is *debug*, which corresponds to user-level debug information.

Instruction Traces
..................

Here is an example of an instruction trace: ::

  720264540: 103619: [/chip/cluster/pe1/insn] itc_status_set_set:65 M 1c0114da c.sw a4, 16(a5) a4:00000001 a5:1a109000 PA:1a109010

The event information for executed instructions follows this format:

<function> <priv> <address> <instruction> <operands> <operands info>

Where:

- **<function>** is the function name where the PC belongs, extracted from the debug information. If
  debug information is not enabled, this field is empty.
- **<priv>** is the privilege level.
- **<address>** is the address of the instruction.
- **<instruction>** is the instruction label.
- **<operands>** is the decoded operands.
- **<operands info>** gives details about operand values and their usage.

Operand information follows this convention:

- When a register is accessed, its name is displayed, followed by *=* if it is written or *:* if it
  is read. If both read and written, the register appears twice, followed by its value (the new one
  if written).
- For memory access, *PA:* is displayed, followed by the access address.
- Statements follow the order of the decoded instruction.

Memory accesses displayed are particularly useful for tracking memory corruptions, as they help
identify specific location accesses.

Dumping Traces to a File
........................

By default, all traces are dumped to standard output, but you can specify a file where the traces
should be dumped. The file must be specified for each *--trace* option. You can use the same file
to gather all traces or different files for separate traces.

Example to dump all possible traces into one file: ::

  gvsoc --target=gap.gap9.evk run --trace=.*:log.txt

Example to dump instruction traces to one file and L2 memory accesses to another file: ::

  gvsoc --target=gap.gap9.evk run --trace=insn:insn.txt --trace=l2:l2.txt
