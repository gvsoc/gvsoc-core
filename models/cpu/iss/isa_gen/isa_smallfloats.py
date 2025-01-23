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

from cpu.iss.isa_gen.isa_riscv_gen import FL, FS, R_F, F_R, R_FF, F_F, F_FF, F_FFF, FA_FFF, FA_FF, ui12_3
from cpu.iss.isa_gen.isa_gen import Instr, IsaSubset, Insn

class Xf16(IsaSubset):

    def __init__(self):
        super().__init__(name='f16', instrs=[

            Insn('------- ----- ----- 001 ----- 0000111', 'flh',       FL('h'),            tags=["load", 'fp_op']),
            Insn('------- ----- ----- 001 ----- 0100111', 'fsh',       FS('h'),            tags=['fp_op']),
            Insn('-----10 ----- ----- --- ----- 1000011', 'fmadd.h',   F_FFF('h', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----10 ----- ----- --- ----- 1000111', 'fmsub.h',   F_FFF('h', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----10 ----- ----- --- ----- 1001011', 'fnmsub.h',  F_FFF('h', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----10 ----- ----- --- ----- 1001111', 'fnmadd.h',  F_FFF('h', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('0000010 ----- ----- --- ----- 1010011', 'fadd.h',    F_FF('h', ui12_3),  tags=['sfadd', 'fp_op']),
            Insn('0000110 ----- ----- --- ----- 1010011', 'fsub.h',    F_FF('h', ui12_3),  tags=['sfadd', 'fp_op']),
            Insn('0001010 ----- ----- --- ----- 1010011', 'fmul.h',    F_FF('h', ui12_3),  tags=['sfmul', 'fp_op']),
            Insn('0001110 ----- ----- --- ----- 1010011', 'fdiv.h',    F_FF('h', ui12_3),  tags=['sfdiv', 'fp_op']),
            Insn('0101110 00000 ----- --- ----- 1010011', 'fsqrt.h',   F_F('h', ui12_3),   tags=['sfdiv', 'fp_op']),
            Insn('0010010 ----- ----- 000 ----- 1010011', 'fsgnj.h',   F_FF('h'),          tags=['sfconv', 'fp_op']),
            Insn('0010010 ----- ----- 001 ----- 1010011', 'fsgnjn.h',  F_FF('h'),          tags=['sfconv', 'fp_op']),
            Insn('0010010 ----- ----- 010 ----- 1010011', 'fsgnjx.h',  F_FF('h'),          tags=['sfconv', 'fp_op']),
            Insn('0010110 ----- ----- 000 ----- 1010011', 'fmin.h',    F_FF('h'),          tags=['sfconv', 'fp_op']),
            Insn('0010110 ----- ----- 001 ----- 1010011', 'fmax.h',    F_FF('h'),          tags=['sfconv', 'fp_op']),
            Insn('1010010 ----- ----- 010 ----- 1010011', 'feq.h',     R_FF('h'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1010010 ----- ----- 001 ----- 1010011', 'flt.h',     R_FF('h'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1010010 ----- ----- 000 ----- 1010011', 'fle.h',     R_FF('h'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1100010 00000 ----- --- ----- 1010011', 'fcvt.w.h',  R_F('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1100010 00001 ----- --- ----- 1010011', 'fcvt.wu.h', R_F('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1101010 00000 ----- --- ----- 1010011', 'fcvt.h.w',  F_R('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1101010 00001 ----- --- ----- 1010011', 'fcvt.h.wu', F_R('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1110010 00000 ----- 000 ----- 1010011', 'fmv.x.h',   R_F('h'),           tags=['sfother', 'nseq', 'fp_op']),
            Insn('1110010 00000 ----- 001 ----- 1010011', 'fclass.h',  R_F('h'),           tags=['sfother', 'nseq', 'fp_op']),
            Insn('1111010 00000 ----- 000 ----- 1010011', 'fmv.h.x',   F_R('h'),           tags=['sfother', 'nseq', 'fp_op']),

            # If RV64Xf16 supported
            Insn('1100010 00010 ----- --- ----- 1010011', 'fcvt.l.h',  R_F('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f16']),
            Insn('1100010 00011 ----- --- ----- 1010011', 'fcvt.lu.h', R_F('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f16']),
            Insn('1101010 00010 ----- --- ----- 1010011', 'fcvt.h.l',  F_R('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f16']),
            Insn('1101010 00011 ----- --- ----- 1010011', 'fcvt.h.lu', F_R('h', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f16']),

            # If F also supported
            Insn('0100000 00010 ----- 000 ----- 1010011', 'fcvt.s.h',  F_F('s_h'),         tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f16f']),
            Insn('0100010 00000 ----- --- ----- 1010011', 'fcvt.h.s',  F_F('h_s', ui12_3), tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f16f']),

            # # If D also supported
            # Instr('fcvt.d.h', Format_R2F3,'0100001 00010 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16d']),
            # Instr('fcvt.h.d', Format_R2F3,'0100010 00001 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f16d']),
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rv64i'):
            isa.disable_from_isa_tag('rv64f16')

        if not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('f16f')

        if not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16d')



class Xf16alt(IsaSubset):

    def __init__(self):
        super().__init__(name='f16alt', instrs=[
            Insn('-----10 ----- ----- 101 ----- 1000011', 'fmadd.ah',   F_FFF('ah'), tags=['sfmadd']),
            Insn('-----10 ----- ----- 101 ----- 1000111', 'fmsub.ah',   F_FFF('ah'), tags=['sfmadd']),
            Insn('-----10 ----- ----- 101 ----- 1001011', 'fnmsub.ah',  F_FFF('ah'), tags=['sfmadd']),
            Insn('-----10 ----- ----- 101 ----- 1001111', 'fnmadd.ah',  F_FFF('ah'), tags=['sfmadd']),
            Insn('0000010 ----- ----- 101 ----- 1010011', 'fadd.ah',    F_FF('ah'),  tags=['sfadd']),
            Insn('0000110 ----- ----- 101 ----- 1010011', 'fsub.ah',    F_FF('ah'),  tags=['sfadd']),
            Insn('0001010 ----- ----- 101 ----- 1010011', 'fmul.ah',    F_FF('ah'),  tags=['sfmul']),
            Insn('0001110 ----- ----- 101 ----- 1010011', 'fdiv.ah',    F_FF('ah'),  tags=['sfdiv']),
            Insn('0101110 00000 ----- 101 ----- 1010011', 'fsqrt.ah',   F_F('ah'),   tags=['sfdiv']),
            Insn('0010010 ----- ----- 100 ----- 1010011', 'fsgnj.ah',   F_FF('ah'),  tags=['sfconv']),
            Insn('0010010 ----- ----- 101 ----- 1010011', 'fsgnjn.ah',  F_FF('ah'),  tags=['sfconv']),
            Insn('0010010 ----- ----- 110 ----- 1010011', 'fsgnjx.ah',  F_FF('ah'),  tags=['sfconv']),
            Insn('0010110 ----- ----- 100 ----- 1010011', 'fmin.ah',    F_FF('ah'),  tags=['sfconv']),
            Insn('0010110 ----- ----- 101 ----- 1010011', 'fmax.ah',    F_FF('ah'),  tags=['sfconv']),
            Insn('1010010 ----- ----- 110 ----- 1010011', 'feq.ah',     R_FF('ah'),  tags=['sfother', 'nseq']),
            Insn('1010010 ----- ----- 101 ----- 1010011', 'flt.ah',     R_FF('ah'),  tags=['sfother', 'nseq']),
            Insn('1010010 ----- ----- 100 ----- 1010011', 'fle.ah',     R_FF('ah'),  tags=['sfother', 'nseq']),
            Insn('1100010 00000 ----- 101 ----- 1010011', 'fcvt.w.ah',  R_F('ah'),   tags=['sfconv', 'nseq']),
            Insn('1100010 00001 ----- 101 ----- 1010011', 'fcvt.wu.ah', R_F('ah'),   tags=['sfconv', 'nseq']),
            Insn('1101010 00000 ----- 101 ----- 1010011', 'fcvt.ah.w',  F_R('ah'),   tags=['sfconv', 'nseq']),
            Insn('1101010 00001 ----- 101 ----- 1010011', 'fcvt.ah.wu', F_R('ah'),   tags=['sfconv', 'nseq']),
            Insn('1110010 00000 ----- 100 ----- 1010011', 'fmv.x.ah',   R_F('ah'),   tags=['sfother', 'nseq']),
            Insn('1110010 00000 ----- 101 ----- 1010011', 'fclass.ah',  R_F('ah'),   tags=['sfother', 'nseq']),
            Insn('1111010 00000 ----- 100 ----- 1010011', 'fmv.ah.x',   F_R('ah'),   tags=['sfother', 'nseq']),

            # If RV64Xf16alt supported
            Insn('1100010 00010 ----- 101 ----- 1010011', 'fcvt.l.ah',  R_F('ah'),   tags=['sfconv', 'nseq'], isa_tags=['rv64f16alt']),
            Insn('1100010 00011 ----- 101 ----- 1010011', 'fcvt.lu.ah', R_F('ah'),   tags=['sfconv', 'nseq'], isa_tags=['rv64f16alt']),
            Insn('1101010 00010 ----- 101 ----- 1010011', 'fcvt.ah.l',  F_R('ah'),   tags=['sfconv', 'nseq'], isa_tags=['rv64f16alt']),
            Insn('1101010 00011 ----- 101 ----- 1010011', 'fcvt.ah.lu', F_R('ah'),   tags=['sfconv', 'nseq'], isa_tags=['rv64f16alt']),

            # If F also supported
            Insn('0100000 00110 ----- 000 ----- 1010011', 'fcvt.s.ah',  F_F('ah'),   tags=['sfconv', 'nseq'], isa_tags=['f16altf']),
            Insn('0100010 00000 ----- 101 ----- 1010011', 'fcvt.ah.s',  F_F('ah'),   tags=['sfconv', 'nseq'], isa_tags=['f16altf']),

            # # If D also supported
            # Instr('fcvt.d.ah', Format_R2F3,'0100001 00110 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f16altd']),
            # Instr('fcvt.ah.d', Format_R2F3,'0100010 00001 ----- 101 ----- 1010011', tags=['sfconv'], isa_tags=['f16altd']),

            # If Xf16 also supported
            Insn('0100010 00110 ----- --- ----- 1010011', 'fcvt.h.ah',  F_F('h_ah', ui12_3), tags=['sfconv', 'nseq'], isa_tags=['f16altf16']),
            Insn('0100010 00010 ----- 101 ----- 1010011', 'fcvt.ah.h',  F_F('ah_h'),         tags=['sfconv', 'nseq'], isa_tags=['f16altf16']),
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rv64i'):
            isa.disable_from_isa_tag('rv64f16alt')

        if not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('f16altf')

        if not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16altd')

        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f16altf16')



class Xf8(IsaSubset):

    def __init__(self):
        super().__init__(name='f8', instrs=[
            Insn('------- ----- ----- 000 ----- 0000111', 'flb',       FL('b'),            tags=["load", 'fp_op']),
            Insn('------- ----- ----- 000 ----- 0100111', 'fsb',       FS('b'),            tags=['fp_op']),
            Insn('-----11 ----- ----- --- ----- 1000011', 'fmadd.b',   F_FFF('b', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----11 ----- ----- --- ----- 1000111', 'fmsub.b',   F_FFF('b', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----11 ----- ----- --- ----- 1001011', 'fnmsub.b',  F_FFF('b', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('-----11 ----- ----- --- ----- 1001111', 'fnmadd.b',  F_FFF('b', ui12_3), tags=['sfmadd', 'fp_op']),
            Insn('0000011 ----- ----- --- ----- 1010011', 'fadd.b',    F_FF('b', ui12_3),  tags=['sfadd', 'fp_op']),
            Insn('0000111 ----- ----- --- ----- 1010011', 'fsub.b',    F_FF('b', ui12_3),  tags=['sfadd', 'fp_op']),
            Insn('0001011 ----- ----- --- ----- 1010011', 'fmul.b',    F_FF('b', ui12_3),  tags=['sfmul', 'fp_op']),
            Insn('0001111 ----- ----- --- ----- 1010011', 'fdiv.b',    F_FF('b', ui12_3),  tags=['sfdiv', 'fp_op']),
            Insn('0101111 00000 ----- --- ----- 1010011', 'fsqrt.b',   F_F('b', ui12_3),   tags=['sfdiv', 'fp_op']),
            Insn('0010011 ----- ----- 000 ----- 1010011', 'fsgnj.b',   F_FF('b'),          tags=['sfconv', 'fp_op']),
            Insn('0010011 ----- ----- 001 ----- 1010011', 'fsgnjn.b',  F_FF('b'),          tags=['sfconv', 'fp_op']),
            Insn('0010011 ----- ----- 010 ----- 1010011', 'fsgnjx.b',  F_FF('b'),          tags=['sfconv', 'fp_op']),
            Insn('0010111 ----- ----- 000 ----- 1010011', 'fmin.b',    F_FF('b'),          tags=['sfconv', 'fp_op']),
            Insn('0010111 ----- ----- 001 ----- 1010011', 'fmax.b',    F_FF('b'),          tags=['sfconv', 'fp_op']),
            Insn('1010011 ----- ----- 010 ----- 1010011', 'feq.b',     R_FF('b'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1010011 ----- ----- 001 ----- 1010011', 'flt.b',     R_FF('b'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1010011 ----- ----- 000 ----- 1010011', 'fle.b',     R_FF('b'),          tags=['sfother', 'nseq', 'fp_op']),
            Insn('1100011 00000 ----- --- ----- 1010011', 'fcvt.w.b',  R_F('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1100011 00001 ----- --- ----- 1010011', 'fcvt.wu.b', R_F('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1101011 00000 ----- --- ----- 1010011', 'fcvt.b.w',  F_R('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1101011 00001 ----- --- ----- 1010011', 'fcvt.b.wu', F_R('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op']),
            Insn('1110011 00000 ----- 000 ----- 1010011', 'fmv.x.b',   R_F('b'),           tags=['sfother', 'nseq', 'fp_op']),
            Insn('1110011 00000 ----- 001 ----- 1010011', 'fclass.b',  R_F('b'),           tags=['sfother', 'nseq', 'fp_op']),
            Insn('1111011 00000 ----- 000 ----- 1010011', 'fmv.b.x',   F_R('b'),           tags=['sfother', 'nseq', 'fp_op']),

            # If RV64Xf8 supported
            Insn('1100011 00010 ----- --- ----- 1010011', 'fcvt.l.b',  R_F('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f8']),
            Insn('1100011 00011 ----- --- ----- 1010011', 'fcvt.lu.b', R_F('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f8']),
            Insn('1101011 00010 ----- --- ----- 1010011', 'fcvt.b.l',  F_R('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f8']),
            Insn('1101011 00011 ----- --- ----- 1010011', 'fcvt.b.lu', F_R('b', ui12_3),   tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['rv64f8']),

            # If F also supported
            Insn('0100000 00011 ----- 000 ----- 1010011', 'fcvt.s.b',  F_F('s_b'),         tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f']),
            Insn('0100011 00000 ----- --- ----- 1010011', 'fcvt.b.s',  F_F('b_s', ui12_3), tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f']),

            # # If D also supported
            # Instr('fcvt.d.b', Format_R2F3,'0100001 00011 ----- 000 ----- 1010011', tags=['sfconv'], isa_tags=['f8d']),
            # Instr('fcvt.b.d', Format_R2F3,'0100011 00001 ----- --- ----- 1010011', tags=['sfconv'], isa_tags=['f8d']),

            # If Xf16 also supported
            Insn('0100010 00011 ----- 000 ----- 1010011', 'fcvt.h.b', F_F('h_b'),         tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f16']),
            Insn('0100011 00010 ----- --- ----- 1010011', 'fcvt.b.h', F_F('b_h', ui12_3), tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f16']),

            # If Xf16alt also supported
            Insn('0100010 00011 ----- 101 ----- 1010011', 'fcvt.ah.b', F_F('ah_b'),         tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f16alt']),
            Insn('0100011 00110 ----- --- ----- 1010011', 'fcvt.b.ah', F_F('b_ah', ui12_3), tags=['sfconv', 'nseq', 'fp_op'], isa_tags=['f8f16alt']),
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rv64i'):
            isa.disable_from_isa_tag('rv64f8')
        if not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('f8f')
        if not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f8d')
        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f8f16')
        if not isa.has_isa('f16alt'):
            isa.disable_from_isa_tag('f8f16alt')


#
# Vectorial Floats
#

class Xfvec(IsaSubset):

    def __init__(self, inc_vfsum=False):
        instrs=[
        #
        # For F
        #
            Insn('1000001 ----- ----- 000 ----- 0110011', 'vfadd.s',    F_FF('S'),   tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000001 ----- ----- 100 ----- 0110011', 'vfadd.r.s',  F_FF('S'),   tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000010 ----- ----- 000 ----- 0110011', 'vfsub.s',    F_FF('S'),   tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000010 ----- ----- 100 ----- 0110011', 'vfsub.r.s',  F_FF('S'),   tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000011 ----- ----- 000 ----- 0110011', 'vfmul.s',    F_FF('S'),   tags=['fmul', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000011 ----- ----- 100 ----- 0110011', 'vfmul.r.s',  F_FF('S'),   tags=['fmul', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000101 ----- ----- 000 ----- 0110011', 'vfmin.s',    F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000101 ----- ----- 100 ----- 0110011', 'vfmin.r.s',  F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000110 ----- ----- 000 ----- 0110011', 'vfmax.s',    F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1000110 ----- ----- 100 ----- 0110011', 'vfmax.r.s',  F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001000 ----- ----- 000 ----- 0110011', 'vfmac.s',    FA_FFF('S'), tags=['fmadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001000 ----- ----- 100 ----- 0110011', 'vfmac.r.s',  FA_FFF('S'), tags=['fmadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001001 ----- ----- 000 ----- 0110011', 'vfmre.s',    FA_FFF('S'), tags=['fmadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001001 ----- ----- 100 ----- 0110011', 'vfmre.r.s',  FA_FFF('S'), tags=['fmadd', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001101 ----- ----- 100 ----- 0110011', 'vfsgnj.r.s', F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001101 ----- ----- 000 ----- 0110011', 'vfsgnj.s',   F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001110 ----- ----- 000 ----- 0110011', 'vfsgnjn.s',  F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001110 ----- ----- 100 ----- 0110011', 'vfsgnjn.r.s',F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001111 ----- ----- 000 ----- 0110011', 'vfsgnjx.s',  F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1001111 ----- ----- 100 ----- 0110011', 'vfsgnjx.r.s',F_FF('S'),   tags=['fconv', 'fp_op'], isa_tags=['f32vec']),
            Insn('1010000 ----- ----- 000 ----- 0110011', 'vfeq.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010000 ----- ----- 100 ----- 0110011', 'vfeq.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010001 ----- ----- 000 ----- 0110011', 'vfne.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010001 ----- ----- 100 ----- 0110011', 'vfne.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010010 ----- ----- 000 ----- 0110011', 'vflt.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010010 ----- ----- 100 ----- 0110011', 'vflt.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010011 ----- ----- 000 ----- 0110011', 'vfge.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010011 ----- ----- 100 ----- 0110011', 'vfge.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010100 ----- ----- 000 ----- 0110011', 'vfle.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010100 ----- ----- 100 ----- 0110011', 'vfle.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010101 ----- ----- 000 ----- 0110011', 'vfgt.s',     R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1010101 ----- ----- 100 ----- 0110011', 'vfgt.r.s',   R_FF('S'),   tags=['fother', 'fp_op', 'nseq'], isa_tags=['f32vec']),
            Insn('1011000 ----- ----- 000 ----- 0110011', 'vfcpka.s.s', F_FF('S'),   tags=['fother', 'fp_op'], isa_tags=['f32vec']),

            # TODO these instructions are in the specs, check why they are not implemented
            # Instr('vfdiv.s',    Format_RVF, '1000100 ----- ----- 000 ----- 0110011', tags=['fdiv', 'fp_op'], isa_tags=['f32vec']),
            # Instr('vfdiv.r.s',  Format_RVF, '1000100 ----- ----- 100 ----- 0110011', tags=['fdiv', 'fp_op'], isa_tags=['f32vec']),
            # Instr('vfsqrt.s',   Format_RVF2,'1000111 00000 ----- 000 ----- 0110011', tags=['fdiv', 'fp_op'], isa_tags=['f32vec']),
            # Instr('vfclass.s', Format_R2VF2,'1001100 00001 ----- 000 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f32vec']),
            # Instr('vfcpka.s.d', Format_RVF, '1011010 ----- ----- 000 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f32vec']),

            # # Unless RV32D supported
            # Instr('vfmv.x.s',   Format_R3F, '1001100 00000 ----- 000 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f32vecno32d']),
            # Instr('vfmv.s.x',   Format_R3F2,'1001100 00000 ----- 100 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f32vecno32d']),

            # Instr('vfcvt.x.s',  Format_R3F, '1001100 00010 ----- 000 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.xu.s', Format_R3F, '1001100 00010 ----- 100 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.s.x',  Format_R3F2,'1001100 00011 ----- 000 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f32vecno32d']),
            # Instr('vfcvt.s.xu', Format_R3F2,'1001100 00011 ----- 100 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f32vecno32d']),

        #
        # For Xf16
        #
            Insn('1000001 ----- ----- 010 ----- 0110011', 'vfadd.h',     F_FF('H'),    tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000001 ----- ----- 110 ----- 0110011', 'vfadd.r.h',   F_FF('H'),    tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000010 ----- ----- 010 ----- 0110011', 'vfsub.h',     F_FF('H'),    tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000010 ----- ----- 110 ----- 0110011', 'vfsub.r.h',   F_FF('H'),    tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000011 ----- ----- 010 ----- 0110011', 'vfmul.h',     F_FF('H'),    tags=['fmul', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000011 ----- ----- 110 ----- 0110011', 'vfmul.r.h',   F_FF('H'),    tags=['fmul', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000100 ----- ----- 010 ----- 0110011', 'vfdiv.h',     F_FF('H'),    tags=['fdiv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000100 ----- ----- 110 ----- 0110011', 'vfdiv.r.h',   F_FF('H'),    tags=['fdiv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000101 ----- ----- 010 ----- 0110011', 'vfmin.h',     F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000101 ----- ----- 110 ----- 0110011', 'vfmin.r.h',   F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000110 ----- ----- 010 ----- 0110011', 'vfmax.h',     F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000110 ----- ----- 110 ----- 0110011', 'vfmax.r.h',   F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1000111 00000 ----- 010 ----- 0110011', 'vfsqrt.h',    F_F('H'),     tags=['fdiv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001000 ----- ----- 010 ----- 0110011', 'vfmac.h',     FA_FFF('H'),  tags=['fmadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001000 ----- ----- 110 ----- 0110011', 'vfmac.r.h',   FA_FFF('H'),  tags=['fmadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001001 ----- ----- 010 ----- 0110011', 'vfmre.h',     FA_FFF('H'),  tags=['fmadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001001 ----- ----- 110 ----- 0110011', 'vfmre.r.h',   FA_FFF('H'),  tags=['fmadd', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001100 00001 ----- 010 ----- 0110011', 'vfclass.h',   R_F('H'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001101 ----- ----- 010 ----- 0110011', 'vfsgnj.h',    F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001101 ----- ----- 110 ----- 0110011', 'vfsgnj.r.h',  F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001110 ----- ----- 010 ----- 0110011', 'vfsgnjn.h',   F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001110 ----- ----- 110 ----- 0110011', 'vfsgnjn.r.h', F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001111 ----- ----- 010 ----- 0110011', 'vfsgnjx.h',   F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1001111 ----- ----- 110 ----- 0110011', 'vfsgnjx.r.h', F_FF('H'),    tags=['fconv', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010000 ----- ----- 010 ----- 0110011', 'vfeq.h',      R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010000 ----- ----- 110 ----- 0110011', 'vfeq.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010001 ----- ----- 010 ----- 0110011', 'vfne.h',      R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010001 ----- ----- 110 ----- 0110011', 'vfne.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010010 ----- ----- 010 ----- 0110011', 'vflt.h',      R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010010 ----- ----- 110 ----- 0110011', 'vflt.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010011 ----- ----- 010 ----- 0110011', 'vfge.h',      R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010011 ----- ----- 110 ----- 0110011', 'vfge.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010100 ----- ----- 010 ----- 0110011', 'vfle.h',      R_FF('H'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010100 ----- ----- 110 ----- 0110011', 'vfle.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010101 ----- ----- 010 ----- 0110011', 'vfgt.h',      R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1010101 ----- ----- 110 ----- 0110011', 'vfgt.r.h',    R_FF('H'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vec']),
            Insn('1011000 ----- ----- 010 ----- 0110011', 'vfcpka.h.s',  F_FF('H_SS'), tags=['fother', 'fp_op'], isa_tags=['f16vec']),

            # Unless RV32D supported
            Insn('1001100 00000 ----- 010 ----- 0110011', 'vfmv.x.h',   R_F('H'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),
            Insn('1001100 00000 ----- 110 ----- 0110011', 'vfmv.h.x',   F_R('H'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),
            Insn('1001100 00010 ----- 010 ----- 0110011', 'vfcvt.x.h',  R_F('H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),
            Insn('1001100 00010 ----- 110 ----- 0110011', 'vfcvt.xu.h', R_F('H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),
            Insn('1001100 00011 ----- 010 ----- 0110011', 'vfcvt.h.x',  F_R('H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),
            Insn('1001100 00011 ----- 110 ----- 0110011', 'vfcvt.h.xu', F_R('H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16vecno32d']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.h',  Format_RVF2,'1001100 00110 ----- 000 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f16vecd']),
            # Instr('vfcvt.h.s',  Format_RVF2,'1001100 00100 ----- 010 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f16vecd']),
            Insn('1011000 ----- ----- 110 ----- 0110011', 'vfcpkb.h.s', F_FF('H_SS'), tags=['fother', 'fp_op'], isa_tags=['f16vecd']),
            # Instr('vfcpka.h.d', Format_RVF4,'1011010 ----- ----- 010 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f16vecd']),
            # Instr('vfcpkb.h.d', Format_RVF4,'1011010 ----- ----- 110 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f16vecd']),

        #
        # For Xf16alt
        #
            Insn('1000001 ----- ----- 001 ----- 0110011', 'vfadd.ah',     F_FF('AH'),   tags=['fadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000001 ----- ----- 101 ----- 0110011', 'vfadd.r.ah',   F_FF('AH'),   tags=['fadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000010 ----- ----- 001 ----- 0110011', 'vfsub.ah',     F_FF('AH'),   tags=['fadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000010 ----- ----- 101 ----- 0110011', 'vfsub.r.ah',   F_FF('AH'),   tags=['fadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000011 ----- ----- 001 ----- 0110011', 'vfmul.ah',     F_FF('AH'),   tags=['fmul', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000011 ----- ----- 101 ----- 0110011', 'vfmul.r.ah',   F_FF('AH'),   tags=['fmul', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000100 ----- ----- 001 ----- 0110011', 'vfdiv.ah',     F_FF('AH'),   tags=['fdiv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000100 ----- ----- 101 ----- 0110011', 'vfdiv.r.ah',   F_FF('AH'),   tags=['fdiv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000101 ----- ----- 001 ----- 0110011', 'vfmin.ah',     F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000101 ----- ----- 101 ----- 0110011', 'vfmin.r.ah',   F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000110 ----- ----- 001 ----- 0110011', 'vfmax.ah',     F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1000110 ----- ----- 101 ----- 0110011', 'vfmax.r.ah',   F_FF('AH'),   tags=['fconv', 'fp_op']),
            Insn('1000111 00000 ----- 001 ----- 0110011', 'vfsqrt.ah',    F_F('AH') ,   tags=['fdiv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001000 ----- ----- 001 ----- 0110011', 'vfmac.ah',     FA_FFF('AH'), tags=['fmadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001000 ----- ----- 101 ----- 0110011', 'vfmac.r.ah',   FA_FFF('AH'), tags=['fmadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001001 ----- ----- 001 ----- 0110011', 'vfmre.ah',     FA_FFF('AH'), tags=['fmadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001001 ----- ----- 101 ----- 0110011', 'vfmre.r.ah',   FA_FFF('AH'), tags=['fmadd', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001100 00001 ----- 001 ----- 0110011', 'vfclass.ah',   R_F('AH'),    tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001101 ----- ----- 001 ----- 0110011', 'vfsgnj.r.ah',  F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001101 ----- ----- 101 ----- 0110011', 'vfsgnj.ah',    F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001110 ----- ----- 001 ----- 0110011', 'vfsgnjn.ah',   F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001110 ----- ----- 101 ----- 0110011', 'vfsgnjn.r.ah', F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001111 ----- ----- 001 ----- 0110011', 'vfsgnjx.ah',   F_FF('AH'),   tags=['fconv', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1001111 ----- ----- 101 ----- 0110011', 'vfsgnjx.r.ah', F_FF('AH'),   tags=['fconv', 'fp_op']),
            Insn('1010000 ----- ----- 001 ----- 0110011', 'vfeq.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010000 ----- ----- 101 ----- 0110011', 'vfeq.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010001 ----- ----- 001 ----- 0110011', 'vfne.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010001 ----- ----- 101 ----- 0110011', 'vfne.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010010 ----- ----- 001 ----- 0110011', 'vflt.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010010 ----- ----- 101 ----- 0110011', 'vflt.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010011 ----- ----- 001 ----- 0110011', 'vfge.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010011 ----- ----- 101 ----- 0110011', 'vfge.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010100 ----- ----- 001 ----- 0110011', 'vfle.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010100 ----- ----- 101 ----- 0110011', 'vfle.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010101 ----- ----- 001 ----- 0110011', 'vfgt.ah',      R_FF('AH'),   tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvec']),
            Insn('1010101 ----- ----- 101 ----- 0110011', 'vfgt.r.ah',    R_FF('AH'),   tags=['fother', 'nseq', 'fp_op']),
            Insn('1011000 ----- ----- 001 ----- 0110011', 'vfcpka.ah.s',  F_FF('AH_S'), tags=['fother', 'fp_op'], isa_tags=['f16altvec']),

            # Unless RV32D supported
            Insn('1001100 00000 ----- 001 ----- 0110011', 'vfmv.x.ah',   R_F('AH'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),
            Insn('1001100 00000 ----- 101 ----- 0110011', 'vfmv.ah.x',   F_R('AH'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),
            Insn('1001100 00010 ----- 001 ----- 0110011', 'vfcvt.x.ah',  R_F('AH'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),
            Insn('1001100 00010 ----- 101 ----- 0110011', 'vfcvt.xu.ah', R_F('AH'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),
            Insn('1001100 00011 ----- 001 ----- 0110011', 'vfcvt.ah.x',  F_R('AH'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),
            Insn('1001100 00011 ----- 101 ----- 0110011', 'vfcvt.ah.xu', F_R('AH'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecno32d']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.ah',  Format_RVF2,'1001100 00101 ----- 000 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f16altvecd']),
            # Instr('vfcvt.ah.s',  Format_RVF2,'1001100 00100 ----- 001 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f16altvecd']),

            # Instr('vfcpkb.ah.s', Format_RVF4,'1011000 ----- ----- 101 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f16altvecd']),
            # Instr('vfcpka.ah.d', Format_RVF4,'1011010 ----- ----- 001 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f16altvecd']),
            # Instr('vfcpkb.ah.d', Format_RVF4,'1011010 ----- ----- 101 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f16altvecd']),

            # If Xf16 extension also supported
            Insn('1001100 00101 ----- 010 ----- 0110011', 'vfcvt.h.ah',  F_F('H_AH') ,tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecf16']),
            Insn('1001100 00110 ----- 001 ----- 0110011', 'vfcvt.ah.h',  F_F('AH_H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f16altvecf16']),

        #
        # For Xf8
        #
            Insn('1000001 ----- ----- 011 ----- 0110011', 'vfadd.b',     F_FF('B_BB'),  tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000001 ----- ----- 111 ----- 0110011', 'vfadd.r.b',   F_FF('B_BB'),  tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000010 ----- ----- 011 ----- 0110011', 'vfsub.b',     F_FF('B_BB'),  tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000010 ----- ----- 111 ----- 0110011', 'vfsub.r.b',   F_FF('B_BB'),  tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000011 ----- ----- 011 ----- 0110011', 'vfmul.b',     F_FF('B_BB'),  tags=['fmul', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000011 ----- ----- 111 ----- 0110011', 'vfmul.r.b',   F_FF('B_BB'),  tags=['fmul', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000100 ----- ----- 011 ----- 0110011', 'vfdiv.b',     F_FF('B_BB'),  tags=['fdiv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000100 ----- ----- 111 ----- 0110011', 'vfdiv.r.b',   F_FF('B_BB'),  tags=['fdiv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000101 ----- ----- 011 ----- 0110011', 'vfmin.b',     F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000101 ----- ----- 111 ----- 0110011', 'vfmin.r.b',   F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000110 ----- ----- 011 ----- 0110011', 'vfmax.b',     F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000110 ----- ----- 111 ----- 0110011', 'vfmax.r.b',   F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1000111 00000 ----- 011 ----- 0110011', 'vfsqrt.b',    F_F('B'),      tags=['fdiv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001000 ----- ----- 011 ----- 0110011', 'vfmac.b',     FA_FFF('B') ,  tags=['fmadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001000 ----- ----- 111 ----- 0110011', 'vfmac.r.b',   FA_FFF('B') ,  tags=['fmadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001001 ----- ----- 011 ----- 0110011', 'vfmre.b',     FA_FFF('B') ,  tags=['fmadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001001 ----- ----- 111 ----- 0110011', 'vfmre.r.b',   FA_FFF('B') ,  tags=['fmadd', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001100 00001 ----- 011 ----- 0110011', 'vfclass.b',   R_F('B') ,     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001101 ----- ----- 011 ----- 0110011', 'vfsgnj.b',    F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001101 ----- ----- 111 ----- 0110011', 'vfsgnj.r.b',  F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001110 ----- ----- 011 ----- 0110011', 'vfsgnjn.b',   F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001110 ----- ----- 111 ----- 0110011', 'vfsgnjn.r.b', F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001111 ----- ----- 011 ----- 0110011', 'vfsgnjx.b',   F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1001111 ----- ----- 111 ----- 0110011', 'vfsgnjx.r.b', F_FF('B_BB'),  tags=['fconv', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010000 ----- ----- 011 ----- 0110011', 'vfeq.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010000 ----- ----- 111 ----- 0110011', 'vfeq.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010001 ----- ----- 011 ----- 0110011', 'vfne.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010001 ----- ----- 111 ----- 0110011', 'vfne.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010010 ----- ----- 011 ----- 0110011', 'vflt.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010010 ----- ----- 111 ----- 0110011', 'vflt.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010011 ----- ----- 011 ----- 0110011', 'vfge.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010011 ----- ----- 111 ----- 0110011', 'vfge.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010100 ----- ----- 011 ----- 0110011', 'vfle.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010100 ----- ----- 111 ----- 0110011', 'vfle.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010101 ----- ----- 011 ----- 0110011', 'vfgt.b',      R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),
            Insn('1010101 ----- ----- 111 ----- 0110011', 'vfgt.r.b',    R_FF('B'),     tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vec']),

            # Unless RV32D supported
            Insn('1001100 00000 ----- 011 ----- 0110011', 'vfmv.x.b',   R_F('B'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),
            Insn('1001100 00000 ----- 111 ----- 0110011', 'vfmv.b.x',   F_R('B'), tags=['fother', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),
            Insn('1001100 00010 ----- 011 ----- 0110011', 'vfcvt.x.b',  R_F('B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),
            Insn('1001100 00010 ----- 111 ----- 0110011', 'vfcvt.xu.b', R_F('B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),
            Insn('1001100 00011 ----- 011 ----- 0110011', 'vfcvt.b.x',  F_R('B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),
            Insn('1001100 00011 ----- 111 ----- 0110011', 'vfcvt.b.xu', F_R('B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecno32d']),

            # If F extension also supported (implies FLEN>=32)
            Insn('1011000 ----- ----- 011 ----- 0110011', 'vfcpka.b.s', F_FF('B_SS'), tags=['fother', 'fp_op'], isa_tags=['f8vecf']),
            Insn('1011000 ----- ----- 111 ----- 0110011', 'vfcpkb.b.s', F_FF('B_SS'), tags=['fother', 'fp_op'], isa_tags=['f8vecf']),
            Insn('1011001 ----- ----- 011 ----- 0110011', 'vfcpkc.b.s', F_FF('B_SS'), tags=['fother', 'fp_op'], isa_tags=['f8vecd']),
            Insn('1011001 ----- ----- 111 ----- 0110011', 'vfcpkd.b.s', F_FF('B_SS'), tags=['fother', 'fp_op'], isa_tags=['f8vecd']),

            # # If D extension also supported (implies FLEN>=64)
            # Instr('vfcvt.s.b',  Format_RVF2,'1001100 00111 ----- 000 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f8vecd']),
            # Instr('vfcvt.b.s',  Format_RVF2,'1001100 00100 ----- 011 ----- 0110011', tags=['fconv', 'fp_op'], isa_tags=['f8vecd']),

            # Instr('vfcpka.b# ', Format_RVF4,'1011010 ----- ----- 011 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f8vecd']),
            # Instr('vfcpkb.b.d', Format_RVF4,'1011010 ----- ----- 111 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f8vecd']),
            # Instr('vfcpkc.b.d', Format_RVF4,'1011011 ----- ----- 011 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f8vecd']),
            # Instr('vfcpkd.b.d', Format_RVF4,'1011011 ----- ----- 111 ----- 0110011', tags=['fother', 'fp_op'], isa_tags=['f8vecd']),

            # If Xf16 extension also supported
            Insn('1001100 00111 ----- 010 ----- 0110011', 'vfcvt.h.b',  F_F('H_B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecf16']),
            Insn('1001100 00110 ----- 011 ----- 0110011', 'vfcvt.b.h',  F_F('B_H'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecf16']),

            # If Xf16alt extension also supported
            Insn('1001100 00111 ----- 001 ----- 0110011', 'vfcvt.ah.b', F_F('AH_B'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecf16alt']),
            Insn('1001100 00101 ----- 011 ----- 0110011', 'vfcvt.b.ah', F_F('B_AH'), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['f8vecf16alt']),
        ]

        if inc_vfsum:
            instrs += [
                Insn('1000111 11100 ----- 000 ----- 0110011', 'vfsum.s',         FA_FF('S'),      tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
                Insn('1010111 11100 ----- 000 ----- 0110011', 'vfnsum.s',        FA_FF('S'),      tags=['fadd', 'fp_op'], isa_tags=['f32vec']),
                Insn('1000111 11100 ----- 010 ----- 0110011', 'vfsum.h',         FA_FF('H'),      tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
                Insn('1010111 11100 ----- 010 ----- 0110011', 'vfnsum.h',        FA_FF('H'),      tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
                Insn('1000111 00111 ----- 010 ----- 0110011', 'vfsum.b',         FA_FF('B'),      tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
                Insn('1010111 00111 ----- 010 ----- 0110011', 'vfnsum.b',        FA_FF('B'),      tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
                Insn('1000111 10110 ----- 000 ----- 0110011', 'vfsumex.s.h',     FA_FF('S_HS'),   tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
                Insn('1010111 10110 ----- 000 ----- 0110011', 'vfnsumex.s.h',    FA_FF('S_HS'),   tags=['fadd', 'fp_op'], isa_tags=['f16vec']),
                Insn('1000111 10111 ----- 010 ----- 0110011', 'vfsumex.h.b',     FA_FF('H_BH'),   tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
                Insn('1010111 10111 ----- 010 ----- 0110011', 'vfnsumex.h.b',    FA_FF('H_BH'),   tags=['fadd', 'fp_op'], isa_tags=['f8vec']),
                Insn('1001011 ----- ----- 010 ----- 0110011', 'vfdotpex.h.b',    FA_FFF('H_BBH'), tags=['fadd', 'fp_op'], isa_tags=['f8vec', 'noalt']),
                Insn('1001011 ----- ----- 110 ----- 0110011', 'vfdotpex.h.r.b',  FA_FFF('H_BBH'), tags=['fadd', 'fp_op'], isa_tags=['f8vec', 'noalt']),
                Insn('1011101 ----- ----- 010 ----- 0110011', 'vfndotpex.h.b',   FA_FFF('H_BBH'), tags=['fadd', 'fp_op'], isa_tags=['f8vec', 'noalt']),
                Insn('1011101 ----- ----- 110 ----- 0110011', 'vfndotpex.h.r.b', FA_FFF('H_BBH'), tags=['fadd', 'fp_op'], isa_tags=['f8vec', 'noalt']),

            ]

        super().__init__(name='fvec', instrs=instrs)

    def check_compatibilities(self, isa):
        if not isa.has_isa('rvf') or not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f32vec')

        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f16vec')

        if isa.has_isa('rv32i') and isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16vecno32d')

        if not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16vecd')

        if not isa.has_isa('f16alt'):
            isa.disable_from_isa_tag('f16altvec')

        if not isa.has_isa('f16alt') or not isa.has_isa('rv64i') and isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16altvecno32d')

        if not isa.has_isa('f16alt') or not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f16altvecd')

        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f16altvecf16')

        if not isa.has_isa('f8'):
            isa.disable_from_isa_tag('f8vec')

        if not isa.has_isa('f8') or not isa.has_isa('rv64i') and isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f8vecno32d')

        if not isa.has_isa('f8') or not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('f8vecf')

        if not isa.has_isa('f8') or not isa.has_isa('rvd'):
            isa.disable_from_isa_tag('f8vecd')

        if not isa.has_isa('f8') or not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f8vecf16')

        if not isa.has_isa('f8') or not isa.has_isa('f16alt'):
            isa.disable_from_isa_tag('f8vecf16alt')

class XfvecSnitch(Xfvec):

    def __init__(self):
        super().__init__(inc_vfsum=True)

#
# Auxiliary Float operations
#

class Xfaux(IsaSubset):

    def __init__(self):
        super().__init__(name='faux', instrs=[
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
            Insn('0100110 ----- ----- --- ----- 1010011', 'fmulex.s.h', F_FF('s_hh', ui12_3),    tags=['fmul'], isa_tags=['f16aux']),
            Insn('0101010 ----- ----- --- ----- 1010011', 'fmacex.s.h', FA_FFF('s_hhh', ui12_3), tags=['fmadd'], isa_tags=['f16aux']),

            # If Xfvec supported
            Insn('1001010 ----- ----- 010 ----- 0110011', 'vfdotp.h',        FA_FFF('H'),     tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1001010 ----- ----- 110 ----- 0110011', 'vfdotp.r.h',      FA_FFF('H'),     tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1001011 ----- ----- 000 ----- 0110011', 'vfdotpex.s.h',    FA_FFF('S_HHS'), tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1001011 ----- ----- 100 ----- 0110011', 'vfdotpex.s.r.h',  FA_FFF('S_HHS'), tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1011101 ----- ----- 000 ----- 0110011', 'vfndotpex.s.h',   FA_FFF('S_HHS'), tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1011101 ----- ----- 100 ----- 0110011', 'vfndotpex.s.r.h', FA_FFF('S_HHS'), tags=['fmadd'], isa_tags=['f16auxvec']),
            Insn('1010110 ----- ----- 010 ----- 0110011', 'vfavg.h',         F_FF('H'),       tags=['fadd'], isa_tags=['f16auxvec']),
            Insn('1010110 ----- ----- 110 ----- 0110011', 'vfavg.r.h',       F_FF('H'),       tags=['fadd'], isa_tags=['f16auxvec']),

        #
        # For Xf16aux
        #
            Insn('0100110 ----- ----- 101 ----- 1010011', 'fmulex.s.ah', F_FF('s_ahah'),        tags=['fmul'], isa_tags=['f16altaux']),
            Insn('0101010 ----- ----- 101 ----- 1010011', 'fmacex.s.ah', FA_FFF('s_ahahah'),    tags=['fmadd'], isa_tags=['f16altaux']),

            # If Xfvec supported
            Insn('1001010 ----- ----- 001 ----- 0110011', 'vfdotp.ah',       FA_FFF('AH'),      tags=['fmadd'], isa_tags=['f16altauxvec']),
            Insn('1001010 ----- ----- 101 ----- 0110011', 'vfdotp.r.ah',     FA_FFF('AH'),      tags=['fmadd'], isa_tags=['f16altauxvec']),
            Insn('1001011 ----- ----- 001 ----- 0110011', 'vfdotpex.s.ah',   FA_FFF('S_AHAHS'), tags=['fmadd'], isa_tags=['f16altauxvec']),
            Insn('1001011 ----- ----- 101 ----- 0110011', 'vfdotpex.s.r.ah', FA_FFF('S_AHAHS'), tags=['fmadd'], isa_tags=['f16altauxvec']),
            Insn('1010110 ----- ----- 001 ----- 0110011', 'vfavg.ah',        F_FF('AH'),        tags=['fadd'], isa_tags=['f16altauxvec']),
            Insn('1010110 ----- ----- 101 ----- 0110011', 'vfavg.r.ah',      F_FF('AH'),        tags=['fadd'], isa_tags=['f16altauxvec']),

        #
        # For Xf8
        #
            Insn('0100111 ----- ----- --- ----- 1010011', 'fmulex.s.b', F_FF('s_bb', ui12_3),    tags=['fmul'], isa_tags=['f8aux']),
            Insn('0101011 ----- ----- --- ----- 1010011', 'fmacex.s.b', FA_FFF('s_bbb', ui12_3), tags=['fmadd'], isa_tags=['f8aux']),

            # If Xfvec supported
            Insn('1001010 ----- ----- 011 ----- 0110011', 'vfdotp.b',       FA_FFF('B'),     tags=['fmadd'], isa_tags=['f8auxvec']),
            Insn('1001010 ----- ----- 111 ----- 0110011', 'vfdotp.r.b',     FA_FFF('B'),     tags=['fmadd'], isa_tags=['f8auxvec']),
            Insn('1001011 ----- ----- 011 ----- 0110011', 'vfdotpex.s.b',   FA_FFF('S_BBS'), tags=['fmadd'], isa_tags=['f8auxvec']),
            Insn('1001011 ----- ----- 111 ----- 0110011', 'vfdotpex.s.r.b', FA_FFF('S_BBS'), tags=['fmadd'], isa_tags=['f8auxvec']),
            Insn('1010110 ----- ----- 011 ----- 0110011', 'vfavg.b',        F_FF('B'),       tags=['fadd'], isa_tags=['f8auxvec']),
            Insn('1010110 ----- ----- 111 ----- 0110011', 'vfavg.r.b',      F_FF('B'),       tags=['fadd'], isa_tags=['f8auxvec']),

        ])


    def check_compatibilities(self, isa):
        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f16aux')

        if not isa.has_isa('f16') or not isa.has_isa('fvec'):
            isa.disable_from_isa_tag('f16auxvec')

        if not isa.has_isa('f16'):
            isa.disable_from_isa_tag('f16altaux')

        if not isa.has_isa('f16') or not isa.has_isa('fvec'):
            isa.disable_from_isa_tag('f16altauxvec')

        if not isa.has_isa('f8'):
            isa.disable_from_isa_tag('f8aux')

        if not isa.has_isa('f8') or not isa.has_isa('fvec'):
            isa.disable_from_isa_tag('f8auxvec')
