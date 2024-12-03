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

import argparse
import gvsoc.runner
import gvsoc.systree
import gap.gap9.bare_board
import my_uart


# This is the Gapy descriptor for our board which will make it available on the command-line.
class Target(gvsoc.runner.Target):

    gapy_description = "GAP9 bare board with uart"

    def __init__(self, parser, options):
            super(Target, self).__init__(parser, options,
                model=MyBoard)


# This our custom board with our UART model.
# It just starts from a bare board which contains only a chip and the reset, and then add
# and connects our uart model
class MyBoard(gap.gap9.bare_board.Gap9BareBoard):

    def __init__(self, parent: gvsoc.systree.Component, name: str, parser: argparse.ArgumentParser,
                options: list):

        super(MyBoard, self).__init__(parent, name, parser, options)

        uart = my_uart.MyUart(self, 'uart')

        # Note that we also specify the pads where the UART is connected.
        # This is optional and is used to activate padframe model, which will check if pads
        # are well configured from the SW.
        self.get_chip().o_UART(0, uart.i_UART(), rx=60, tx=61, rts=62, cts=63)
