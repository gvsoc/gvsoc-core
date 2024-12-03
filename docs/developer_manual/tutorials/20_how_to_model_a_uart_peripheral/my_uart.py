#
# Copyright (C) 2022 GreenWaves Technologies
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

import gvsoc.systree
import utils.uart.uart_adapter


# This is the generator for our UART model.
# It allows the upper generator like the board instantiating it and binding it
class MyUart(gvsoc.systree.Component):

    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super(MyUart, self).__init__(parent, name)

        # Include the adapter. This will make sure the adapter sources are compiled
        utils.uart.uart_adapter.UartAdapter(self)

        # Our source code. This will make sure it gets compile when we compile GVSOC for our
        # board.
        self.add_sources(['my_uart.cpp'])

        # Declare a user property so that the baudrate can be configured from the command-line
        # Such properties can be displayed with gapy command "target_properties" and the baudrate
        # can be set with option --target-property uart/baudrate=1000000
        baudrate = self.declare_user_property(
            name='baudrate', value=115200, cast=int,
            description='Specifies uart baudrate in bps'
        )

        # Store this use property as component property so that the C++ code can get it
        self.add_property('baudrate', baudrate)


    # Create a method to let upper generator easily connect this component
    def i_UART(self) -> gvsoc.systree.SlaveItf:
        # This interface is the UART input interface. The name here must match the one used
        # in the model when instantiating the adapter.
        return self.slave_itf(name='uart', signature='uart')
