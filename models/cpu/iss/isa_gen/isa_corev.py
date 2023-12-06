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

Format_BITREV = [ OutReg(0, Range(7,  5)),
                  InReg (0, Range(15, 5)),
                  UnsignedImm(0, Range(20, 5)),
                  UnsignedImm(1, Range(25, 2)),
]
Format_RRRR = [ OutReg(0, Range(7,  5)),
                InReg (2, Range(7,  5), dumpName=False),
                InReg (0, Range(15, 5)),
                InReg (1, Range(20, 5)),
]
Format_RRRR2 = [ OutReg(0, Range(7,  5)),
                 InReg (0, Range(7,  5), dumpName=False),
                 InReg (1, Range(15, 5)),
                 InReg (2, Range(20, 5)),
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
Format_RRRRU = [ OutReg(0, Range(7,  5)),
                 InReg (2, Range(7,  5)),
                 InReg (0, Range(15, 5)),
                 InReg (1, Range(20, 5)),
                 UnsignedImm(0, Range(25, 5)),
]
Format_R1 = [ OutReg(0, Range(7,  5)),
              InReg (0, Range(15, 5)),
]
Format_LR = [ OutReg(0, Range(7,  5)),
              Indirect(InReg (0, Range(15, 5)), InReg (1, Range(20, 5))),
]
Format_LRPOST = [ OutReg(0, Range(7,  5)),
                  Indirect(InReg (0, Range(15, 5)), InReg (1, Range(20, 5)), postInc=True),
]
Format_RR = [ OutReg(0, Range(7,  5)),
              InReg (0, Range(15, 5)),
              InReg (1, Range(20, 5)),
              InReg (2, Range(25, 5)),
]
Format_SR = [ InReg (1, Range(20, 5)),
              Indirect(InReg (0, Range(15, 5)), InReg (2, Range(7, 5))),
]
Format_LPOST = [ OutReg(0, Range(7,  5)),
                 Indirect(InReg (0, Range(15, 5)), SignedImm(0, Range(20, 12)), postInc=True),
]
Format_I4U = [ OutReg(0, Range(7, 5)),
               InReg(0, Range(15, 5)),
               UnsignedImm(0, Range(25, 5)),
               UnsignedImm(1, Range(20, 5)),
]
Format_I5U = [ OutReg(0, Range(7, 5)),
               InReg(1, Range(7, 5), dumpName=False),
               InReg(0, Range(15, 5)),
               UnsignedImm(0, Range(25, 5)),
               UnsignedImm(1, Range(20, 5)),
]
Format_I5U2 = [ OutReg(0, Range(7, 5)),
                InReg(1, Range(7, 5), dumpName=False),
                InReg(0, Range(15, 5)),
                InReg(2, Range(20, 5)),
]
Format_SPOST = [ InReg(1, Range(20, 5)),
                 Indirect(InReg(0, Range(15, 5)), SignedImm(0, Ranges([[7, 5, 0], [25, 7, 5]])), postInc=True),
]
Format_SRPOST = [ InReg(1, Range(20, 5)),
                  Indirect(InReg(0, Range(15, 5)), InReg(2, Range(7,5)), postInc=True),
]
Format_SB2 = [ InReg(0, Range(15, 5)),
               SignedImm(0, Ranges([[7, 1, 11], [8, 4, 1], [25, 6, 5], [31, 1, 12]])),
               SignedImm(1, Range(20, 5)),
]
Format_HL0 = [ UnsignedImm(0, Range(7, 1)),
               InReg (0, Range(15, 5)),
               UnsignedImm(1, Range(20, 12)),
]
Format_HL1 = [ UnsignedImm(0, Range(7, 1)),
               UnsignedImm(1, Range(20, 12)),
               UnsignedImm(2, Range(15, 5)),
]

class CoreV(IsaSubset):

    def __init__(self):
        super().__init__(name='corev', instrs=[

            Instr('LB_POSTINC',    Format_LPOST, '------- ----- ----- 000 ----- 0001011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_POSTINC',   Format_LPOST, '------- ----- ----- 100 ----- 0001011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_POSTINC',    Format_LPOST, '------- ----- ----- 001 ----- 0001011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_POSTINC',   Format_LPOST, '------- ----- ----- 101 ----- 0001011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_POSTINC',    Format_LPOST, '------- ----- ----- 010 ----- 0001011', L='cv.lw' , fast_handler=True, tags=["load"]),

            Instr('LB_RR_POSTINC',   Format_LRPOST,  '0000000 ----- ----- 111 ----- 0001011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR_POSTINC',  Format_LRPOST,  '0100000 ----- ----- 111 ----- 0001011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_RR_POSTINC',   Format_LRPOST,  '0001000 ----- ----- 111 ----- 0001011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_RR_POSTINC',  Format_LRPOST,  '0101000 ----- ----- 111 ----- 0001011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_RR_POSTINC',   Format_LRPOST,  '0010000 ----- ----- 111 ----- 0001011', L='cv.lw' , fast_handler=True, tags=["load"]),

            Instr('LB_RR',    Format_LR, '0000000 ----- ----- 111 ----- 0000011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR',   Format_LR, '0100000 ----- ----- 111 ----- 0000011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_RR',    Format_LR, '0001000 ----- ----- 111 ----- 0000011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_RR',   Format_LR, '0101000 ----- ----- 111 ----- 0000011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_RR',    Format_LR, '0010000 ----- ----- 111 ----- 0000011', L='cv.lw' , fast_handler=True, tags=["load"]),

            Instr('SB_POSTINC',    Format_SPOST, '------- ----- ----- 000 ----- 0101011', L='cv.sb' , fast_handler=True),
            Instr('SH_POSTINC',    Format_SPOST, '------- ----- ----- 001 ----- 0101011', L='cv.sh' , fast_handler=True),
            Instr('SW_POSTINC',    Format_SPOST, '------- ----- ----- 010 ----- 0101011', L='cv.sw' , fast_handler=True),

            Instr('SB_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 100 ----- 0101011',  L='cv.sb' , fast_handler=True),
            Instr('SH_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 101 ----- 0101011',  L='cv.sh' , fast_handler=True),
            Instr('SW_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 110 ----- 0101011',  L='cv.sw' , fast_handler=True),

            Instr('SB_RR',    Format_SR, '0000000 ----- ----- 100 ----- 0100011', L='cv.sb', fast_handler=True),
            Instr('SH_RR',    Format_SR, '0000000 ----- ----- 101 ----- 0100011', L='cv.sh', fast_handler=True),
            Instr('SW_RR',    Format_SR, '0000000 ----- ----- 110 ----- 0100011', L='cv.sw', fast_handler=True),

            Instr('cv.elw',           Format_L,   '------- ----- ----- 110 ----- 0000011', tags=["load"]),

            Instr('cv.starti',Format_HL0,'------- ----- 00000 000 0000- 1111011'),
            Instr('cv.endi',  Format_HL0,'------- ----- 00000 001 0000- 1111011'),
            Instr('cv.count', Format_HL0,'0000000 00000 ----- 010 0000- 1111011'),
            Instr('cv.counti',Format_HL0,'------- ----- 00000 011 0000- 1111011'),
            Instr('cv.setup', Format_HL0,'------- ----- ----- 100 0000- 1111011'),
            Instr('cv.setupi',Format_HL1,'------- ----- ----- 101 0000- 1111011'),

            Instr('cv.extract',      Format_I4U,  '11----- ----- ----- 000 ----- 0110011'),
            Instr('cv.extractu',     Format_I4U,  '11----- ----- ----- 001 ----- 0110011'),
            Instr('cv.insert',       Format_I5U,  '11----- ----- ----- 010 ----- 0110011'),
            Instr('cv.bclr',         Format_I4U,  '11----- ----- ----- 011 ----- 0110011'),
            Instr('cv.bset',         Format_I4U,  '11----- ----- ----- 100 ----- 0110011'),
            Instr('cv.extractr',     Format_R,    '1000000 ----- ----- 000 ----- 0110011'),
            Instr('cv.extractur',    Format_R,    '1000000 ----- ----- 001 ----- 0110011'),
            Instr('cv.insertr',      Format_I5U2, '1000000 ----- ----- 010 ----- 0110011'),
            Instr('cv.bclrr',        Format_R,    '1000000 ----- ----- 011 ----- 0110011'),
            Instr('cv.bsetr',        Format_R,    '1000000 ----- ----- 100 ----- 0110011'),
            Instr('cv.bitrev',       Format_BITREV,   '11000-- ----- ----- 101 ----- 0110011'),

            Instr('cv.ror', Format_R,  '0000100 ----- ----- 101 ----- 0110011'),
            Instr('cv.ff1', Format_R1,  '0001000 00000 ----- 000 ----- 0110011'),
            Instr('cv.fl1',Format_R1,   '0001000 00000 ----- 001 ----- 0110011'),
            Instr('cv.clb', Format_R1,  '0001000 00000 ----- 010 ----- 0110011'),
            Instr('cv.cnt', Format_R1,  '0001000 00000 ----- 011 ----- 0110011'),

            Instr('cv.abs',  Format_R1, '0000010 00000 ----- 000 ----- 0110011'),
            Instr('cv.slet',Format_R,  '0000010 ----- ----- 010 ----- 0110011'),
            Instr('cv.sletu',Format_R, '0000010 ----- ----- 011 ----- 0110011'),
            Instr('cv.min', Format_R,  '0000010 ----- ----- 100 ----- 0110011'),
            Instr('cv.minu',Format_R,  '0000010 ----- ----- 101 ----- 0110011'),
            Instr('cv.max', Format_R,  '0000010 ----- ----- 110 ----- 0110011'),
            Instr('cv.maxu',Format_R,  '0000010 ----- ----- 111 ----- 0110011'),
            Instr('cv.exths',Format_R1, '0001000 00000 ----- 100 ----- 0110011'),
            Instr('cv.exthz',Format_R1, '0001000 00000 ----- 101 ----- 0110011'),
            Instr('cv.extbs',Format_R1, '0001000 00000 ----- 110 ----- 0110011'),
            Instr('cv.extbz',Format_R1, '0001000 00000 ----- 111 ----- 0110011'),

            Instr('cv.clip',         Format_I1U,  '0001010 ----- ----- 001 ----- 0110011'),
            Instr('cv.clipu',        Format_I1U,  '0001010 ----- ----- 010 ----- 0110011'),
            Instr('cv.clipr',          Format_R,    '0001010 ----- ----- 101 ----- 0110011'),
            Instr('cv.clipur',         Format_R,    '0001010 ----- ----- 110 ----- 0110011'),

            Instr('cv.addN',         Format_RRRU2,'00----- ----- ----- 010 ----- 1011011'),
            Instr('cv.adduN',        Format_RRRU2,'10----- ----- ----- 010 ----- 1011011'),
            Instr('cv.addRN',        Format_RRRU2,'00----- ----- ----- 110 ----- 1011011'),
            Instr('cv.adduRN',       Format_RRRU2,'10----- ----- ----- 110 ----- 1011011'),

            Instr('cv.subN',         Format_RRRU2,'00----- ----- ----- 011 ----- 1011011'),
            Instr('cv.subuN',        Format_RRRU2,'10----- ----- ----- 011 ----- 1011011'),
            Instr('cv.subRN',        Format_RRRU2,'00----- ----- ----- 111 ----- 1011011'),
            Instr('cv.subuRN',       Format_RRRU2,'10----- ----- ----- 111 ----- 1011011'),

            Instr('cv.addNr',          Format_RRRR2,    '0100000 ----- ----- 010 ----- 1011011'),
            Instr('cv.adduNr',         Format_RRRR2,    '1100000 ----- ----- 010 ----- 1011011'),
            Instr('cv.addRNr',         Format_RRRR2,    '0100000 ----- ----- 110 ----- 1011011'),
            Instr('cv.adduRNr',        Format_RRRR2,    '1100000 ----- ----- 110 ----- 1011011'),

            Instr('cv.subNr',          Format_RRRR2,    '0100000 ----- ----- 011 ----- 1011011'),
            Instr('cv.subuNr',         Format_RRRR2,    '1100000 ----- ----- 011 ----- 1011011'),
            Instr('cv.subRNr',         Format_RRRR2,    '0100000 ----- ----- 111 ----- 1011011'),
            Instr('cv.subuRNr',        Format_RRRR2,    '1100000 ----- ----- 111 ----- 1011011'),

            Instr('cv.beqimm',        Format_SB2, '------- ----- ----- 010 ----- 1100011', fast_handler=True, decode='bxx_decode'),
            Instr('cv.bneimm',        Format_SB2, '------- ----- ----- 011 ----- 1100011', fast_handler=True, decode='bxx_decode'),

            Instr('cv.mac',           Format_RRRR, '0100001 ----- ----- 000 ----- 0110011'),
            Instr('cv.msu',           Format_RRRR, '0100001 ----- ----- 001 ----- 0110011'),

            Instr('cv.muls',          Format_R,    '1000000 ----- ----- 000 ----- 1011011'),
            Instr('cv.mulhhs',        Format_R,    '1100000 ----- ----- 000 ----- 1011011'),
            Instr('cv.mulsN',         Format_RRRU2,'10----- ----- ----- 000 ----- 1011011'),
            Instr('cv.mulhhsN',       Format_RRRU2,'11----- ----- ----- 000 ----- 1011011'),
            Instr('cv.mulsRN',        Format_RRRU2,'10----- ----- ----- 100 ----- 1011011'),
            Instr('cv.mulhhsRN',      Format_RRRU2,'11----- ----- ----- 100 ----- 1011011'),
            Instr('cv.mulu',          Format_R,    '0000000 ----- ----- 000 ----- 1011011'),
            Instr('cv.mulhhu',        Format_R,    '0100000 ----- ----- 000 ----- 1011011'),
            Instr('cv.muluN',         Format_RRRU2,'00----- ----- ----- 000 ----- 1011011'),
            Instr('cv.mulhhuN',       Format_RRRU2,'01----- ----- ----- 000 ----- 1011011'),
            Instr('cv.muluRN',        Format_RRRU2,'00----- ----- ----- 100 ----- 1011011'),
            Instr('cv.mulhhuRN',      Format_RRRU2,'01----- ----- ----- 100 ----- 1011011'),
            Instr('cv.macsN',         Format_RRRRU,'10----- ----- ----- 001 ----- 1011011'),
            Instr('cv.machhsN',       Format_RRRRU,'11----- ----- ----- 001 ----- 1011011'),
            Instr('cv.macsRN',        Format_RRRRU,'10----- ----- ----- 101 ----- 1011011'),
            Instr('cv.machhsRN',      Format_RRRRU,'11----- ----- ----- 101 ----- 1011011'),
            Instr('cv.macuN',         Format_RRRRU,'00----- ----- ----- 001 ----- 1011011'),
            Instr('cv.machhuN',       Format_RRRRU,'01----- ----- ----- 001 ----- 1011011'),
            Instr('cv.macuRN',        Format_RRRRU,'00----- ----- ----- 101 ----- 1011011'),
            Instr('cv.machhuRN',      Format_RRRRU,'01----- ----- ----- 101 ----- 1011011'),

        ])
