Usage
-----

The GVSOC  simulation launch process is handled by a runner named Gapy. This runner abstracts the
application launch procedure on GVSOC , enabling the same application to be deployed consistently
across multiple platforms, such as GVSOC , HDL simulators, or real hardware. Despite this
standardization, custom options can still be specified to utilize GVSOC -specific features, such as
trace management.

Specifying the Platform
.......................

When Gapy is executed, the GVSOC  platform must be specified using the `--platform` option: ::

  gapy --platform=gvsoc

Specifying the Target
.....................

Since the same generic GVSOC  engine is compiled once for all supported targets, the target to be
simulated must be specified using the `--target` option: ::

  gapy --platform=gvsoc --target=rv64

The target tells GVSOC  which system must be simulated. It corresponds to a Python generator
which will be called by Gapy to instantiate the set of components which will simulate the target
system. In this example, the `rv64` system instantiates a generic RISC-V 64-bit system with a single
memory which is able to boot Linux.

Specifying Target Directories
.............................

Since targets are chip-dependent, Gapy needs to know where the possible targets can be found
through the `--target-dir` option. As the target is a Python script, called a generator, which
will use other Python scripts to instantiate the full architecture, this option can be used several
times to specify multiple paths. The paths usually depend on the environment where GVSOC  is
integrated. Here is an example for Gap SDK: ::

  gapy --platform=gvsoc --target=gap.gap9.evk --target-dir=$GAP_SDK_HOME/gvsoc/gvsoc_gap --target-dir=$GAP_SDK_HOME/gvsoc/gvsoc/models

Specifying Model Directories
............................

Since models are compiled separately from the engine as shared libraries and dynamically loaded
by the engine when the target system is instantiated, some additional options might be needed
to give the paths to these models: ::

  gapy --platform=gvsoc --target=gap.gap9.evk --target-dir=$GAP_SDK_HOME/gvsoc/gvsoc_gap --target-dir=$GAP_SDK_HOME/gvsoc/gvsoc/models --model-dir=$GAP_SDK_HOME/install/workstation/models

Using the *gvsoc* script
........................

The *gvsoc* script can simplify the process by avoiding the need to specify the platform, target,
and model directories explicitly: ::

  gvsoc --target=gap.gap9.evk

This script will execute a `gapy` command with all the necessary directories automatically taken
from the installation folder.

In the following sections, we will use the *gvsoc* script for examples. However, these examples are
still relevant if you use the *gapy* command with the platform, target, and models directories
specified.

Specifying the Working Directory
................................

As GVSOC  will generate several temporary files, it is also good to launch it from a specific folder
or to specify it through the `--work-dir` option: ::

  gvsoc --target=rv64 --work-dir=build

Viewing available Options
.........................

The list of available options can be displayed using the `--help` option. Since options can differ
depending on the platform, target, or even other options, it is important to execute it with the
full command-line: ::

  gvsoc --target=rv64 --help

Specifying the Application Binary
.................................

One example of a chip-dependent option is the `--binary` option which allows specifying the
application binary to be simulated, which is not relevant for all chips: ::

  gvsoc --target=rv64 --binary=test.elf

Running Commands
................

Finally, as Gapy manages all that is needed to execute an application on the target, like
producing flash images, it must be told the list of commands to be executed. Here are a few
examples: ::

  gvsoc --target=rv64 --binary=test.elf run

  gvsoc --target=gap.gap9.evk --work-dir=build image flash run