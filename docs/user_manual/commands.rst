Commands
--------

gvrun provides a set of commands to control the target.

Multiple commands can be provided on the same command line. They are executed sequentially, from
left to right.

The special command *commands* can be used to get the list of available commands. Since the list
can vary depending on the target and the specific options, it is good to execute it with the full
command-line, particularly with the target specified: ::

  gvrun --target rv64 commands

Here is a non-exhaustive list of common commands, which can be found on most targets, even though
targets can remove commands if they don't make sense for that target or add commands:

=======================  ==============================================================================
Commands                 Description
=======================  ==============================================================================
commands                 Show the list of available commands
targets                  Show the list of available targets
image                    Generate the target images needed to run execution
flash                    Upload the flash contents to the target
compile                  Build executables for the target
build                    Execute the commands compile, image and flash
run                      Start execution on the target
all                      Execute the commands build and run
tree                     Dump the tree of attributes and parameters
diagram                  Generate a Graphviz architecture diagram of the target
=======================  ==============================================================================

commands
........

This shows the list of available commands, which should be similar to the table above.

Since targets or options can remove commands that do not make sense for the target or add
dedicated commands, the list of commands may be slightly different from this list. It is also good
to execute this command with the right target and the full list of options: ::

  gvrun --target gap.gap9.evk:attr.boot.mode=flash commands

In this example, the attribute is included in case it changes the list of commands.

targets
.......

This shows the list of available targets and a short description: ::

  gvrun --target rv64 targets

image
.....

This command is needed only for targets where files like the application binary should be embedded
inside an image. This is usually the case for targets having a flash.

This will build an image containing all the files specified to be put inside the image.

This image can then be used by other commands to be uploaded to the target.

The files to be put inside the image are specified through dedicated options which are explained
later.

flash
.....

This command uploads the flash image built with the *image* command into the target.

Since there is no physical target on GVSOC, this actually produces a file that the model can
read in order to initialize the flash when the simulation is started.

The format of the file is specific to each model.

build
.....

This is a convenience command that combines *compile*, *image*, and *flash* in a single step.

all
...

This is a convenience command that combines *build* and *run* in a single step.

tree
....

This command dumps the full hierarchy of attributes and parameters for the target system.

The format can be controlled with the ``--tree-format`` option. Available format items are:
``attr``, ``arch``, ``target``, ``build``, ``prop``, and ``all``: ::

  gvrun --target gap.gap9.evk tree --tree-format=all

diagram
.......

This command generates a Graphviz architecture diagram of the target system: ::

  gvrun --target gap.gap9.evk diagram
