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

Format_RF4 = [ OutFReg(0, Range(7,  5)),
                    InFReg (2, Range(7,  5), dumpName=False),
                    InFReg (0, Range(15, 5)),
                    InFReg (1, Range(20, 5)),
                    UnsignedImm(0, Range(12, 3)),
]
Format_RVF = [ OutFReg(0, Range(7,  5)),
               InFReg (0, Range(15, 5)),
               InFReg (1, Range(20, 5)),
]
Format_RVF2 = [ OutFReg(0, Range(7,  5)),
                InFReg (0, Range(15, 5)),
]
Format_RVF4 = [ OutFReg(0, Range(7,  5)),
                InFReg (2, Range(7,  5), dumpName=False),
                InFReg (0, Range(15, 5)),
                InFReg (1, Range(20, 5)),
]
Format_R2VF = [ OutReg(0, Range(7,  5)),
                InFReg (0, Range(15, 5)),
                InFReg (1, Range(20, 5)),
]
Format_R2VF2 = [ OutReg(0, Range(7,  5)),
                 InFReg (0, Range(15, 5)),
]

class Xf16(IsaSubset):

    def __init__(self):
        super().__init__(name='f16', active=False, instrs=[

            Instr('flh',       Format_FL, '------- ----- ----- 001 ----- 0000111', tags=["load"]),
            Instr('fsh',       Format_FS, '------- ----- ----- 001 ----- 0100111'),

            Instr('fmadd.h',   Format_R4U,'-----10 ----- ----- --- ----- 1000011', tags=['sfmadd']),
            Instr('fmsub.h',   Format_R4U,'-----10 ----- ----- --- ----- 1000111', tags=['sfmadd']),
            Instr('fnmsub.h',  Format_R4U,'-----10 ----- ----- --- ----- 1001011', tags=['sfmadd']),
            Instr('fnmadd.h',  Format_R4U,'-----10 ----- ----- --- ----- 1001111', tags=['sfmadd']),

            Instr('fadd.h',    Format_RF, '0000010 ----- ----- --- ----- 1010011', tags=['sfadd']),
            Instr('fsub.h',    Format_RF, '0000110 ----- ----- --- ----- 1010011', tags=['sfadd']),
            Instr('fmul.h',    Format_RF, '0001010 ----- ----- --- ----- 1010011', tags=['sfmul']),
            Instr('fdiv.h',    Format_RF, '0001110 ----- ----- --- ----- 1010011', tags=['sfdiv']),
            Instr('fsqrt.h',  Format_R2F3,'0101110 00000 ----- --- ----- 1010011', tags=['sfdiv']),

            Instr('fsgnj.h',   Format_RF, '0010010 ----- ----- 000 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjn.h',  Format_RF, '0010010 ----- ----- 001 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjx.h',  Format_RF, '0010010 ----- ----- 010 ----- 1010011', tags=['sfconv']),

            Instr('fmin.h',    Format_RF, '0010110 ----- ----- 000 ----- 1010011', tags=['sfconv']),
            Instr('fmax.h',    Format_RF, '0010110 ----- ----- 001 ----- 1010011', tags=['sfconv']),

            Instr('feq.h',    Format_RF2, '1010010 ----- ----- 010 ----- 1010011', tags=['sfother']),
            Instr('flt.h',    Format_RF2, '1010010 ----- ----- 001 ----- 1010011', tags=['sfother']),
            Instr('fle.h',    Format_RF2, '1010010 ----- ----- 000 ----- 1010011', tags=['sfother']),

            Instr('fcvt.w.h', Format_R2F1,'1100010 00000 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.wu.h',Format_R2F1,'1100010 00001 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.h.w', Format_R2F2,'1101010 00000 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.h.wu',Format_R2F2,'1101010 00001 ----- --- ----- 1010011', tags=['sfconv']),

            Instr('fmv.x.h',   Format_R3F,'1110010 00000 ----- 000 ----- 1010011', tags=['sfother']),
            Instr('fclass.h',  Format_R3F,'1110010 00000 ----- 001 ----- 1010011', tags=['sfother']),
            Instr('fmv.h.x',  Format_R3F2,'1111010 00000 ----- 000 ----- 1010011', tags=['sfother']),

            # If RV64Xf16 supported
            Instr('fcvt.l.h', Format_R2F1,'1100010 00010 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16']),
            Instr('fcvt.lu.h',Format_R2F1,'1100010 00011 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16']),
            Instr('fcvt.h.l', Format_R2F2,'1101010 00010 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16']),
            Instr('fcvt.h.lu',Format_R2F2,'1101010 00011 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16']),

            # If F also supported
            Instr('fcvt.s.h', Format_R2F3,'0100000 00010 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16f']),
            Instr('fcvt.h.s', Format_R2F3,'0100010 00000 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f16f']),

            # # If D also supported
            # Instr('fcvt.d.h', Format_R2F3,'0100001 00010 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16d']),
            # Instr('fcvt.h.d', Format_R2F3,'0100010 00001 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f16d']),
        ])

class Xf16alt(IsaSubset):
    
    def __init__(self):
        super().__init__(name='f16alt', active=False, instrs=[
            Instr('fmadd.ah',   Format_R4U,'-----10 ----- ----- 101 ----- 1000011', tags=['sfmadd']),
            Instr('fmsub.ah',   Format_R4U,'-----10 ----- ----- 101 ----- 1000111', tags=['sfmadd']),
            Instr('fnmsub.ah',  Format_R4U,'-----10 ----- ----- 101 ----- 1001011', tags=['sfmadd']),
            Instr('fnmadd.ah',  Format_R4U,'-----10 ----- ----- 101 ----- 1001111', tags=['sfmadd']),

            Instr('fadd.ah',    Format_RF, '0000010 ----- ----- 101 ----- 1010011', tags=['sfadd']),
            Instr('fsub.ah',    Format_RF, '0000110 ----- ----- 101 ----- 1010011', tags=['sfadd']),
            Instr('fmul.ah',    Format_RF, '0001010 ----- ----- 101 ----- 1010011', tags=['sfmul']),
            Instr('fdiv.ah',    Format_RF, '0001110 ----- ----- 101 ----- 1010011', tags=['sfdiv']),
            Instr('fsqrt.ah',  Format_R2F3,'0101110 00000 ----- 101 ----- 1010011', tags=['sfdiv']),

            Instr('fsgnj.ah',   Format_RF, '0010010 ----- ----- 100 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjn.ah',  Format_RF, '0010010 ----- ----- 101 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjx.ah',  Format_RF, '0010010 ----- ----- 110 ----- 1010011', tags=['sfconv']),

            Instr('fmin.ah',    Format_RF, '0010110 ----- ----- 100 ----- 1010011', tags=['sfconv']),
            Instr('fmax.ah',    Format_RF, '0010110 ----- ----- 101 ----- 1010011', tags=['sfconv']),

            Instr('feq.ah',    Format_RF2, '1010010 ----- ----- 110 ----- 1010011', tags=['sfother']),
            Instr('flt.ah',    Format_RF2, '1010010 ----- ----- 101 ----- 1010011', tags=['sfother']),
            Instr('fle.ah',    Format_RF2, '1010010 ----- ----- 100 ----- 1010011', tags=['sfother']),

            Instr('fcvt.w.ah', Format_R2F1,'1100010 00000 ----- 101 ----- 1010011', tags=['sfconv']),
            Instr('fcvt.wu.ah',Format_R2F1,'1100010 00001 ----- 101 ----- 1010011', tags=['sfconv']),
            Instr('fcvt.ah.w', Format_R2F2,'1101010 00000 ----- 101 ----- 1010011', tags=['sfconv']),
            Instr('fcvt.ah.wu',Format_R2F2,'1101010 00001 ----- 101 ----- 1010011', tags=['sfconv']),

            Instr('fmv.x.ah',   Format_R3F,'1110010 00000 ----- 100 ----- 1010011', tags=['sfother']),
            Instr('fclass.ah',  Format_R3F,'1110010 00000 ----- 101 ----- 1010011', tags=['sfother']),
            Instr('fmv.ah.x',  Format_R3F2,'1111010 00000 ----- 100 ----- 1010011', tags=['sfother']),

            # If RV64Xf16alt supported
            Instr('fcvt.l.ah', Format_R2F1,'1100010 00010 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16alt']),
            Instr('fcvt.lu.ah',Format_R2F1,'1100010 00011 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16alt']),
            Instr('fcvt.ah.l', Format_R2F2,'1101010 00010 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16alt']),
            Instr('fcvt.ah.lu',Format_R2F2,'1101010 00011 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['rv64f16alt']),

            # If F also supported
            Instr('fcvt.s.ah', Format_R2F3,'0100000 00110 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16altf']),
            Instr('fcvt.ah.s', Format_R2F3,'0100010 00000 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['f16altf']),

            # # If D also supported
            # Instr('fcvt.d.ah', Format_R2F3,'0100001 00110 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16altd']),
            # Instr('fcvt.ah.d', Format_R2F3,'0100010 00001 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['f16altd']),

            # If Xf16 also supported
            Instr('fcvt.h.ah', Format_R2F3,'0100010 00110 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f16altf16']),
            Instr('fcvt.ah.h', Format_R2F3,'0100010 00010 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['f16altf16']),
        ])

class Xf8(IsaSubset):
    
    def __init__(self):
        super().__init__(name='f8', active=False, instrs=[
            Instr('flb',       Format_FL, '------- ----- ----- 000 ----- 0000111', tags=["load"]),
            Instr('fsb',       Format_FS, '------- ----- ----- 000 ----- 0100111'),

            Instr('fmadd.b',   Format_R4U,'-----11 ----- ----- --- ----- 1000011', tags=['sfmadd']),
            Instr('fmsub.b',   Format_R4U,'-----11 ----- ----- --- ----- 1000111', tags=['sfmadd']),
            Instr('fnmsub.b',  Format_R4U,'-----11 ----- ----- --- ----- 1001011', tags=['sfmadd']),
            Instr('fnmadd.b',  Format_R4U,'-----11 ----- ----- --- ----- 1001111', tags=['sfmadd']),

            Instr('fadd.b',    Format_RF, '0000011 ----- ----- --- ----- 1010011', tags=['sfadd']),
            Instr('fsub.b',    Format_RF, '0000111 ----- ----- --- ----- 1010011', tags=['sfadd']),
            Instr('fmul.b',    Format_RF, '0001011 ----- ----- --- ----- 1010011', tags=['sfmul']),
            Instr('fdiv.b',    Format_RF, '0001111 ----- ----- --- ----- 1010011', tags=['sfdiv']),
            Instr('fsqrt.b',  Format_R2F3,'0101111 00000 ----- --- ----- 1010011', tags=['sfdiv']),

            Instr('fsgnj.b',   Format_RF, '0010011 ----- ----- 000 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjn.b',  Format_RF, '0010011 ----- ----- 001 ----- 1010011', tags=['sfconv']),
            Instr('fsgnjx.b',  Format_RF, '0010011 ----- ----- 010 ----- 1010011', tags=['sfconv']),

            Instr('fmin.b',    Format_RF, '0010111 ----- ----- 000 ----- 1010011', tags=['sfconv']),
            Instr('fmax.b',    Format_RF, '0010111 ----- ----- 001 ----- 1010011', tags=['sfconv']),

            Instr('feq.b',    Format_RF2, '1010011 ----- ----- 010 ----- 1010011', tags=['sfother']),
            Instr('flt.b',    Format_RF2, '1010011 ----- ----- 001 ----- 1010011', tags=['sfother']),
            Instr('fle.b',    Format_RF2, '1010011 ----- ----- 000 ----- 1010011', tags=['sfother']),

            Instr('fcvt.w.b', Format_R2F1,'1100011 00000 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.wu.b',Format_R2F1,'1100011 00001 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.b.w', Format_R2F2,'1101011 00000 ----- --- ----- 1010011', tags=['sfconv']),
            Instr('fcvt.b.wu',Format_R2F2,'1101011 00001 ----- --- ----- 1010011', tags=['sfconv']),

            Instr('fmv.x.b',   Format_R3F,'1110011 00000 ----- 000 ----- 1010011', tags=['sfother']),
            Instr('fclass.b',  Format_R3F,'1110011 00000 ----- 001 ----- 1010011', tags=['sfother']),
            Instr('fmv.b.x',  Format_R3F2,'1111011 00000 ----- 000 ----- 1010011', tags=['sfother']),

            # If RV64Xf8 supported
            Instr('fcvt.l.b', Format_R2F1,'1100011 00010 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f8']),
            Instr('fcvt.lu.b',Format_R2F1,'1100011 00011 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f8']),
            Instr('fcvt.b.l', Format_R2F2,'1101011 00010 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f8']),
            Instr('fcvt.b.lu',Format_R2F2,'1101011 00011 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['rv64f8']),

            # If F also supported
            Instr('fcvt.s.b', Format_R2F3,'0100000 00011 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f8f']),
            Instr('fcvt.b.s', Format_R2F3,'0100011 00000 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f8f']),

            # # If D also supported
            # Instr('fcvt.d.b', Format_R2F3,'0100001 00011 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f8d']),
            # Instr('fcvt.b.d', Format_R2F3,'0100011 00001 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f8d']),

            # If Xf16 also supported
            Instr('fcvt.h.b', Format_R2F3,'0100010 00011 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f8f16']),
            Instr('fcvt.b.h', Format_R2F3,'0100011 00010 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f8f16']),

            # If Xf16alt also supported
            Instr('fcvt.ah.b',Format_R2F3,'0100010 00011 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['f8f16alt']),
            Instr('fcvt.b.ah',Format_R2F3,'0100011 00110 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f8f16alt']),
        ])


#
# Vectorial Floats
#

class Xfvec(IsaSubset):
    
    def __init__(self):
        super().__init__(name='fvec', active=False, instrs=[
        #
        # For F
        #
            # Instr('vfadd.s',    Format_RVF, '1000001 ----- ----- 000 ----- 0110011', tags=['fadd'], isa_tags=['f32vec']),
            # Instr('vfadd.r.s',  Format_RVF, '1000001 ----- ----- 100 ----- 0110011', tags=['fadd'], isa_tags=['f32vec']),
            # Instr('vfsub.s',    Format_RVF, '1000010 ----- ----- 000 ----- 0110011', tags=['fadd'], isa_tags=['f32vec']),
            # Instr('vfsub.r.s',  Format_RVF, '1000010 ----- ----- 100 ----- 0110011', tags=['fadd'], isa_tags=['f32vec']),
            # Instr('vfmul.s',    Format_RVF, '1000011 ----- ----- 000 ----- 0110011', tags=['fmul'], isa_tags=['f32vec']),
            # Instr('vfmul.r.s',  Format_RVF, '1000011 ----- ----- 100 ----- 0110011', tags=['fmul'], isa_tags=['f32vec']),
            # Instr('vfdiv.s',    Format_RVF, '1000100 ----- ----- 000 ----- 0110011', tags=['fdiv'], isa_tags=['f32vec']),
            # Instr('vfdiv.r.s',  Format_RVF, '1000100 ----- ----- 100 ----- 0110011', tags=['fdiv'], isa_tags=['f32vec']),

            # Instr('vfmin.s',    Format_RVF, '1000101 ----- ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfmin.r.s',  Format_RVF, '1000101 ----- ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfmax.s',    Format_RVF, '1000110 ----- ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfmax.r.s',  Format_RVF, '1000110 ----- ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),

            # Instr('vfsqrt.s',   Format_RVF2,'1000111 00000 ----- 000 ----- 0110011', tags=['fdiv'], isa_tags=['f32vec']),

            # Instr('vfmac.s',    Format_RVF4,'1001000 ----- ----- 000 ----- 0110011', tags=['fmadd'], isa_tags=['f32vec']),
            # Instr('vfmac.r.s',  Format_RVF4,'1001000 ----- ----- 100 ----- 0110011', tags=['fmadd'], isa_tags=['f32vec']),
            # Instr('vfmre.s',    Format_RVF4,'1001001 ----- ----- 000 ----- 0110011', tags=['fmadd'], isa_tags=['f32vec']),
            # Instr('vfmre.r.s',  Format_RVF4,'1001001 ----- ----- 100 ----- 0110011', tags=['fmadd'], isa_tags=['f32vec']),

            # Instr('vfclass.s', Format_R2VF2,'1001100 00001 ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),

            # Instr('vfsgnj.r.s', Format_RVF, '1001101 ----- ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfsgnj.s',   Format_RVF, '1001101 ----- ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfsgnjn.s',  Format_RVF, '1001110 ----- ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfsgnjn.r.s',Format_RVF, '1001110 ----- ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfsgnjx.s',  Format_RVF, '1001111 ----- ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),
            # Instr('vfsgnjx.r.s',Format_RVF, '1001111 ----- ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vec']),

            # Instr('vfeq.s',    Format_R2VF, '1010000 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfeq.r.s',  Format_R2VF, '1010000 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfne.s',    Format_R2VF, '1010001 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfne.r.s',  Format_R2VF, '1010001 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vflt.s',    Format_R2VF, '1010010 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vflt.r.s',  Format_R2VF, '1010010 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfge.s',    Format_R2VF, '1010011 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfge.r.s',  Format_R2VF, '1010011 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfle.s',    Format_R2VF, '1010100 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfle.r.s',  Format_R2VF, '1010100 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfgt.s',    Format_R2VF, '1010101 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # Instr('vfgt.r.s',  Format_R2VF, '1010101 ----- ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),

            # Instr('vfcpka.s.s', Format_RVF, '1011000 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),
            # # Instr('vfcpka.s.d', Format_RVF, '1011010 ----- ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vec']),

            # # Unless RV32D supported
            # Instr('vfmv.x.s',   Format_R3F, '1001100 00000 ----- 000 ----- 0110011', tags=['fother'], isa_tags=['f32vecno32d']),
            # Instr('vfmv.s.x',   Format_R3F2,'1001100 00000 ----- 100 ----- 0110011', tags=['fother'], isa_tags=['f32vecno32d']),

            # Instr('vfcvt.x.s',  Format_R3F, '1001100 00010 ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.xu.s', Format_R3F, '1001100 00010 ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.s.x',  Format_R3F2,'1001100 00011 ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.s.xu', Format_R3F2,'1001100 00011 ----- 100 ----- 0110011', tags=['fconv'], isa_tags=['f32vecno32d']),

        #
        # For Xf16
        #
            Instr('vfadd.h',    Format_RVF, '1000001 ----- ----- 010 ----- 0110011', tags=['fadd'], isa_tags=['f16vec']),
            Instr('vfadd.r.h',  Format_RVF, '1000001 ----- ----- 110 ----- 0110011', tags=['fadd'], isa_tags=['f16vec']),
            Instr('vfsub.h',    Format_RVF, '1000010 ----- ----- 010 ----- 0110011', tags=['fadd'], isa_tags=['f16vec']),
            Instr('vfsub.r.h',  Format_RVF, '1000010 ----- ----- 110 ----- 0110011', tags=['fadd'], isa_tags=['f16vec']),
            Instr('vfmul.h',    Format_RVF, '1000011 ----- ----- 010 ----- 0110011', tags=['fmul'], isa_tags=['f16vec']),
            Instr('vfmul.r.h',  Format_RVF, '1000011 ----- ----- 110 ----- 0110011', tags=['fmul'], isa_tags=['f16vec']),
            Instr('vfdiv.h',    Format_RVF, '1000100 ----- ----- 010 ----- 0110011', tags=['fdiv'], isa_tags=['f16vec']),
            Instr('vfdiv.r.h',  Format_RVF, '1000100 ----- ----- 110 ----- 0110011', tags=['fdiv'], isa_tags=['f16vec']),

            Instr('vfmin.h',    Format_RVF, '1000101 ----- ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfmin.r.h',  Format_RVF, '1000101 ----- ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfmax.h',    Format_RVF, '1000110 ----- ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfmax.r.h',  Format_RVF, '1000110 ----- ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),

            Instr('vfsqrt.h',   Format_RVF2,'1000111 00000 ----- 010 ----- 0110011', tags=['fdiv'], isa_tags=['f16vec']),

            Instr('vfmac.h',    Format_RVF4,'1001000 ----- ----- 010 ----- 0110011', tags=['fmadd'], isa_tags=['f16vec']),
            Instr('vfmac.r.h',  Format_RVF4,'1001000 ----- ----- 110 ----- 0110011', tags=['fmadd'], isa_tags=['f16vec']),
            Instr('vfmre.h',    Format_RVF4,'1001001 ----- ----- 010 ----- 0110011', tags=['fmadd'], isa_tags=['f16vec']),
            Instr('vfmre.r.h',  Format_RVF4,'1001001 ----- ----- 110 ----- 0110011', tags=['fmadd'], isa_tags=['f16vec']),

            Instr('vfclass.h', Format_R2VF2,'1001100 00001 ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),

            Instr('vfsgnj.h',   Format_RVF, '1001101 ----- ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfsgnj.r.h', Format_RVF, '1001101 ----- ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfsgnjn.h',  Format_RVF, '1001110 ----- ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfsgnjn.r.h',Format_RVF, '1001110 ----- ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfsgnjx.h',  Format_RVF, '1001111 ----- ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),
            Instr('vfsgnjx.r.h',Format_RVF, '1001111 ----- ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vec']),

            Instr('vfeq.h',    Format_R2VF, '1010000 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfeq.r.h',  Format_R2VF, '1010000 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfne.h',    Format_R2VF, '1010001 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfne.r.h',  Format_R2VF, '1010001 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vflt.h',    Format_R2VF, '1010010 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vflt.r.h',  Format_R2VF, '1010010 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfge.h',    Format_R2VF, '1010011 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfge.r.h',  Format_R2VF, '1010011 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfle.h',    Format_R2VF, '1010100 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfle.r.h',  Format_R2VF, '1010100 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfgt.h',    Format_R2VF, '1010101 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),
            Instr('vfgt.r.h',  Format_R2VF, '1010101 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),

            Instr('vfcpka.h.s', Format_R2VF,'1011000 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vec']),

            # Unless RV32D supported
            Instr('vfmv.x.h',   Format_R3F, '1001100 00000 ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vecno32d']),
            Instr('vfmv.h.x',   Format_R3F2,'1001100 00000 ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vecno32d']),

            Instr('vfcvt.x.h',  Format_R3F, '1001100 00010 ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vecno32d']),
            Instr('vfcvt.xu.h', Format_R3F, '1001100 00010 ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vecno32d']),
            Instr('vfcvt.h.x',  Format_R3F2,'1001100 00011 ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vecno32d']),
            Instr('vfcvt.h.xu', Format_R3F2,'1001100 00011 ----- 110 ----- 0110011', tags=['fconv'], isa_tags=['f16vecno32d']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.h',  Format_RVF2,'1001100 00110 ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f16vecd']),
            # Instr('vfcvt.h.s',  Format_RVF2,'1001100 00100 ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16vecd']),

            # Instr('vfcpkb.h.s', Format_RVF4,'1011000 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vecd']),
            # Instr('vfcpka.h.d', Format_RVF4,'1011010 ----- ----- 010 ----- 0110011', tags=['fother'], isa_tags=['f16vecd']),
            # Instr('vfcpkb.h.d', Format_RVF4,'1011010 ----- ----- 110 ----- 0110011', tags=['fother'], isa_tags=['f16vecd']),

        #
        # For Xf16alt
        #
            Instr('vfadd.ah',    Format_RVF, '1000001 ----- ----- 001 ----- 0110011', tags=['fadd'], isa_tags=['f16altvec']),
            Instr('vfadd.r.ah',  Format_RVF, '1000001 ----- ----- 101 ----- 0110011', tags=['fadd'], isa_tags=['f16altvec']),
            Instr('vfsub.ah',    Format_RVF, '1000010 ----- ----- 001 ----- 0110011', tags=['fadd'], isa_tags=['f16altvec']),
            Instr('vfsub.r.ah',  Format_RVF, '1000010 ----- ----- 101 ----- 0110011', tags=['fadd'], isa_tags=['f16altvec']),
            Instr('vfmul.ah',    Format_RVF, '1000011 ----- ----- 001 ----- 0110011', tags=['fmul'], isa_tags=['f16altvec']),
            Instr('vfmul.r.ah',  Format_RVF, '1000011 ----- ----- 101 ----- 0110011', tags=['fmul'], isa_tags=['f16altvec']),
            Instr('vfdiv.ah',    Format_RVF, '1000100 ----- ----- 001 ----- 0110011', tags=['fdiv'], isa_tags=['f16altvec']),
            Instr('vfdiv.r.ah',  Format_RVF, '1000100 ----- ----- 101 ----- 0110011', tags=['fdiv'], isa_tags=['f16altvec']),

            Instr('vfmin.ah',    Format_RVF, '1000101 ----- ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfmin.r.ah',  Format_RVF, '1000101 ----- ----- 101 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfmax.ah',    Format_RVF, '1000110 ----- ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfmax.r.ah',  Format_RVF, '1000110 ----- ----- 101 ----- 0110011', tags=['fconv']),

            Instr('vfsqrt.ah',   Format_RVF2,'1000111 00000 ----- 001 ----- 0110011', tags=['fdiv'], isa_tags=['f16altvec']),

            Instr('vfmac.ah',    Format_RVF4,'1001000 ----- ----- 001 ----- 0110011', tags=['fmadd'], isa_tags=['f16altvec']),
            Instr('vfmac.r.ah',  Format_RVF4,'1001000 ----- ----- 101 ----- 0110011', tags=['fmadd'], isa_tags=['f16altvec']),
            Instr('vfmre.ah',    Format_RVF4,'1001001 ----- ----- 001 ----- 0110011', tags=['fmadd'], isa_tags=['f16altvec']),
            Instr('vfmre.r.ah',  Format_RVF4,'1001001 ----- ----- 101 ----- 0110011', tags=['fmadd'], isa_tags=['f16altvec']),

            Instr('vfclass.ah', Format_R2VF2,'1001100 00001 ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),

            Instr('vfsgnj.r.ah', Format_RVF, '1001101 ----- ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfsgnj.ah',   Format_RVF, '1001101 ----- ----- 101 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfsgnjn.ah',  Format_RVF, '1001110 ----- ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfsgnjn.r.ah',Format_RVF, '1001110 ----- ----- 101 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfsgnjx.ah',  Format_RVF, '1001111 ----- ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvec']),
            Instr('vfsgnjx.r.ah',Format_RVF, '1001111 ----- ----- 101 ----- 0110011', tags=['fconv']),

            Instr('vfeq.ah',    Format_R2VF, '1010000 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfeq.r.ah',  Format_R2VF, '1010000 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfne.ah',    Format_R2VF, '1010001 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfne.r.ah',  Format_R2VF, '1010001 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vflt.ah',    Format_R2VF, '1010010 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vflt.r.ah',  Format_R2VF, '1010010 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfge.ah',    Format_R2VF, '1010011 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfge.r.ah',  Format_R2VF, '1010011 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfle.ah',    Format_R2VF, '1010100 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfle.r.ah',  Format_R2VF, '1010100 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfgt.ah',    Format_R2VF, '1010101 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),
            Instr('vfgt.r.ah',  Format_R2VF, '1010101 ----- ----- 101 ----- 0110011', tags=['fother']),

            Instr('vfcpka.ah.s', Format_R2VF,'1011000 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvec']),

            # Unless RV32D supported
            Instr('vfmv.x.ah',   Format_R3F, '1001100 00000 ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvecno32d']),
            Instr('vfmv.ah.x',   Format_R3F2,'1001100 00000 ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvecno32d']),

            Instr('vfcvt.x.ah',  Format_R3F, '1001100 00010 ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecno32d']),
            Instr('vfcvt.xu.ah', Format_R3F, '1001100 00010 ----- 101 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecno32d']),
            Instr('vfcvt.ah.x',  Format_R3F2,'1001100 00011 ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecno32d']),
            Instr('vfcvt.ah.xu', Format_R3F2,'1001100 00011 ----- 101 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecno32d']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.ah',  Format_RVF2,'1001100 00101 ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecd']),
            # Instr('vfcvt.ah.s',  Format_RVF2,'1001100 00100 ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecd']),

            # Instr('vfcpkb.ah.s', Format_RVF4,'1011000 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvecd']),
            # Instr('vfcpka.ah.d', Format_RVF4,'1011010 ----- ----- 001 ----- 0110011', tags=['fother'], isa_tags=['f16altvecd']),
            # Instr('vfcpkb.ah.d', Format_RVF4,'1011010 ----- ----- 101 ----- 0110011', tags=['fother'], isa_tags=['f16altvecd']),

            # If Xf16 extension also supported
            Instr('vfcvt.h.ah',  Format_RVF2,'1001100 00101 ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecf16']),
            Instr('vfcvt.ah.h',  Format_RVF2,'1001100 00110 ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f16altvecf16']),

        #
        # For Xf8
        #
            Instr('vfadd.b',    Format_RVF, '1000001 ----- ----- 011 ----- 0110011', tags=['fadd'], isa_tags=['f8vec']),
            Instr('vfadd.r.b',  Format_RVF, '1000001 ----- ----- 111 ----- 0110011', tags=['fadd'], isa_tags=['f8vec']),
            Instr('vfsub.b',    Format_RVF, '1000010 ----- ----- 011 ----- 0110011', tags=['fadd'], isa_tags=['f8vec']),
            Instr('vfsub.r.b',  Format_RVF, '1000010 ----- ----- 111 ----- 0110011', tags=['fadd'], isa_tags=['f8vec']),
            Instr('vfmul.b',    Format_RVF, '1000011 ----- ----- 011 ----- 0110011', tags=['fmul'], isa_tags=['f8vec']),
            Instr('vfmul.r.b',  Format_RVF, '1000011 ----- ----- 111 ----- 0110011', tags=['fmul'], isa_tags=['f8vec']),
            Instr('vfdiv.b',    Format_RVF, '1000100 ----- ----- 011 ----- 0110011', tags=['fdiv'], isa_tags=['f8vec']),
            Instr('vfdiv.r.b',  Format_RVF, '1000100 ----- ----- 111 ----- 0110011', tags=['fdiv'], isa_tags=['f8vec']),

            Instr('vfmin.b',    Format_RVF, '1000101 ----- ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfmin.r.b',  Format_RVF, '1000101 ----- ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfmax.b',    Format_RVF, '1000110 ----- ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfmax.r.b',  Format_RVF, '1000110 ----- ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),

            Instr('vfsqrt.b',   Format_RVF2,'1000111 00000 ----- 011 ----- 0110011', tags=['fdiv'], isa_tags=['f8vec']),

            Instr('vfmac.b',    Format_RVF4,'1001000 ----- ----- 011 ----- 0110011', tags=['fmadd'], isa_tags=['f8vec']),
            Instr('vfmac.r.b',  Format_RVF4,'1001000 ----- ----- 111 ----- 0110011', tags=['fmadd'], isa_tags=['f8vec']),
            Instr('vfmre.b',    Format_RVF4,'1001001 ----- ----- 011 ----- 0110011', tags=['fmadd'], isa_tags=['f8vec']),
            Instr('vfmre.r.b',  Format_RVF4,'1001001 ----- ----- 111 ----- 0110011', tags=['fmadd'], isa_tags=['f8vec']),

            Instr('vfclass.b', Format_R2VF2,'1001100 00001 ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),

            Instr('vfsgnj.r.b', Format_RVF, '1001101 ----- ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfsgnj.b',   Format_RVF, '1001101 ----- ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfsgnjn.b',  Format_RVF, '1001110 ----- ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfsgnjn.r.b',Format_RVF, '1001110 ----- ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfsgnjx.b',  Format_RVF, '1001111 ----- ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),
            Instr('vfsgnjx.r.b',Format_RVF, '1001111 ----- ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vec']),

            Instr('vfeq.b',    Format_R2VF, '1010000 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfeq.r.b',  Format_R2VF, '1010000 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfne.b',    Format_R2VF, '1010001 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfne.r.b',  Format_R2VF, '1010001 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vflt.b',    Format_R2VF, '1010010 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vflt.r.b',  Format_R2VF, '1010010 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfge.b',    Format_R2VF, '1010011 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfge.r.b',  Format_R2VF, '1010011 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfle.b',    Format_R2VF, '1010100 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfle.r.b',  Format_R2VF, '1010100 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfgt.b',    Format_R2VF, '1010101 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),
            Instr('vfgt.r.b',  Format_R2VF, '1010101 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vec']),

            # Unless RV32D supported
            Instr('vfmv.x.b',   Format_R3F, '1001100 00000 ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vecno32d']),
            Instr('vfmv.b.x',   Format_R3F2,'1001100 00000 ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vecno32d']),


            Instr('vfcvt.x.b',  Format_R3F, '1001100 00010 ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vecno32d']),
            Instr('vfcvt.xu.b', Format_R3F, '1001100 00010 ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vecno32d']),
            Instr('vfcvt.b.x',  Format_R3F2,'1001100 00011 ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vecno32d']),
            Instr('vfcvt.b.xu', Format_R3F2,'1001100 00011 ----- 111 ----- 0110011', tags=['fconv'], isa_tags=['f8vecno32d']),

            # If F extension also supported (implies FLEN>=32)
            Instr('vfcpka.b.s', Format_R2VF,'1011000 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vecf']),
            Instr('vfcpkb.b.s', Format_R2VF,'1011000 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vecf']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.b',  Format_RVF2,'1001100 00111 ----- 000 ----- 0110011', tags=['fconv'], isa_tags=['f8vecd']),
            # Instr('vfcvt.b.s',  Format_RVF2,'1001100 00100 ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vecd']),

            # Instr('vfcpkc.b.s', Format_RVF4,'1011001 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),
            # Instr('vfcpkd.b.s', Format_RVF4,'1011001 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),
            # Instr('vfcpka.b# ', Format_RVF4,'1011010 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),
            # Instr('vfcpkb.b.d', Format_RVF4,'1011010 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),
            # Instr('vfcpkc.b.d', Format_RVF4,'1011011 ----- ----- 011 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),
            # Instr('vfcpkd.b.d', Format_RVF4,'1011011 ----- ----- 111 ----- 0110011', tags=['fother'], isa_tags=['f8vecd']),

            # If Xf16 extension also supported
            Instr('vfcvt.h.b',  Format_RVF2,'1001100 00111 ----- 010 ----- 0110011', tags=['fconv'], isa_tags=['f8vecf16']),
            Instr('vfcvt.b.h',  Format_RVF2,'1001100 00110 ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vecf16']),

            # If Xf16alt extension also supported
            Instr('vfcvt.ah.b', Format_RVF2,'1001100 00111 ----- 001 ----- 0110011', tags=['fconv'], isa_tags=['f8vecf16alt']),
            Instr('vfcvt.b.ah', Format_RVF2,'1001100 00101 ----- 011 ----- 0110011', tags=['fconv'], isa_tags=['f8vecf16alt']),

        ])


#
# Auxiliary Float operations
#

class Xfaux(IsaSubset):
    
    def __init__(self):
        super().__init__(name='faux', active=False, instrs=[
        #
        # For F
        #
            # # If Xfvec supported
            # Instr('vfdotp.s',      Format_RVF, '1001010 ----- ----- 000 ----- 0110011', tags=['fmadd'], isa_tags=['f32auxvec']),
            # Instr('vfdotp.r.s',    Format_RVF, '1001010 ----- ----- 100 ----- 0110011', tags=['fmadd'], isa_tags=['f32auxvec']),
            # Instr('vfavg.s',       Format_RVF, '1010110 ----- ----- 000 ----- 0110011', tags=['fadd'], isa_tags=['f32auxvec']),
            # Instr('vfavg.r.s',     Format_RVF, '1010110 ----- ----- 100 ----- 0110011', tags=['fadd'], isa_tags=['f32auxvec']),

        #
        # For Xf16
        #
            Instr('fmulex.s.h',     Format_RF, '0100110 ----- ----- --- ----- 1010011', tags=['fmul'], isa_tags=['f16aux']),
            Instr('fmacex.s.h',    Format_RF4, '0101010 ----- ----- --- ----- 1010011', tags=['fmadd'], isa_tags=['f16aux']),

            # If Xfvec supported
            Instr('vfdotp.h',      Format_RVF, '1001010 ----- ----- 010 ----- 0110011', tags=['fmadd'], isa_tags=['f16auxvec']),
            Instr('vfdotp.r.h',    Format_RVF, '1001010 ----- ----- 110 ----- 0110011', tags=['fmadd'], isa_tags=['f16auxvec']),
            Instr('vfdotpex.s.h',  Format_RVF, '1001011 ----- ----- 010 ----- 0110011', tags=['fmadd'], isa_tags=['f16auxvec']),
            Instr('vfdotpex.s.r.h',Format_RVF, '1001011 ----- ----- 110 ----- 0110011', tags=['fmadd'], isa_tags=['f16auxvec']),
            Instr('vfavg.h',       Format_RVF, '1010110 ----- ----- 010 ----- 0110011', tags=['fadd'], isa_tags=['f16auxvec']),
            Instr('vfavg.r.h',     Format_RVF, '1010110 ----- ----- 110 ----- 0110011', tags=['fadd'], isa_tags=['f16auxvec']),

        #
        # For Xf16aux
        #
            Instr('fmulex.s.ah',    Format_RF, '0100110 ----- ----- 101 ----- 1010011', tags=['fmul'], isa_tags=['f16altaux']),
            Instr('fmacex.s.ah',   Format_RF4, '0101010 ----- ----- 101 ----- 1010011', tags=['fmadd'], isa_tags=['f16altaux']),

            # If Xfvec supported
            Instr('vfdotp.ah',      Format_RVF,'1001010 ----- ----- 001 ----- 0110011', tags=['fmadd'], isa_tags=['f16altauxvec']),
            Instr('vfdotp.r.ah',    Format_RVF,'1001010 ----- ----- 101 ----- 0110011', tags=['fmadd'], isa_tags=['f16altauxvec']),
            Instr('vfdotpex.s.ah',  Format_RVF,'1001011 ----- ----- 001 ----- 0110011', tags=['fmadd'], isa_tags=['f16altauxvec']),
            Instr('vfdotpex.s.r.ah',Format_RVF,'1001011 ----- ----- 101 ----- 0110011', tags=['fmadd'], isa_tags=['f16altauxvec']),
            Instr('vfavg.ah',       Format_RVF,'1010110 ----- ----- 001 ----- 0110011', tags=['fadd'], isa_tags=['f16altauxvec']),
            Instr('vfavg.r.ah',     Format_RVF,'1010110 ----- ----- 101 ----- 0110011', tags=['fadd'], isa_tags=['f16altauxvec']),

        #
        # For Xf8
        #
            Instr('fmulex.s.b',     Format_RF, '0100111 ----- ----- --- ----- 1010011', tags=['fmul'], isa_tags=['f8aux']),
            Instr('fmacex.s.b',    Format_RF4, '0101011 ----- ----- --- ----- 1010011', tags=['fmadd'], isa_tags=['f8aux']),

            # If Xfvec supported
            Instr('vfdotp.b',      Format_RVF, '1001010 ----- ----- 011 ----- 0110011', tags=['fmadd'], isa_tags=['f8auxvec']),
            Instr('vfdotp.r.b',    Format_RVF, '1001010 ----- ----- 111 ----- 0110011', tags=['fmadd'], isa_tags=['f8auxvec']),
            Instr('vfdotpex.s.b',  Format_RVF, '1001011 ----- ----- 011 ----- 0110011', tags=['fmadd'], isa_tags=['f8auxvec']),
            Instr('vfdotpex.s.r.b',Format_RVF, '1001011 ----- ----- 111 ----- 0110011', tags=['fmadd'], isa_tags=['f8auxvec']),
            Instr('vfavg.b',       Format_RVF, '1010110 ----- ----- 011 ----- 0110011', tags=['fadd'], isa_tags=['f8auxvec']),
            Instr('vfavg.r.b',     Format_RVF, '1010110 ----- ----- 111 ----- 0110011', tags=['fadd'], isa_tags=['f8auxvec']),

        ])

