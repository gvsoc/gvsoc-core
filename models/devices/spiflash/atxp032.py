# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""ATXP032 octal SPI NOR flash, at command level.

The flash controller hands over each command with its opcode, address and
data once the bus phase is over (wire<SpiflashCmd*>, spiflash_cmd.hpp). Any
bulk read returns the array, a bulk write programs it (after a write enable),
and the usual single commands are decoded: write enable/disable, status read,
JEDEC ID, status write, the block and chip erases. Everything else is accepted
and ignored. Programs, erases and status writes keep the flash busy, in its
status register, for the time they take on the device.
"""

from typing import Annotated

from config_tree import Config, cfg_field
from gvrun.runtime import Runtime
from gvsoc.systree import Component, SlaveItf


class Atxp032Config(Config):
    size: int = cfg_field(default=4 * 1024 * 1024, fmt="hex", dump=True,
        desc="Flash size in bytes")
    image: Annotated[str, Runtime] = cfg_field(default=None, dump=True,
        desc="Preload image path; set late by the board")
    # Busy time of the operations, in ns: the typical ones of the ATXP032
    # datasheet
    program_byte_ns: int = cfg_field(default=8_000, dump=True,
        desc="Program time per byte; a program takes the smaller of this times its bytes "
             "and the page program time")
    program_page_ns: int = cfg_field(default=1_250_000, dump=True,
        desc="Page program time")
    erase_4k_ns: int = cfg_field(default=35_000_000, dump=True, desc="4 KB block erase time")
    erase_32k_ns: int = cfg_field(default=250_000_000, dump=True, desc="32 KB block erase time")
    erase_64k_ns: int = cfg_field(default=450_000_000, dump=True, desc="64 KB block erase time")
    erase_chip_ns: int = cfg_field(default=3_500_000_000, dump=True, desc="Chip erase time")
    write_status_ns: int = cfg_field(default=200, dump=True,
        desc="Status register write time")


class Atxp032(Component):

    def __init__(self, parent: Component, name: str, config: Atxp032Config | None = None):
        if config is None:
            config = Atxp032Config()
        super().__init__(parent, name, config=config)

        self.add_sources(['devices/spiflash/atxp032.cpp'])

    def i_INPUT(self) -> SlaveItf:
        return SlaveItf(self, 'input', signature='wire<SpiflashCmd*>')
