#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from typing import ClassVar

import gvsoc.systree as st
from vp.clock_domain import Clock_domain
from elftools.elf.elffile import ELFFile
from config_tree import Config, cfg_field


class CoreTestbenchSpislaveBootConfig(Config):
    """Boot of the chip through its SPI slave, driven by the testbench."""

    _defer_parent_init: ClassVar[bool] = True

    enabled: bool = cfg_field(default=False, dump=True, desc=(
        "True to boot the chip through its SPI slave at startup"
    ))
    entry: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Entry point the chip jumps to once the binary is loaded"
    ))
    delay_ps: int = cfg_field(default=0, dump=True, desc=(
        "Delay before starting the load, in ps"
    ))
    itf: int = cfg_field(default=0, dump=True, desc=(
        "SPI interface used for the boot"
    ))
    frequency: int = cfg_field(default=0, dump=True, desc=(
        "SPI frequency of the boot, in Hz"
    ))
    stim_file: str = cfg_field(default="", dump=True, desc=(
        "SLM file containing the binary to load"
    ))
    dummy_cycles: int = cfg_field(default=0, dump=True, desc=(
        "Number of dummy cycles of the SPI slave"
    ))
    full_duplex: bool = cfg_field(default=False, dump=True, desc=(
        "True if the SPI slave works in full duplex"
    ))


class CoreTestbenchConfig(Config):
    """Configuration of the testbench model (devices.testbench.testbench)."""

    ctrl_type: str = cfg_field(default="", dump=True, desc=(
        "Interface used to control the testbench, 'uart' for a UART"
    ))
    nb_gpio: int = cfg_field(default=0, dump=True, desc="Number of GPIOs")
    nb_spi: int = cfg_field(default=0, dump=True, desc="Number of SPI interfaces")
    nb_uart: int = cfg_field(default=0, dump=True, desc="Number of UART interfaces")
    nb_i2c: int = cfg_field(default=0, dump=True, desc="Number of I2C interfaces")
    nb_i2s: int = cfg_field(default=0, dump=True, desc="Number of I2S interfaces")
    uart_id: int = cfg_field(default=0, dump=True, desc=(
        "UART interface controlling the testbench"
    ))
    uart_baudrate: int = cfg_field(default=0, dump=True, desc=(
        "Baudrate of the UART controlling the testbench"
    ))
    spislave_boot: CoreTestbenchSpislaveBootConfig = cfg_field(
        default_factory=CoreTestbenchSpislaveBootConfig, desc="Boot through the SPI slave")


class Testbench(st.Component):
    """
    Testbench

    This instantiates a testbench which can be controlled from simulated SW or proxy to
    interact with a chip.

    Attributes
    ----------
    uart : list
        List of UART interfaces which must be connected to the testbench

    i2s : list
        List of I2S interfaces which must be connected to the testbench

    """

    def __init__(self, parent, name, uart=[], i2s=[], nb_gpio=0, spislave_dummy_cycles=0,
                 spislave_full_duplex=True):
        super(Testbench, self).__init__(parent, name)

        # Testbench implementation as this component is just a wrapper
        testbench = Testbench.Testbench_implem(self, 'testbench', nb_gpio=nb_gpio,
            spislave_dummy_cycles=spislave_dummy_cycles, spislave_full_duplex=spislave_full_duplex)

        # The testbench needs its owm cloc domain to enqueue clock events
        clock = Clock_domain(self, 'clock', frequency=50000000)

        self.bind(clock, 'out', testbench, 'clock')

        # UARTS
        for i in uart:
            # Each uart needs its own clock domain to enqueue clock events
            # Their frequency will changed by the uart models
            uart_clock = Clock_domain(self, 'uart%d_clock' % i, frequency=50000000)
            uart_tx_clock = Clock_domain(self, 'uart%d_tx_clock' % i, frequency=50000000)

            self.bind(self, 'uart%d' % i, testbench, 'uart%d' % i)

            self.bind(uart_clock, 'out', testbench, 'uart%d_clock' % i)
            self.bind(testbench, 'uart%d_clock_cfg' % i, uart_clock, 'clock_in')

            self.bind(uart_tx_clock, 'out', testbench, 'uart%d_tx_clock' % i)
            self.bind(testbench, 'uart%d_tx_clock_cfg' % i, uart_tx_clock, 'clock_in')

        # I2S
        for i in i2s:
            self.bind(testbench, 'i2s%d' % i, self, 'i2s%d' % i)

        # GPIO
        for i in range(0, nb_gpio):
            self.bind(self, f'gpio{i}', testbench, f'gpio{i}')

        # SPI
        for itf in range(0, 7):
            for cs in range(0, 4):
                self.bind(self, f'spi{itf}_cs{cs}_data', testbench, f'spi{itf}_cs{cs}_data')
                self.bind(self, f'spi{itf}_cs{cs}', testbench, f'spi{itf}_cs{cs}')


    class Testbench_implem(st.Component):

        def __init__(self, parent, name, uart_id=0, uart_baudrate=115200, nb_gpio=0,
                     spislave_dummy_cycles=0, spislave_full_duplex=True, spislave_binary=None):

            # The baudrate given as argument has never been used, the testbench is always
            # controlled at 115200
            config = CoreTestbenchConfig(
                ctrl_type="uart", nb_gpio=nb_gpio, nb_spi=7, nb_uart=5, nb_i2c=3, nb_i2s=4,
                uart_id=uart_id, uart_baudrate=115200,
                spislave_boot=CoreTestbenchSpislaveBootConfig(
                    enabled=False, delay_ps=1000000000, itf=4, frequency=50000000,
                    dummy_cycles=spislave_dummy_cycles, full_duplex=spislave_full_duplex))

            super(Testbench.Testbench_implem, self).__init__(parent, name, config=config)

            self.set_component('devices.testbench.testbench')

            if spislave_binary is not None:
                stim_file = self.get_target().get_abspath('spislave.slm')
                testbench.gen_jtag_stimuli(spislave_binary, stim_file)
                with open(spislave_binary, 'rb') as file:
                    config.spislave_boot.entry = ELFFile(file)['e_entry']
                config.spislave_boot.stim_file = stim_file
