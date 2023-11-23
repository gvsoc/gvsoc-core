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

import gvsoc.systree as st
from vp.clock_domain import Clock_domain


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

    def __init__(self, parent, name, uart=[], i2s=[], nb_gpio=0, spislave_dummy_cycles=0):
        super(Testbench, self).__init__(parent, name)

        # Testbench implementation as this component is just a wrapper
        testbench = Testbench.Testbench_implem(self, 'testbench', nb_gpio=nb_gpio, spislave_dummy_cycles=spislave_dummy_cycles)

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

        def __init__(self, parent, name, uart_id=0, uart_baudrate=115200, nb_gpio=0, spislave_dummy_cycles=0):
            super(Testbench.Testbench_implem, self).__init__(parent, name)

            # Register all parameters as properties so that they can be overwritten from the command-line
            self.add_property('uart_id', uart_id)
            self.add_property('uart_baudrate', 115200)

            self.set_component('devices.testbench.testbench')
            
            self.add_properties({
                "verbose": False,
                "ctrl_type": "uart",
                "nb_gpio": nb_gpio,
                "nb_spi": 7,
                "nb_uart": 5,
                "nb_i2c": 3,
                "nb_i2s": 4,
                "uart_id": self.get_property('uart_id'),
                "uart_baudrate": self.get_property('uart_baudrate'),

                "spislave_boot": {
                    "enabled": False,
                    "delay_ps": "1000000000",
                    "itf": 4,
                    "frequency": 50000000,
                    "stim_file": None,
                    "dummy_cycles": spislave_dummy_cycles
                }
            })
