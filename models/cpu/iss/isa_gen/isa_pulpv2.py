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

#
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#

from cpu.iss.isa_gen.isa_gen import *
from cpu.iss.isa_gen.isa_riscv_gen import *

#
# PULP extensions
#

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
Format_RRRS = [ OutReg(0, Range(7,  5)),
                InReg (0, Range(7,  5)),
                InReg (1, Range(15, 5)),
                SignedImm(0, Ranges([[25, 1, 0], [20, 5, 1]])),
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
Format_RRU = [ OutReg(0, Range(7,  5)),
               InReg (0, Range(15, 5)),
               UnsignedImm(0, Ranges([[25, 1, 0], [20, 5, 1]])),
]
Format_RRU2 = [ OutReg(0, Range(7,  5)),
                InReg (0, Range(15, 5)),
                UnsignedImm(0, Ranges([[25, 1, 0], [20, 5, 1]])),
]
Format_RRS = [ OutReg(0, Range(7,  5)),
               InReg (0, Range(15, 5)),
               SignedImm(0, Ranges([[25, 1, 0], [20, 5, 1]])),
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

class PulpV2(IsaSubset):

    def __init__(self, hwloop=True, elw=True):
        instrs=[
            # Reg-reg LD/ST
            Instr('LB_RR',    Format_LR, '0000000 ----- ----- 111 ----- 0000011', L='p.lb' , fast_handler=True, tags=["load"]),
            Instr('LH_RR',    Format_LR, '0001000 ----- ----- 111 ----- 0000011', L='p.lh' , fast_handler=True, tags=["load"]),
            Instr('LW_RR',    Format_LR, '0010000 ----- ----- 111 ----- 0000011', L='p.lw' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR',   Format_LR, '0100000 ----- ----- 111 ----- 0000011', L='p.lbu', fast_handler=True, tags=["load"]),
            Instr('LHU_RR',   Format_LR, '0101000 ----- ----- 111 ----- 0000011', L='p.lhu', fast_handler=True, tags=["load"]),

            # Regular post-inc LD/ST
            Instr('LB_POSTINC',    Format_LPOST, '------- ----- ----- 000 ----- 0001011', L='p.lb' , fast_handler=True, tags=["load"]),
            Instr('LH_POSTINC',    Format_LPOST, '------- ----- ----- 001 ----- 0001011', L='p.lh' , fast_handler=True, tags=["load"]),
            Instr('LW_POSTINC',    Format_LPOST, '------- ----- ----- 010 ----- 0001011', L='p.lw' , fast_handler=True, tags=["load"]),
            Instr('LBU_POSTINC',   Format_LPOST, '------- ----- ----- 100 ----- 0001011', L='p.lbu', fast_handler=True, tags=["load"]),
            Instr('LHU_POSTINC',   Format_LPOST, '------- ----- ----- 101 ----- 0001011', L='p.lhu', fast_handler=True, tags=["load"]),
            Instr('SB_POSTINC',    Format_SPOST, '------- ----- ----- 000 ----- 0101011', L='p.sb' , fast_handler=True),
            Instr('SH_POSTINC',    Format_SPOST, '------- ----- ----- 001 ----- 0101011', L='p.sh' , fast_handler=True),
            Instr('SW_POSTINC',    Format_SPOST, '------- ----- ----- 010 ----- 0101011', L='p.sw' , fast_handler=True),

            # Reg-reg post-inc LD/ST
            Instr('LB_RR_POSTINC',   Format_LRPOST,  '0000000 ----- ----- 111 ----- 0001011', L='p.lb' , fast_handler=True, tags=["load"]),
            Instr('LH_RR_POSTINC',   Format_LRPOST,  '0001000 ----- ----- 111 ----- 0001011', L='p.lh' , fast_handler=True, tags=["load"]),
            Instr('LW_RR_POSTINC',   Format_LRPOST,  '0010000 ----- ----- 111 ----- 0001011', L='p.lw' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR_POSTINC',  Format_LRPOST,  '0100000 ----- ----- 111 ----- 0001011', L='p.lbu', fast_handler=True, tags=["load"]),
            Instr('LHU_RR_POSTINC',  Format_LRPOST,  '0101000 ----- ----- 111 ----- 0001011', L='p.lhu', fast_handler=True, tags=["load"]),
            Instr('SB_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 100 ----- 0101011',  L='p.sb' , fast_handler=True),
            Instr('SH_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 101 ----- 0101011',  L='p.sh' , fast_handler=True),
            Instr('SW_RR_POSTINC',   Format_SRPOST, '0000000 ----- ----- 110 ----- 0101011',  L='p.sw' , fast_handler=True),

            # Additional ALU operations
            Instr('p.avgu',Format_R,  '0000010 ----- ----- 001 ----- 0110011'),
            Instr('p.slet',Format_R,  '0000010 ----- ----- 010 ----- 0110011'),
            Instr('p.sletu',Format_R, '0000010 ----- ----- 011 ----- 0110011'),
            Instr('p.min', Format_R,  '0000010 ----- ----- 100 ----- 0110011'),
            Instr('p.minu',Format_R,  '0000010 ----- ----- 101 ----- 0110011'),
            Instr('p.max', Format_R,  '0000010 ----- ----- 110 ----- 0110011'),
            Instr('p.maxu',Format_R,  '0000010 ----- ----- 111 ----- 0110011'),
            Instr('p.ror', Format_R,  '0000100 ----- ----- 101 ----- 0110011'),
            Instr('p.ff1', Format_R1,  '0001000 00000 ----- 000 ----- 0110011'),
            Instr('p.fl1',Format_R1,   '0001000 00000 ----- 001 ----- 0110011'),
            Instr('p.clb', Format_R1,  '0001000 00000 ----- 010 ----- 0110011'),
            Instr('p.cnt', Format_R1,  '0001000 00000 ----- 011 ----- 0110011'),
            Instr('p.exths',Format_R1, '0001000 00000 ----- 100 ----- 0110011'),
            Instr('p.exthz',Format_R1, '0001000 00000 ----- 101 ----- 0110011'),
            Instr('p.extbs',Format_R1, '0001000 00000 ----- 110 ----- 0110011'),
            Instr('p.extbz',Format_R1, '0001000 00000 ----- 111 ----- 0110011'),

            Instr('p.abs',  Format_R1, '0000010 00000 ----- 000 ----- 0110011'),

            Instr('SB_RR',    Format_SR, '0000000 ----- ----- 100 ----- 0100011', L='p.sb', fast_handler=True),
            Instr('SH_RR',    Format_SR, '0000000 ----- ----- 101 ----- 0100011', L='p.sh', fast_handler=True),
            Instr('SW_RR',    Format_SR, '0000000 ----- ----- 110 ----- 0100011', L='p.sw', fast_handler=True),

            Instr('pv.add.h',        Format_R,   '000000- ----- ----- 000 ----- 1010111'),
            Instr('pv.add.sc.h',     Format_R,   '000000- ----- ----- 100 ----- 1010111'),
            Instr('pv.add.sci.h',    Format_RRS, '000000- ----- ----- 110 ----- 1010111'),
            Instr('pv.add.b',        Format_R,   '000000- ----- ----- 001 ----- 1010111'),
            Instr('pv.add.sc.b',     Format_R,   '000000- ----- ----- 101 ----- 1010111'),
            Instr('pv.add.sci.b',    Format_RRS, '000000- ----- ----- 111 ----- 1010111'),

            Instr('pv.sub.h',        Format_R,   '000010- ----- ----- 000 ----- 1010111'),
            Instr('pv.sub.sc.h',     Format_R,   '000010- ----- ----- 100 ----- 1010111'),
            Instr('pv.sub.sci.h',    Format_RRS, '000010- ----- ----- 110 ----- 1010111'),
            Instr('pv.sub.b',        Format_R,   '000010- ----- ----- 001 ----- 1010111'),
            Instr('pv.sub.sc.b',     Format_R,   '000010- ----- ----- 101 ----- 1010111'),
            Instr('pv.sub.sci.b',    Format_RRS, '000010- ----- ----- 111 ----- 1010111'),

            Instr('pv.avg.h',        Format_R,   '000100- ----- ----- 000 ----- 1010111'),
            Instr('pv.avg.sc.h',     Format_R,   '000100- ----- ----- 100 ----- 1010111'),
            Instr('pv.avg.sci.h',    Format_RRS, '000100- ----- ----- 110 ----- 1010111'),
            Instr('pv.avg.b',        Format_R,   '000100- ----- ----- 001 ----- 1010111'),
            Instr('pv.avg.sc.b',     Format_R,   '000100- ----- ----- 101 ----- 1010111'),
            Instr('pv.avg.sci.b',    Format_RRS, '000100- ----- ----- 111 ----- 1010111'),

            Instr('pv.avgu.h',       Format_R,   '000110- ----- ----- 000 ----- 1010111'),
            Instr('pv.avgu.sc.h',    Format_R,   '000110- ----- ----- 100 ----- 1010111'),
            Instr('pv.avgu.sci.h',   Format_RRU, '000110- ----- ----- 110 ----- 1010111'),
            Instr('pv.avgu.b',       Format_R,   '000110- ----- ----- 001 ----- 1010111'),
            Instr('pv.avgu.sc.b',    Format_R,   '000110- ----- ----- 101 ----- 1010111'),
            Instr('pv.avgu.sci.b',   Format_RRU, '000110- ----- ----- 111 ----- 1010111'),

            Instr('pv.min.h',        Format_R,   '001000- ----- ----- 000 ----- 1010111'),
            Instr('pv.min.sc.h',     Format_R,   '001000- ----- ----- 100 ----- 1010111'),
            Instr('pv.min.sci.h',    Format_RRS, '001000- ----- ----- 110 ----- 1010111'),
            Instr('pv.min.b',        Format_R,   '001000- ----- ----- 001 ----- 1010111'),
            Instr('pv.min.sc.b',     Format_R,   '001000- ----- ----- 101 ----- 1010111'),
            Instr('pv.min.sci.b',    Format_RRS, '001000- ----- ----- 111 ----- 1010111'),

            Instr('pv.minu.h',       Format_R,   '001010- ----- ----- 000 ----- 1010111'),
            Instr('pv.minu.sc.h',    Format_R,   '001010- ----- ----- 100 ----- 1010111'),
            Instr('pv.minu.sci.h',   Format_RRU, '001010- ----- ----- 110 ----- 1010111'),
            Instr('pv.minu.b',       Format_R,   '001010- ----- ----- 001 ----- 1010111'),
            Instr('pv.minu.sc.b',    Format_R,   '001010- ----- ----- 101 ----- 1010111'),
            Instr('pv.minu.sci.b',   Format_RRU, '001010- ----- ----- 111 ----- 1010111'),

            Instr('pv.max.h',        Format_R,   '001100- ----- ----- 000 ----- 1010111'),
            Instr('pv.max.sc.h',     Format_R,   '001100- ----- ----- 100 ----- 1010111'),
            Instr('pv.max.sci.h',    Format_RRS, '001100- ----- ----- 110 ----- 1010111'),
            Instr('pv.max.b',        Format_R,   '001100- ----- ----- 001 ----- 1010111'),
            Instr('pv.max.sc.b',     Format_R,   '001100- ----- ----- 101 ----- 1010111'),
            Instr('pv.max.sci.b',    Format_RRS, '001100- ----- ----- 111 ----- 1010111'),

            Instr('pv.maxu.h',       Format_R,   '001110- ----- ----- 000 ----- 1010111'),
            Instr('pv.maxu.sc.h',    Format_R,   '001110- ----- ----- 100 ----- 1010111'),
            Instr('pv.maxu.sci.h',   Format_RRU, '001110- ----- ----- 110 ----- 1010111'),
            Instr('pv.maxu.b',       Format_R,   '001110- ----- ----- 001 ----- 1010111'),
            Instr('pv.maxu.sc.b',    Format_R,   '001110- ----- ----- 101 ----- 1010111'),
            Instr('pv.maxu.sci.b',   Format_RRU, '001110- ----- ----- 111 ----- 1010111'),

            Instr('pv.srl.h',        Format_R,   '010000- ----- ----- 000 ----- 1010111'),
            Instr('pv.srl.sc.h',     Format_R,   '010000- ----- ----- 100 ----- 1010111'),
            Instr('pv.srl.sci.h',    Format_RRU, '010000- ----- ----- 110 ----- 1010111'),
            Instr('pv.srl.b',        Format_R,   '010000- ----- ----- 001 ----- 1010111'),
            Instr('pv.srl.sc.b',     Format_R,   '010000- ----- ----- 101 ----- 1010111'),
            Instr('pv.srl.sci.b',    Format_RRU, '010000- ----- ----- 111 ----- 1010111'),

            Instr('pv.sra.h',        Format_R,   '010010- ----- ----- 000 ----- 1010111'),
            Instr('pv.sra.sc.h',     Format_R,   '010010- ----- ----- 100 ----- 1010111'),
            Instr('pv.sra.sci.h',    Format_RRS, '010010- ----- ----- 110 ----- 1010111'),
            Instr('pv.sra.b',        Format_R,   '010010- ----- ----- 001 ----- 1010111'),
            Instr('pv.sra.sc.b',     Format_R,   '010010- ----- ----- 101 ----- 1010111'),
            Instr('pv.sra.sci.b',    Format_RRS, '010010- ----- ----- 111 ----- 1010111'),

            Instr('pv.sll.h',        Format_R,   '010100- ----- ----- 000 ----- 1010111'),
            Instr('pv.sll.sc.h',     Format_R,   '010100- ----- ----- 100 ----- 1010111'),
            Instr('pv.sll.sci.h',    Format_RRU, '010100- ----- ----- 110 ----- 1010111'),
            Instr('pv.sll.b',        Format_R,   '010100- ----- ----- 001 ----- 1010111'),
            Instr('pv.sll.sc.b',     Format_R,   '010100- ----- ----- 101 ----- 1010111'),
            Instr('pv.sll.sci.b',    Format_RRU, '010100- ----- ----- 111 ----- 1010111'),

            Instr('pv.or.h',         Format_R,   '010110- ----- ----- 000 ----- 1010111'),
            Instr('pv.or.sc.h',      Format_R,   '010110- ----- ----- 100 ----- 1010111'),
            Instr('pv.or.sci.h',     Format_RRS, '010110- ----- ----- 110 ----- 1010111'),
            Instr('pv.or.b',         Format_R,   '010110- ----- ----- 001 ----- 1010111'),
            Instr('pv.or.sc.b',      Format_R,   '010110- ----- ----- 101 ----- 1010111'),
            Instr('pv.or.sci.b',     Format_RRS, '010110- ----- ----- 111 ----- 1010111'),

            Instr('pv.xor.h',        Format_R,   '011000- ----- ----- 000 ----- 1010111'),
            Instr('pv.xor.sc.h',     Format_R,   '011000- ----- ----- 100 ----- 1010111'),
            Instr('pv.xor.sci.h',    Format_RRS, '011000- ----- ----- 110 ----- 1010111'),
            Instr('pv.xor.b',        Format_R,   '011000- ----- ----- 001 ----- 1010111'),
            Instr('pv.xor.sc.b',     Format_R,   '011000- ----- ----- 101 ----- 1010111'),
            Instr('pv.xor.sci.b',    Format_RRS, '011000- ----- ----- 111 ----- 1010111'),

            Instr('pv.and.h',        Format_R,   '011010- ----- ----- 000 ----- 1010111'),
            Instr('pv.and.sc.h',     Format_R,   '011010- ----- ----- 100 ----- 1010111'),
            Instr('pv.and.sci.h',    Format_RRS, '011010- ----- ----- 110 ----- 1010111'),
            Instr('pv.and.b',        Format_R,   '011010- ----- ----- 001 ----- 1010111'),
            Instr('pv.and.sc.b',     Format_R,   '011010- ----- ----- 101 ----- 1010111'),
            Instr('pv.and.sci.b',    Format_RRS, '011010- ----- ----- 111 ----- 1010111'),

            Instr('pv.abs.h',        Format_R1,  '0111000 ----- ----- 000 ----- 1010111'),
            Instr('pv.abs.b',        Format_R1,  '0111000 ----- ----- 001 ----- 1010111'),

            Instr('pv.extract.h',    Format_RRU, '011110- ----- ----- 110 ----- 1010111'),
            Instr('pv.extract.b',    Format_RRU, '011110- ----- ----- 111 ----- 1010111'),
            Instr('pv.extractu.h',   Format_RRU, '100100- ----- ----- 110 ----- 1010111'),
            Instr('pv.extractu.b',   Format_RRU, '100100- ----- ----- 111 ----- 1010111'),

            Instr('pv.insert.h',     Format_RRRU,'101100- ----- ----- 110 ----- 1010111'),
            Instr('pv.insert.b',     Format_RRRU,'101100- ----- ----- 111 ----- 1010111'),

            Instr('pv.dotsp.h',      Format_R,   '100110- ----- ----- 000 ----- 1010111'),
            Instr('pv.dotsp.h.sc',   Format_R,   '100110- ----- ----- 100 ----- 1010111'),
            Instr('pv.dotsp.h.sci',  Format_RRS, '100110- ----- ----- 110 ----- 1010111'),

            Instr('pv.dotsp.b',      Format_R,   '100110- ----- ----- 001 ----- 1010111'),
            Instr('pv.dotsp.b.sc',   Format_R,   '100110- ----- ----- 101 ----- 1010111'),
            Instr('pv.dotsp.b.sci',  Format_RRS, '100110- ----- ----- 111 ----- 1010111'),

            Instr('pv.dotup.h',      Format_R,   '100000- ----- ----- 000 ----- 1010111'),
            Instr('pv.dotup.h.sc',   Format_R,   '100000- ----- ----- 100 ----- 1010111'),
            Instr('pv.dotup.h.sci',  Format_RRU, '100000- ----- ----- 110 ----- 1010111'),

            Instr('pv.dotup.b',      Format_R,   '100000- ----- ----- 001 ----- 1010111'),
            Instr('pv.dotup.b.sc',   Format_R,   '100000- ----- ----- 101 ----- 1010111'),
            Instr('pv.dotup.b.sci',  Format_RRU, '100000- ----- ----- 111 ----- 1010111'),

            Instr('pv.dotusp.h',     Format_R,   '100010- ----- ----- 000 ----- 1010111'),
            Instr('pv.dotusp.h.sc',  Format_R,   '100010- ----- ----- 100 ----- 1010111'),
            Instr('pv.dotusp.h.sci', Format_RRS, '100010- ----- ----- 110 ----- 1010111'),

            Instr('pv.dotusp.b',     Format_R,   '100010- ----- ----- 001 ----- 1010111'),
            Instr('pv.dotusp.b.sc',  Format_R,   '100010- ----- ----- 101 ----- 1010111'),
            Instr('pv.dotusp.b.sci', Format_RRS, '100010- ----- ----- 111 ----- 1010111'),


            Instr('pv.sdotsp.h',     Format_RRRR,'101110- ----- ----- 000 ----- 1010111'),
            Instr('pv.sdotsp.h.sc',  Format_RRRR,'101110- ----- ----- 100 ----- 1010111'),
            Instr('pv.sdotsp.h.sci', Format_RRRS,'101110- ----- ----- 110 ----- 1010111'),

            Instr('pv.sdotsp.b',     Format_RRRR,'101110- ----- ----- 001 ----- 1010111'),
            Instr('pv.sdotsp.b.sc',  Format_RRRR,'101110- ----- ----- 101 ----- 1010111'),
            Instr('pv.sdotsp.b.sci', Format_RRRS,'101110- ----- ----- 111 ----- 1010111'),

            Instr('pv.sdotup.h',     Format_RRRR,'101000- ----- ----- 000 ----- 1010111'),
            Instr('pv.sdotup.h.sc',  Format_RRRR,'101000- ----- ----- 100 ----- 1010111'),
            Instr('pv.sdotup.h.sci', Format_RRRU,'101000- ----- ----- 110 ----- 1010111'),

            Instr('pv.sdotup.b',     Format_RRRR,'101000- ----- ----- 001 ----- 1010111'),
            Instr('pv.sdotup.b.sc',  Format_RRRR,'101000- ----- ----- 101 ----- 1010111'),
            Instr('pv.sdotup.b.sci', Format_RRRU,'101000- ----- ----- 111 ----- 1010111'),

            Instr('pv.sdotusp.h',    Format_RRRR,'101010- ----- ----- 000 ----- 1010111'),
            Instr('pv.sdotusp.h.sc', Format_RRRR,'101010- ----- ----- 100 ----- 1010111'),
            Instr('pv.sdotusp.h.sci',Format_RRRS,'101010- ----- ----- 110 ----- 1010111'),

            Instr('pv.sdotusp.b',    Format_RRRR,'101010- ----- ----- 001 ----- 1010111'),
            Instr('pv.sdotusp.b.sc', Format_RRRR,'101010- ----- ----- 101 ----- 1010111'),
            Instr('pv.sdotusp.b.sci',Format_RRRS,'101010- ----- ----- 111 ----- 1010111'),

            Instr('pv.shuffle.h',    Format_R,   '110000- ----- ----- 000 ----- 1010111'),
            Instr('pv.shuffle.h.sci',Format_RRU, '110000- ----- ----- 110 ----- 1010111'),

            Instr('pv.shuffle.b',    Format_R,   '110000- ----- ----- 001 ----- 1010111'),
            Instr('pv.shufflei0.b.sci',Format_RRU2,'110000- ----- ----- 111 ----- 1010111'),
            Instr('pv.shufflei1.b.sci',Format_RRU2,'111010- ----- ----- 111 ----- 1010111'),
            Instr('pv.shufflei2.b.sci',Format_RRU2,'111100- ----- ----- 111 ----- 1010111'),
            Instr('pv.shufflei3.b.sci',Format_RRU2,'111110- ----- ----- 111 ----- 1010111'),

            Instr('pv.shuffle2.h',   Format_RRRR,'110010- ----- ----- 000 ----- 1010111'),
            Instr('pv.shuffle2.b',   Format_RRRR,'110010- ----- ----- 001 ----- 1010111'),

            Instr('pv.pack.h',       Format_RRRR,'1101000 ----- ----- 000 ----- 1010111'),
            Instr('pv.packhi.b',     Format_RRRR,'110110- ----- ----- 001 ----- 1010111'),
            Instr('pv.packlo.b',     Format_RRRR,'111000- ----- ----- 001 ----- 1010111'),

            Instr('pv.cmpeq.h',      Format_R,   '000001- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpeq.sc.h',   Format_R,   '000001- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpeq.sci.h',  Format_RRS, '000001- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpeq.b',      Format_R,   '000001- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpeq.sc.b',   Format_R,   '000001- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpeq.sci.b',  Format_RRS, '000001- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpne.h',      Format_R,   '000011- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpne.sc.h',   Format_R,   '000011- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpne.sci.h',  Format_RRS, '000011- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpne.b',      Format_R,   '000011- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpne.sc.b',   Format_R,   '000011- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpne.sci.b',  Format_RRS, '000011- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpgt.h',      Format_R,   '000101- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpgt.sc.h',   Format_R,   '000101- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpgt.sci.h',  Format_RRS, '000101- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpgt.b',      Format_R,   '000101- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpgt.sc.b',   Format_R,   '000101- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpgt.sci.b',  Format_RRS, '000101- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpge.h',      Format_R,   '000111- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpge.sc.h',   Format_R,   '000111- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpge.sci.h',  Format_RRS, '000111- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpge.b',      Format_R,   '000111- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpge.sc.b',   Format_R,   '000111- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpge.sci.b',  Format_RRS, '000111- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmplt.h',      Format_R,   '001001- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmplt.sc.h',   Format_R,   '001001- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmplt.sci.h',  Format_RRS, '001001- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmplt.b',      Format_R,   '001001- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmplt.sc.b',   Format_R,   '001001- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmplt.sci.b',  Format_RRS, '001001- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmple.h',      Format_R,   '001011- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmple.sc.h',   Format_R,   '001011- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmple.sci.h',  Format_RRS, '001011- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmple.b',      Format_R,   '001011- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmple.sc.b',   Format_R,   '001011- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmple.sci.b',  Format_RRS, '001011- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpgtu.h',     Format_R,   '001101- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpgtu.sc.h',  Format_R,   '001101- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpgtu.sci.h', Format_RRU, '001101- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpgtu.b',     Format_R,   '001101- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpgtu.sc.b',  Format_R,   '001101- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpgtu.sci.b', Format_RRU, '001101- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpgeu.h',     Format_R,   '001111- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpgeu.sc.h',  Format_R,   '001111- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpgeu.sci.h', Format_RRU, '001111- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpgeu.b',     Format_R,   '001111- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpgeu.sc.b',  Format_R,   '001111- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpgeu.sci.b', Format_RRU, '001111- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpltu.h',     Format_R,   '010001- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpltu.sc.h',  Format_R,   '010001- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpltu.sci.h', Format_RRU, '010001- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpltu.b',     Format_R,   '010001- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpltu.sc.b',  Format_R,   '010001- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpltu.sci.b', Format_RRU, '010001- ----- ----- 111 ----- 1010111'),

            Instr('pv.cmpleu.h',     Format_R,   '010011- ----- ----- 000 ----- 1010111'),
            Instr('pv.cmpleu.sc.h',  Format_R,   '010011- ----- ----- 100 ----- 1010111'),
            Instr('pv.cmpleu.sci.h', Format_RRU, '010011- ----- ----- 110 ----- 1010111'),
            Instr('pv.cmpleu.b',     Format_R,   '010011- ----- ----- 001 ----- 1010111'),
            Instr('pv.cmpleu.sc.b',  Format_R,   '010011- ----- ----- 101 ----- 1010111'),
            Instr('pv.cmpleu.sci.b', Format_RRU, '010011- ----- ----- 111 ----- 1010111'),

            Instr('p.beqimm',        Format_SB2, '------- ----- ----- 010 ----- 1100011', tags=["branch"], fast_handler=True, decode='bxx_decode'),
            Instr('p.bneimm',        Format_SB2, '------- ----- ----- 011 ----- 1100011', tags=["branch"], fast_handler=True, decode='bxx_decode'),

            Instr('p.mac',           Format_RRRR, '0100001 ----- ----- 000 ----- 0110011'),
            Instr('p.msu',           Format_RRRR, '0100001 ----- ----- 001 ----- 0110011'),

            Instr('p.muls',          Format_R,    '1000000 ----- ----- 000 ----- 1011011'),
            Instr('p.mulhhs',        Format_R,    '1100000 ----- ----- 000 ----- 1011011'),
            Instr('p.mulsN',         Format_RRRU2,'10----- ----- ----- 000 ----- 1011011'),
            Instr('p.mulhhsN',       Format_RRRU2,'11----- ----- ----- 000 ----- 1011011'),
            Instr('p.mulsNR',        Format_RRRU2,'10----- ----- ----- 100 ----- 1011011'),
            Instr('p.mulhhsNR',      Format_RRRU2,'11----- ----- ----- 100 ----- 1011011'),

            Instr('p.mulu',          Format_R,    '0000000 ----- ----- 000 ----- 1011011'),
            Instr('p.mulhhu',        Format_R,    '0100000 ----- ----- 000 ----- 1011011'),
            Instr('p.muluN',         Format_RRRU2,'00----- ----- ----- 000 ----- 1011011'),
            Instr('p.mulhhuN',       Format_RRRU2,'01----- ----- ----- 000 ----- 1011011'),
            Instr('p.muluNR',        Format_RRRU2,'00----- ----- ----- 100 ----- 1011011'),
            Instr('p.mulhhuNR',      Format_RRRU2,'01----- ----- ----- 100 ----- 1011011'),

            Instr('p.macs',          Format_RRRR, '1000000 ----- ----- 001 ----- 1011011'),
            Instr('p.machhs',        Format_RRRR, '1100000 ----- ----- 001 ----- 1011011'),
            Instr('p.macsN',         Format_RRRRU,'10----- ----- ----- 001 ----- 1011011'),
            Instr('p.machhsN',       Format_RRRRU,'11----- ----- ----- 001 ----- 1011011'),
            Instr('p.macsNR',        Format_RRRRU,'10----- ----- ----- 101 ----- 1011011'),
            Instr('p.machhsNR',      Format_RRRRU,'11----- ----- ----- 101 ----- 1011011'),

            Instr('p.macu',          Format_RRRR, '0000000 ----- ----- 001 ----- 1011011'),
            Instr('p.machhu',        Format_RRRR, '0100000 ----- ----- 001 ----- 1011011'),
            Instr('p.macuN',         Format_RRRRU,'00----- ----- ----- 001 ----- 1011011'),
            Instr('p.machhuN',       Format_RRRRU,'01----- ----- ----- 001 ----- 1011011'),
            Instr('p.macuNR',        Format_RRRRU,'00----- ----- ----- 101 ----- 1011011'),
            Instr('p.machhuNR',      Format_RRRRU,'01----- ----- ----- 101 ----- 1011011'),

            Instr('p.addNi',         Format_RRRU2,'00----- ----- ----- 010 ----- 1011011'),
            Instr('p.adduNi',        Format_RRRU2,'10----- ----- ----- 010 ----- 1011011'),
            Instr('p.addRNi',        Format_RRRU2,'00----- ----- ----- 110 ----- 1011011'),
            Instr('p.adduRNi',       Format_RRRU2,'10----- ----- ----- 110 ----- 1011011'),

            Instr('p.subNi',         Format_RRRU2,'00----- ----- ----- 011 ----- 1011011'),
            Instr('p.subuNi',        Format_RRRU2,'10----- ----- ----- 011 ----- 1011011'),
            Instr('p.subRNi',        Format_RRRU2,'00----- ----- ----- 111 ----- 1011011'),
            Instr('p.subuRNi',       Format_RRRU2,'10----- ----- ----- 111 ----- 1011011'),

            Instr('p.addN',          Format_RRRR2,    '0100000 ----- ----- 010 ----- 1011011'),
            Instr('p.adduN',         Format_RRRR2,    '1100000 ----- ----- 010 ----- 1011011'),
            Instr('p.addRN',         Format_RRRR2,    '0100000 ----- ----- 110 ----- 1011011'),
            Instr('p.adduRN',        Format_RRRR2,    '1100000 ----- ----- 110 ----- 1011011'),

            Instr('p.subN',          Format_RRRR2,    '0100000 ----- ----- 011 ----- 1011011'),
            Instr('p.subuN',         Format_RRRR2,    '1100000 ----- ----- 011 ----- 1011011'),
            Instr('p.subRN',         Format_RRRR2,    '0100000 ----- ----- 111 ----- 1011011'),
            Instr('p.subuRN',        Format_RRRR2,    '1100000 ----- ----- 111 ----- 1011011'),

            Instr('p.clipi',         Format_I1U,  '0001010 ----- ----- 001 ----- 0110011'),
            Instr('p.clipui',        Format_I1U,  '0001010 ----- ----- 010 ----- 0110011'),
            Instr('p.clip',          Format_R,    '0001010 ----- ----- 101 ----- 0110011'),
            Instr('p.clipu',         Format_R,    '0001010 ----- ----- 110 ----- 0110011'),

            Instr('p.extracti',      Format_I4U,  '11----- ----- ----- 000 ----- 0110011'),
            Instr('p.extractui',     Format_I4U,  '11----- ----- ----- 001 ----- 0110011'),
            Instr('p.extract',       Format_R,    '1000000 ----- ----- 000 ----- 0110011'),
            Instr('p.extractu',      Format_R,    '1000000 ----- ----- 001 ----- 0110011'),
            Instr('p.inserti',       Format_I5U,  '11----- ----- ----- 010 ----- 0110011'),
            Instr('p.insert',        Format_I5U2, '1000000 ----- ----- 010 ----- 0110011'),
            Instr('p.bseti',         Format_I4U,  '11----- ----- ----- 100 ----- 0110011'),
            Instr('p.bclri',         Format_I4U,  '11----- ----- ----- 011 ----- 0110011'),
            Instr('p.bset',          Format_R,    '1000000 ----- ----- 100 ----- 0110011'),
            Instr('p.bclr',          Format_R,    '1000000 ----- ----- 011 ----- 0110011'),

        ]

        # HW loops
        if hwloop:
            instrs += [
                Instr('lp.starti',Format_HL0,'------- ----- ----- 000 0000- 1111011'),
                Instr('lp.endi',  Format_HL0,'------- ----- ----- 001 0000- 1111011'),
                Instr('lp.count', Format_HL0,'------- ----- ----- 010 0000- 1111011'),
                Instr('lp.counti',Format_HL0,'------- ----- ----- 011 0000- 1111011'),
                Instr('lp.setup', Format_HL0,'------- ----- ----- 100 0000- 1111011'),
                Instr('lp.setupi',Format_HL1,'------- ----- ----- 101 0000- 1111011'),
            ]

        if elw:
            instrs += [
                Instr('p.elw',           Format_L,   '------- ----- ----- 110 ----- 0000011', tags=["load"]),
            ]

        super().__init__(name='pulpv2', instrs=instrs, includes=[
            '<cpu/iss/include/isa/pulp_v2.hpp>',
        ])
