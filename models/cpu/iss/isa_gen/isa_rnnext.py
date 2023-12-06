#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
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

from cpu.iss.isa_gen.isa_gen import *
from cpu.iss.isa_gen.isa_riscv_gen import *

Format_LRRRR = [ OutReg(0, Range(7,  5)),
                 InReg (2, Range(7,  5), dumpName=False),
                 Indirect(InReg(0, Range(15, 5)), SignedImm(0, Ranges([]))),
                 InReg (1, Range(20, 5)),
]
Format_LRR = [ OutReg(0, Range(7,  5)),
               InReg (0, Range(15, 5)),
               Indirect(InReg (1, Range(20, 5)), SignedImm(0, Ranges([]))),
]
Format_R1 = [ OutReg(0, Range(7,  5)),
              InReg (0, Range(15, 5)),
]
Format_LR = [ OutReg(0, Range(7,  5)),
              Indirect(InReg (0, Range(15, 5)), InReg (1, Range(20, 5))),
]

class RnnExt(IsaSubset):

    def __init__(self):
        super().__init__(name='rnnext', active=False, instrs=[
            Instr('pl.sdotsp.h.0', Format_LRRRR,'101110- ----- ----- 000 ----- 1110111'),
            Instr('pl.sdotsp.h.1', Format_LRRRR,'101111- ----- ----- 000 ----- 1110111'),
            Instr('pl.tanh',     Format_R1,   '1111100 00000 ----- 000 ----- 1110111'),
            Instr('pl.sig',      Format_R1,   '1111100 00000 ----- 001 ----- 1110111'),
        ])
