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

Format_RRRR = [ OutReg(0, Range(7,  5)),
                InReg (2, Range(7,  5), dumpName=False),
                InReg (0, Range(15, 5)),
                InReg (1, Range(20, 5)),
]
Format_LRR = [ OutReg(0, Range(7,  5)),
               InReg (0, Range(15, 5)),
               Indirect(InReg (1, Range(20, 5)), SignedImm(0, Ranges([]))),
]
Format_RRRU = [ OutReg(0, Range(7,  5)),
                InReg (0, Range(7,  5)),
                InReg (1, Range(15, 5)),
                UnsignedImm(0, Ranges([[25, 1, 0], [20, 5, 1]])),
]
Format_RRRU2 = [ OutReg(0, Range(7,  5)),
                 InReg (0, Range(15, 5)),
                 InReg (1, Range(20, 5)),
                 UnsignedImm(0, Range(25, 5)),
]
Format_RRRU3 = [ OutReg(0, Range(7,  5)),
                 InReg (0, Range(7,  5)),
                 InReg (1, Range(15, 5)),
                 UnsignedImm(0, Range(20, 5)),
]
Format_LR = [ OutReg(0, Range(7,  5)),
              Indirect(InReg (0, Range(15, 5)), InReg (1, Range(20, 5))),
]
Format_RR = [ OutReg(0, Range(7,  5)),
              InReg (0, Range(15, 5)),
              InReg (1, Range(20, 5)),
              InReg (2, Range(25, 5)),
]

# created list for pulp_nn isa extension
class PulpNn(IsaSubset):

    def __init__(self):
        super().__init__(name='pulpnn', active=False, instrs=[

            #add vector instruction ((missing scalar version))
            Instr('pv.add.n',         Format_R,     '0000000 ----- ----- 010 ----- 1010111'),
            Instr('pv.add.sc.n',      Format_R,     '0000000 ----- ----- 011 ----- 1010111'),
            Instr('pv.add.c',         Format_R,     '0000001 ----- ----- 010 ----- 1010111'),
            Instr('pv.add.sc.c',      Format_R,     '0000001 ----- ----- 011 ----- 1010111'),
            #sub vector instruction ((missing scalar version))
            Instr('pv.sub.n',         Format_R,     '0000100 ----- ----- 010 ----- 1010111'),
            Instr('pv.sub.sc.n',      Format_R,     '0000100 ----- ----- 011 ----- 1010111'),
            Instr('pv.sub.c',         Format_R,     '0000101 ----- ----- 010 ----- 1010111'),
            Instr('pv.sub.sc.c',      Format_R,     '0000101 ----- ----- 011 ----- 1010111'),
            # avg signed operands
            Instr('pv.avg.n',        Format_R,   '0001000 ----- ----- 010 ----- 1010111'),
            Instr('pv.avg.sc.n',     Format_R,   '0001000 ----- ----- 011 ----- 1010111'),
            Instr('pv.avg.c',        Format_R,   '0001001 ----- ----- 010 ----- 1010111'),
            Instr('pv.avg.sc.c',     Format_R,   '0001001 ----- ----- 011 ----- 1010111'),
            # avg unsigned operands
            Instr('pv.avgu.n',        Format_R,   '0001100 ----- ----- 010 ----- 1010111'),
            Instr('pv.avgu.sc.n',     Format_R,   '0001100 ----- ----- 011 ----- 1010111'),
            Instr('pv.avgu.c',        Format_R,   '0001101 ----- ----- 010 ----- 1010111'),
            Instr('pv.avgu.sc.c',     Format_R,   '0001101 ----- ----- 011 ----- 1010111'),
            # max signed operands
            Instr('pv.max.n',        Format_R,   '0011000 ----- ----- 010 ----- 1010111'),
            Instr('pv.max.sc.n',     Format_R,   '0011000 ----- ----- 011 ----- 1010111'),
            Instr('pv.max.c',        Format_R,   '0011001 ----- ----- 010 ----- 1010111'),
            Instr('pv.max.sc.c',     Format_R,   '0011001 ----- ----- 011 ----- 1010111'),
            # max unsigned operands
            Instr('pv.maxu.n',        Format_R,   '0011100 ----- ----- 010 ----- 1010111'),
            Instr('pv.maxu.sc.n',     Format_R,   '0011100 ----- ----- 011 ----- 1010111'),
            Instr('pv.maxu.c',        Format_R,   '0011101 ----- ----- 010 ----- 1010111'),
            Instr('pv.maxu.sc.c',     Format_R,   '0011101 ----- ----- 011 ----- 1010111'),
            # min signed operands
            Instr('pv.min.n',        Format_R,   '0010000 ----- ----- 010 ----- 1010111'),
            Instr('pv.min.sc.n',     Format_R,   '0010000 ----- ----- 011 ----- 1010111'),
            Instr('pv.min.c',        Format_R,   '0010001 ----- ----- 010 ----- 1010111'),
            Instr('pv.min.sc.c',     Format_R,   '0010001 ----- ----- 011 ----- 1010111'),
            # min unsigned operands
            Instr('pv.minu.n',        Format_R,   '0010100 ----- ----- 010 ----- 1010111'),
            Instr('pv.minu.sc.n',     Format_R,   '0010100 ----- ----- 011 ----- 1010111'),
            Instr('pv.minu.c',        Format_R,   '0010101 ----- ----- 010 ----- 1010111'),
            Instr('pv.minu.sc.c',     Format_R,   '0010101 ----- ----- 011 ----- 1010111'),
            # srl vector operations
            Instr('pv.srl.n',        Format_R,   '0100000 ----- ----- 010 ----- 1010111'),
            Instr('pv.srl.sc.n',     Format_R,   '0100000 ----- ----- 011 ----- 1010111'),
            Instr('pv.srl.c',        Format_R,   '0100001 ----- ----- 010 ----- 1010111'),
            Instr('pv.srl.sc.c',     Format_R,   '0100001 ----- ----- 011 ----- 1010111'),
            # sra vector operations
            Instr('pv.sra.n',        Format_R,   '0100100 ----- ----- 010 ----- 1010111'),
            Instr('pv.sra.sc.n',     Format_R,   '0100100 ----- ----- 011 ----- 1010111'),
            Instr('pv.sra.c',        Format_R,   '0100101 ----- ----- 010 ----- 1010111'),
            Instr('pv.sra.sc.c',     Format_R,   '0100101 ----- ----- 011 ----- 1010111'),
            # sll vector operations
            Instr('pv.sll.n',        Format_R,   '0101000 ----- ----- 010 ----- 1010111'),
            Instr('pv.sll.sc.n',     Format_R,   '0101000 ----- ----- 011 ----- 1010111'),
            Instr('pv.sll.c',        Format_R,   '0101001 ----- ----- 010 ----- 1010111'),
            Instr('pv.sll.sc.c',     Format_R,   '0101001 ----- ----- 011 ----- 1010111'),
            # or vector operations
            Instr('pv.or.n',        Format_R,   '0101100 ----- ----- 010 ----- 1010111'),
            Instr('pv.or.sc.n',     Format_R,   '0101100 ----- ----- 011 ----- 1010111'),
            Instr('pv.or.c',        Format_R,   '0101101 ----- ----- 010 ----- 1010111'),
            Instr('pv.or.sc.c',     Format_R,   '0101101 ----- ----- 011 ----- 1010111'),
            # xor vector operations
            Instr('pv.xor.n',        Format_R,   '0110000 ----- ----- 010 ----- 1010111'),
            Instr('pv.xor.sc.n',     Format_R,   '0110000 ----- ----- 011 ----- 1010111'),
            Instr('pv.xor.c',        Format_R,   '0110001 ----- ----- 010 ----- 1010111'),
            Instr('pv.xor.sc.c',     Format_R,   '0110001 ----- ----- 011 ----- 1010111'),
            # and vector operations
            Instr('pv.and.n',        Format_R,   '0110100 ----- ----- 010 ----- 1010111'),
            Instr('pv.and.sc.n',     Format_R,   '0110100 ----- ----- 011 ----- 1010111'),
            Instr('pv.and.c',        Format_R,   '0110101 ----- ----- 010 ----- 1010111'),
            Instr('pv.and.sc.c',     Format_R,   '0110101 ----- ----- 011 ----- 1010111'),
            # abs vector operations
            Instr('pv.abs.n',        Format_R,   '0111000 ----- ----- 010 ----- 1010111'),
            Instr('pv.abs.c',        Format_R,   '0111001 ----- ----- 010 ----- 1010111'),
            #dotup
            Instr('pv.dotup.n',     Format_RRRR,'1000000 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotup.n.sc',  Format_RRRR,'1000000 ----- ----- 011 ----- 1010111'),
            Instr('pv.dotup.c',     Format_RRRR,'1000001 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotup.c.sc',  Format_RRRR,'1000001 ----- ----- 011 ----- 1010111'),
            Instr('pv.dotusp.n',     Format_RRRR,'1000100 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotusp.n.sc',  Format_RRRR,'1000100 ----- ----- 011 ----- 1010111'),
            Instr('pv.dotusp.c',     Format_RRRR,'1000101 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotusp.c.sc',  Format_RRRR,'1000101 ----- ----- 011 ----- 1010111'),
            Instr('pv.dotsp.n',     Format_RRRR,'1001100 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotsp.n.sc',  Format_RRRR,'1001100 ----- ----- 011 ----- 1010111'),
            Instr('pv.dotsp.c',     Format_RRRR,'1001101 ----- ----- 010 ----- 1010111'),
            Instr('pv.dotsp.c.sc',  Format_RRRR,'1001101 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotup.n',     Format_RRRR,'1010000 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotup.n.sc',  Format_RRRR,'1010000 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotup.c',     Format_RRRR,'1010001 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotup.c.sc',  Format_RRRR,'1010001 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotusp.n',     Format_RRRR,'1010100 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotusp.n.sc',  Format_RRRR,'1010100 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotusp.c',     Format_RRRR,'1010101 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotusp.c.sc',  Format_RRRR,'1010101 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotsp.n',     Format_RRRR,'1011100 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotsp.n.sc',  Format_RRRR,'1011100 ----- ----- 011 ----- 1010111'),
            Instr('pv.sdotsp.c',     Format_RRRR,'1011101 ----- ----- 010 ----- 1010111'),
            Instr('pv.sdotsp.c.sc',  Format_RRRR,'1011101 ----- ----- 011 ----- 1010111'),
            # quantization
            Instr('pv.qnt.n',       Format_LRR,'1110000 ----- ----- 010 ----- 1010111'),
            # mac & load
            Instr('pv.mlsdotup.h', Format_RRRU3, '1110000 ----- ----- 000 ----- 1110111'),
            Instr('pv.mlsdotusp.h', Format_RRRU3, '1110100 ----- ----- 000 ----- 1110111'),
            Instr('pv.mlsdotsup.h', Format_RRRU3, '1101100 ----- ----- 000 ----- 1110111'),
            Instr('pv.mlsdotsp.h', Format_RRRU3, '1111100 ----- ----- 000 ----- 1110111'),
            Instr('pv.mlsdotup.b', Format_RRRU3, '1110000 ----- ----- 001 ----- 1110111'),
            Instr('pv.mlsdotusp.b', Format_RRRU3, '1110100 ----- ----- 001 ----- 1110111'),
            Instr('pv.mlsdotsup.b', Format_RRRU3, '1101100 ----- ----- 001 ----- 1110111'),
            Instr('pv.mlsdotsp.b', Format_RRRU3, '1111100 ----- ----- 001 ----- 1110111'),
            Instr('pv.mlsdotup.n', Format_RRRU3, '1110000 ----- ----- 010 ----- 1110111'),
            Instr('pv.mlsdotusp.n', Format_RRRU3, '1110100 ----- ----- 010 ----- 1110111'),
            Instr('pv.mlsdotsup.n', Format_RRRU3, '1101100 ----- ----- 010 ----- 1110111'),
            Instr('pv.mlsdotsp.n', Format_RRRU3, '1111100 ----- ----- 010 ----- 1110111'),
            Instr('pv.mlsdotup.c', Format_RRRU3, '1110000 ----- ----- 011 ----- 1110111'),
            Instr('pv.mlsdotusp.c', Format_RRRU3, '1110100 ----- ----- 011 ----- 1110111'),
            Instr('pv.mlsdotsup.c', Format_RRRU3, '1101100 ----- ----- 011 ----- 1110111'),
            Instr('pv.mlsdotsp.c', Format_RRRU3, '1111100 ----- ----- 011 ----- 1110111'),
        ])
