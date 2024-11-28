Commands
--------

Gapy provides a set of commands to control the target.

Multiple commands can be provided on the same command line. They are executed sequentially, from
left to right.

The special command *commands* can be used to get the list of available commands. Since the list
can vary depending on the target and the specific options, it is good to execute it with the full
command-line, particularly with the target specified: ::

  gvsoc --target=rv64 commands

Here is a non-exhaustive list of common commands, which can be found on most targets, even though
targets can remove commands if they don't make sense for that target or add commands:

=======================  ==============================================================================
Commands                 Description
=======================  ==============================================================================
commands                 Show the list of available commands
targets                  Show the list of available targets. Gapy will search within the specified
                         target directories for valid targets
image                    Generate the target images needed to run execution
flash                    Upload the flash contents to the target
flash_layout             Dump the layout of the flashes
flash_dump_sections      Dump each section of each flash memory
flash_dump_app_sections  Dump each section of each flash memory
flash_properties         Dump the value of all flash section properties
target_properties        Dump the value of all target properties
run                      Start execution on the target
=======================  ==============================================================================

commands
........

This shows the list of available commands, which should be similar to the table above.

Since targets or options can remove commands that do not make sense for the target or add
dedicated commands, the list of commands may be slightly different from this list. It is also good
to execute this command with the right target and the full list of options: ::

  gvsoc --target=gap.gap9.evk --target-property=boot.mode=flash commands

In this example, the target property is included in case it changes the list of commands.

targets
.......

This shows the list of available targets and a short description.

Since the available targets are discovered from the target directories, it is important to
specify them when using this command with Gapy, but it is not necessary with the gvsoc command: ::

  gapy --target-dir=$GAP_SDK_HOME/gvsoc/gvsoc_gap targets

  gvsoc targets

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

Since there is no physical target on GVSOC , this actually produces a file that the model can
read in order to initialize the flash when the simulation is started.

The format of the file is specific to each model.

flash Layout
............

This command dumps the content of all the flash.

This will show all what is put inside the flash image and is useful for checking what might be
wrong with a flash image.

The option *--flash-layout-level=<integer>* can be added to specify the depth of the tree which is
dumped.

A value of 0 will dump only the name of each flash section:

.. code-block:: shell

  Layout for flash: mram
    +----------------+-----------------+--------------+
    | Section offset | Section name    | Section size |
    +----------------+-----------------+--------------+
    | 0x0            | rom             | 0x83a0       |
    | 0x83a0         | partition_table | 0xc0         |
    | 0x8460         | app_binary      | 0x0          |
    | 0x8460         | readfs          | 0x10         |
    | 0x8470         | hostfs          | 0x0          |
    | 0xa000         | lfs             | 0x0          |
    | 0xa000         | raw             | 0x1f6000     |
    +----------------+-----------------+--------------+

Higher values will dump more information about each section's content:

.. code-block:: shell

  Layout for flash: mram
    +----------------+-----------------+--------------+----------------------------------------------------------------------------------------------------+
    | Section offset | Section name    | Section size | Section content                                                                                    |
    +----------------+-----------------+--------------+----------------------------------------------------------------------------------------------------+
    | 0x0            | rom             | 0x83a0       | +--------+--------+-----------------------+------------------------------------------------------+ |
    |                |                 |              | | Offset | Size   | Name                  | Content                                              | |
    |                |                 |              | +--------+--------+-----------------------+------------------------------------------------------+ |
    |                |                 |              | | 0x0    | 0x534  | ROM header            | +--------+--------------------+-------+------------+ | |
    |                |                 |              | |        |        |                       | | Offset | Name               | Size  | Value      | | |
    |                |                 |              | |        |        |                       | +--------+--------------------+-------+------------+ | |
    |                |                 |              | |        |        |                       | | 0x0    | next_section       | 0x4   | 0x83a0     | | |
    |                |                 |              | |        |        |                       | | 0x4    | nb_segments        | 0x4   | 0x4        | | |
    |                |                 |              | |        |        |                       | | 0x8    | entry              | 0x4   | 0x1c010294 | | |
    |                |                 |              | |        |        |                       | | 0xc    | unused             | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x10   | xip_dev            | 0x4   | 0x2        | | |
    |                |                 |              | |        |        |                       | | 0x14   | xip_vaddr          | 0x4   | 0x20000000 | | |
    |                |                 |              | |        |        |                       | | 0x18   | xip_page_size      | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x1c   | xip_flash_base     | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x20   | xip_flash_nb_pages | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x24   | xip_l2_base        | 0x4   | 0x1c18e000 | | |
    |                |                 |              | |        |        |                       | | 0x28   | xip_l2_nb_pages    | 0x4   | 0x10       | | |
    |                |                 |              | |        |        |                       | | 0x2c   | kc_length          | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x30   | key_length         | 0x4   | 0x0        | | |
    |                |                 |              | |        |        |                       | | 0x34   | ac                 | 0x400 | -          | | |
    |                |                 |              | |        |        |                       | | 0x434  | kc                 | 0x80  | -          | | |
    |                |                 |              | |        |        |                       | | 0x4b4  | kc_write           | 0x80  | -          | | |
    |                |                 |              | | 0x534  | 0x10   | Binary segment header | +--------+--------------+------+------------+        | |
    |                |                 |              | |        |        |                       | | Offset | Name         | Size | Value      |        | |
    |                |                 |              | |        |        |                       | +--------+--------------+------+------------+        | |
    |                |                 |              | |        |        |                       | | 0x534  | flash_offset | 0x4  | 0x574      |        | |
    |                |                 |              | |        |        |                       | | 0x538  | mem_addr     | 0x4  | 0x1c000008 |        | |
    |                |                 |              | |        |        |                       | | 0x53c  | size         | 0x4  | 0x780      |        | |
    |                |                 |              | |        |        |                       | | 0x540  | crc          | 0x4  | 

