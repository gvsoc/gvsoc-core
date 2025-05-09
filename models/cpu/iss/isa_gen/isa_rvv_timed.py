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

#
# RV32V
#

# Encodings for vector instruction set

        # 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
# OPV   #   func6    |m|   vs2   |   vs1   |func3|    vd   |     op      #
# OPIVI #   func6    |m|   vs2   |   imm   |func3|    vd   |     op      #
# OPVLS #  NF  |w|mop|m|  lumop  |   rs1   |width|    vd   |     op      #
# OPVLI # 0|         zimm        |   rs1   |1 1 1|    rd   |     op      #

Format_OPV_0 = [ OutVReg     (0, Range(7 , 5)),
               InReg      (0, Range(15, 5))
]

Format_OPV_1 = [
    OutFReg    (0, Range(7 , 5), f='sew'),
    InVRegF    (0, Range(20, 5)),
]

Format_OPV_2 = [ OutVRegF     (0, Range(7 , 5)),
                InFReg     (0, Range(15, 5), f='sew'),
                UnsignedImm(0, Range(25, 1)),
]

Format_OPV_3 = [
    OutVRegF     (0, Range(7 , 5)),
    InVRegF      (0, Range(15, 5)),
    InVRegF      (1, Range(7, 5)),
    InVRegF      (2, Range(20, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_4 = [
    OutVRegF     (0, Range(7 , 5)),
    InFReg       (0, Range(15, 5), f='sew'),
    InVRegF      (1, Range(7, 5)),
    InVRegF      (2, Range(20, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_5 = [
    OutVRegF     (0, Range(7 , 5)),
    InVRegF      (0, Range(15, 5)),
    InVRegF      (1, Range(20, 5)),
    InVRegF      (2, Range(7, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_6 = [
    OutVRegF     (0, Range(7 , 5)),
    InFReg       (0, Range(15, 5), f='sew'),
    InVRegF      (1, Range(20, 5)),
    InVRegF      (2, Range(7, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_7 = [
    OutVRegF     (0, Range(7 , 5)),
    InVRegF      (1, Range(20, 5)),
    InVRegF      (0, Range(15, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_8 = [
    OutVRegF     (0, Range(7 , 5)),
    InVRegF      (1, Range(20, 5)),
    InFReg       (0, Range(15, 5), f='sew'),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_9 = [
    OutVRegF     (0, Range(7 , 5)),
    InVReg       (0, Range(20, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_10 = [
    OutVReg      (0, Range(7 , 5)),
    InVRegF      (0, Range(20, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV_11 = [
    OutVRegF     (0, Range(7 , 5)),
    InVRegF      (0, Range(20, 5)),
    UnsignedImm  (0, Range(25, 1)),
]

Format_OPV = [ OutVReg     (0, Range(7 , 5)),
               InVReg      (0, Range(15, 5)),#rs1/vs1
               InVReg      (1, Range(20, 5)),#vs2
               #UnsignedImm(0, Range(25, 0)),
               UnsignedImm(0, Range(25, 1)),
]

Format_OPVV_F = [ OutVRegF     (0, Range(7 , 5)),
               InVRegF      (0, Range(15, 5)),#rs1/vs1
               InVRegF      (1, Range(20, 5)),#vs2
               #UnsignedImm(0, Range(25, 0)),
               UnsignedImm(0, Range(25, 1)),
]

Format_OPVV2_F = [ OutVRegF     (0, Range(7 , 5)),
               InVRegF      (2, Range(7, 5)),#rs1/vs1
               InVRegF      (0, Range(15, 5)),#rs1/vs1
               InVRegF      (1, Range(20, 5)),#vs2
               #UnsignedImm(0, Range(25, 0)),
               UnsignedImm(0, Range(25, 1)),
]

Format_OPV1 = [ OutVReg     (0, Range(7 , 5)),
               InVReg      (0, Range(15, 5)),#rs1/vs1
               #UnsignedImm(0, Range(25, 0)),
               UnsignedImm(0, Range(25, 1)),
]

Format_OPVI = [ OutVReg     (0, Range(7 , 5)),
               InReg      (0, Range(15, 5)),
               UnsignedImm(0, Range(25, 1)),
               UnsignedImm(1, Range(12, 3)),
]

Format_OPVS = [ InVReg     (1, Range(7 , 5)),
               InReg      (0, Range(15, 5)),
               UnsignedImm(0, Range(25, 1)),
               UnsignedImm(1, Range(12, 3)),
]

Format_OPVF = [ OutVReg     (0, Range(7 , 5)),
                InFReg     (0, Range(15, 5)),#rs1/vs1
                InVReg      (1, Range(20, 5)),#vs2
                #UnsignedImm(0, Range(25, 0)),
                UnsignedImm(0, Range(25, 1)),
]
Format_OPVF_F = [ OutVRegF     (0, Range(7 , 5)),
                InFReg     (0, Range(15, 5), f='sew'),#rs1/vs1
                InVRegF      (1, Range(20, 5)),#vs2
                #UnsignedImm(0, Range(25, 0)),
                UnsignedImm(0, Range(25, 1)),
]
Format_OPVF2_F = [ OutVRegF     (0, Range(7 , 5)),
                InFReg     (0, Range(15, 5), f='sew'),#rs1/vs1
                InVRegF      (1, Range(20, 5)),#vs2
                InVRegF      (2, Range(7, 5)),#vs2
                #UnsignedImm(0, Range(25, 0)),
                UnsignedImm(0, Range(25, 1)),
]
Format_OPVFF = [ OutFReg    (0, Range(7 , 5)),
                 InFReg     (0, Range(15, 5)),#rs1/vs1
                 InVReg      (1, Range(20, 5)),#vs2
                 #UnsignedImm(0, Range(25, 0)),
                 UnsignedImm(0, Range(25, 1)),
]
Format_OPIVI = [ OutVReg     (0, Range(7 , 5)),
                 SignedImm  (0, Range(15, 5)),
                 InReg      (0, Range(20, 5)),
                 UnsignedImm(0, Range(25, 1)),
]
Format_OPVLS = [ OutVReg     (0, Range(7 , 5)),
                 InVReg      (0, Range(15, 5)),
                 UnsignedImm(0, Range(25, 0)),
]
#                           V 0.8
# Format_OPVLI':
#     self.args = [ OutReg     (0, Range(7 , 5)),
#                     InReg      (0, Range(15, 5)),
#                     UnsignedImm(0, Range(20, 2)),# vlmul
#                     UnsignedImm(1, Range(22, 3)),# vsew
#                     UnsignedImm(2, Range(20, 12)),# vtype
#                 ]
#                           V 1.0
Format_OPVLI = [ OutReg     (0, Range(7 , 5)),
                 InReg      (0, Range(15, 5)),
                 UnsignedImm(0, Range(20, 3)),# vlmul
                 UnsignedImm(1, Range(23, 3)),# vsew
                 UnsignedImm(2, Range(20, 12)),# vtype
]
Format_OPVL = [ OutReg     (0, Range(7 , 5)),
                InReg      (0, Range(15, 5)),
                InReg      (1, Range(20, 5)),
]

class Rv32v(IsaSubset):

    def __init__(self):
        super().__init__(name='v', instrs=[
            Instr('vadd.vv'       ,   Format_OPV  ,    '000000 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vadd.vi'       ,   Format_OPIVI,    '000000 - ----- ----- 011 ----- 1010111'),
            Instr('vadd.vx'       ,   Format_OPV  ,    '000000 - ----- ----- 100 ----- 1010111'),

            Instr('vsub.vv'       ,   Format_OPV  ,    '000010 - ----- ----- 000 ----- 1010111'),
            Instr('vsub.vx'       ,   Format_OPV  ,    '000010 - ----- ----- 100 ----- 1010111'),

            Instr('vrsub.vi'      ,   Format_OPIVI,    '000011 - ----- ----- 011 ----- 1010111'),
            Instr('vrsub.vx'      ,   Format_OPV  ,    '000011 - ----- ----- 100 ----- 1010111'),

            Instr('vand.vv'       ,   Format_OPV  ,    '001001 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vand.vi'       ,   Format_OPIVI,    '001001 - ----- ----- 011 ----- 1010111'),
            Instr('vand.vx'       ,   Format_OPV  ,    '001001 - ----- ----- 100 ----- 1010111'),

            Instr('vor.vv'        ,   Format_OPV  ,    '001010 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vor.vi'        ,   Format_OPIVI,    '001010 - ----- ----- 011 ----- 1010111'),
            Instr('vor.vx'        ,   Format_OPV  ,    '001010 - ----- ----- 100 ----- 1010111'),

            Instr('vxor.vv'       ,   Format_OPV  ,    '001011 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vxor.vi'       ,   Format_OPIVI,    '001011 - ----- ----- 011 ----- 1010111'),
            Instr('vxor.vx'       ,   Format_OPV  ,    '001011 - ----- ----- 100 ----- 1010111'),

            Instr('vmin.vv'       ,   Format_OPV  ,    '000101 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmin.vx'       ,   Format_OPV  ,    '000101 - ----- ----- 100 ----- 1010111'),

            Instr('vminu.vv'      ,   Format_OPV  ,    '000100 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vminu.vx'      ,   Format_OPV  ,    '000100 - ----- ----- 100 ----- 1010111'),

            Instr('vmax.vv'       ,   Format_OPV  ,    '000111 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmax.vx'       ,   Format_OPV  ,    '000111 - ----- ----- 100 ----- 1010111'),

            Instr('vmaxu.vv'      ,   Format_OPV  ,    '000110 - ----- ----- 000 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmaxu.vx'      ,   Format_OPV  ,    '000110 - ----- ----- 100 ----- 1010111'),

            Instr('vmul.vv'       ,   Format_OPV  ,    '100101 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmul.vx'       ,   Format_OPV  ,    '100101 - ----- ----- 110 ----- 1010111'),

            Instr('vmulh.vv'      ,   Format_OPV  ,    '100111 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmulh.vx'      ,   Format_OPV  ,    '100111 - ----- ----- 110 ----- 1010111'),

            Instr('vmulhu.vv'     ,   Format_OPV  ,    '100100 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmulhu.vx'     ,   Format_OPV  ,    '100100 - ----- ----- 110 ----- 1010111'),

            Instr('vmulhsu.vv'    ,   Format_OPV  ,    '100110 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vmulhsu.vx'    ,   Format_OPV  ,    '100110 - ----- ----- 110 ----- 1010111'),

            Instr('vmerge.vvm'    ,   Format_OPV  ,    '010111 0 ----- ----- 000 ----- 1010111'),

            Instr('vmv.v.v'       ,   Format_OPV1  ,    '010111 1 ----- ----- 000 ----- 1010111'),
            Instr('vmv.v.i'       ,   Format_OPIVI,    '010111 - ----- ----- 011 ----- 1010111'),
            Instr('vmv.v.x'       ,   Format_OPV  ,    '010111 - ----- ----- 100 ----- 1010111'),
            Instr('vmv.s.x'       ,   Format_OPV_0  ,    '010000 - 00000 ----- 110 ----- 1010111'),
            Instr('vmv.x.s'       ,   Format_OPV  ,    '010000 - ----- 00000 010 ----- 1010111'),


            Instr('vwmul.vv'      ,   Format_OPV  ,    '111011 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vwmul.vx'      ,   Format_OPV  ,    '111011 - ----- ----- 110 ----- 1010111'),

            Instr('vwmulu.vv'     ,   Format_OPV  ,    '111000 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vwmulu.vx'     ,   Format_OPV  ,    '111000 - ----- ----- 110 ----- 1010111'),

            Instr('vwmulsu.vv'    ,   Format_OPV  ,    '111010 - ----- ----- 010 ----- 1010111'),#inst[25] = VM , VM = 0 mask enable
            Instr('vwmulsu.vx'    ,   Format_OPV  ,    '111010 - ----- ----- 110 ----- 1010111'),

            Instr('vmacc.vv'      ,   Format_OPV  ,    '101101 - ----- ----- 010 ----- 1010111'),
            Instr('vmacc.vx'      ,   Format_OPV  ,    '101101 - ----- ----- 110 ----- 1010111'),

            Instr('vmadd.vv'      ,   Format_OPV  ,    '101001 - ----- ----- 010 ----- 1010111'),
            Instr('vmadd.vx'      ,   Format_OPV  ,    '101001 - ----- ----- 110 ----- 1010111'),

            Instr('vnmsac.vv'     ,   Format_OPV  ,    '101111 - ----- ----- 010 ----- 1010111'),
            Instr('vnmsac.vx'     ,   Format_OPV  ,    '101111 - ----- ----- 110 ----- 1010111'),

            Instr('vnmsub.vv'     ,   Format_OPV  ,    '101011 - ----- ----- 010 ----- 1010111'),
            Instr('vnmsub.vx'     ,   Format_OPV  ,    '101011 - ----- ----- 110 ----- 1010111'),

            Instr('vwmacc.vv'     ,   Format_OPV  ,    '111101 - ----- ----- 010 ----- 1010111'),
            Instr('vwmacc.vx'     ,   Format_OPV  ,    '111101 - ----- ----- 110 ----- 1010111'),

            Instr('vwmaccu.vv'    ,   Format_OPV  ,    '111100 - ----- ----- 010 ----- 1010111'),
            Instr('vwmaccu.vx'    ,   Format_OPV  ,    '111100 - ----- ----- 110 ----- 1010111'),

            Instr('vwmaccus.vx'   ,   Format_OPV  ,    '111110 - ----- ----- 110 ----- 1010111'),

            Instr('vwmaccsu.vv'   ,   Format_OPV  ,    '111111 - ----- ----- 010 ----- 1010111'),
            Instr('vwmaccsu.vx'   ,   Format_OPV  ,    '111111 - ----- ----- 110 ----- 1010111'),

            Instr('vredsum.vs'    ,   Format_OPV  ,    '000000 - ----- ----- 010 ----- 1010111'),
            Instr('vredand.vs'    ,   Format_OPV  ,    '000001 - ----- ----- 010 ----- 1010111'),
            Instr('vredor.vs'     ,   Format_OPV  ,    '000010 - ----- ----- 010 ----- 1010111'),
            Instr('vredxor.vs'    ,   Format_OPV  ,    '000011 - ----- ----- 010 ----- 1010111'),
            Instr('vredminu.vs'   ,   Format_OPV  ,    '000100 - ----- ----- 010 ----- 1010111'),
            Instr('vredmin.vs'    ,   Format_OPV  ,    '000101 - ----- ----- 010 ----- 1010111'),
            Instr('vredmaxu.vs'   ,   Format_OPV  ,    '000110 - ----- ----- 010 ----- 1010111'),
            Instr('vredmax.vs'    ,   Format_OPV  ,    '000111 - ----- ----- 010 ----- 1010111'),


            Instr('vslideup.vi'   ,   Format_OPIVI,    '001110 - ----- ----- 011 ----- 1010111'),
            Instr('vslideup.vx'   ,   Format_OPV  ,    '001110 - ----- ----- 100 ----- 1010111'),

            Instr('vslidedown.vi' ,   Format_OPIVI,    '001111 - ----- ----- 011 ----- 1010111'),
            Instr('vslidedown.vx' ,   Format_OPV  ,    '001111 - ----- ----- 100 ----- 1010111'),

            Instr('vslide1up.vx'  ,   Format_OPV  ,    '001110 - ----- ----- 110 ----- 1010111'),
            Instr('vslide1down.vx',   Format_OPV  ,    '001111 - ----- ----- 110 ----- 1010111'),

            Instr('vdiv.vv'       ,   Format_OPV  ,    '100001 - ----- ----- 010 ----- 1010111'),
            Instr('vdiv.vx'       ,   Format_OPV  ,    '100001 - ----- ----- 110 ----- 1010111'),

            Instr('vdivu.vv'      ,   Format_OPV  ,    '100000 - ----- ----- 010 ----- 1010111'),
            Instr('vdivu.vx'      ,   Format_OPV  ,    '100000 - ----- ----- 110 ----- 1010111'),

            Instr('vrem.vv'       ,   Format_OPV  ,    '100011 - ----- ----- 010 ----- 1010111'),
            Instr('vrem.vx'       ,   Format_OPV  ,    '100011 - ----- ----- 110 ----- 1010111'),

            Instr('vremu.vv'      ,   Format_OPV  ,    '100010 - ----- ----- 010 ----- 1010111'),
            Instr('vremu.vx'      ,   Format_OPV  ,    '100010 - ----- ----- 110 ----- 1010111'),





            Instr('vfadd.vv'      ,   Format_OPV_7  ,    '000000 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfadd.vf'      ,   Format_OPV_8 ,    '000000 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfsub.vv'      ,   Format_OPV_7  ,    '000010 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfsub.vf'      ,   Format_OPV_8 ,    '000010 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfrsub.vf'     ,   Format_OPV_8  ,    '100111 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmin.vv'      ,   Format_OPV_7  ,    '000100 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmin.vf'      ,   Format_OPV_8 ,    '000100 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmax.vv'      ,   Format_OPV_7  ,    '000110 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmax.vf'      ,   Format_OPV_8 ,    '000110 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmul.vv'      ,   Format_OPV_7  ,    '100100 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmul.vf'      ,   Format_OPV_8 ,    '100100 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmacc.vv'     ,   Format_OPV_5  ,    '101100 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmacc.vf'     ,   Format_OPV_6 ,    '101100 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfnmacc.vv'    ,   Format_OPV_5  ,    '101101 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfnmacc.vf'    ,   Format_OPV_6 ,    '101101 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmsac.vv'     ,   Format_OPV_5  ,    '101110 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmsac.vf'     ,   Format_OPV_6 ,    '101110 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfnmsac.vv'    ,   Format_OPV_5  ,    '101111 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfnmsac.vf'    ,   Format_OPV_6 ,    '101111 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmadd.vv'     ,   Format_OPV_3  ,    '101000 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmadd.vf'     ,   Format_OPV_4 ,    '101000 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfnmadd.vv'    ,   Format_OPV_3  ,    '101001 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfnmadd.vf'    ,   Format_OPV_4 ,    '101001 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfmsub.vv'     ,   Format_OPV_3  ,    '101010 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfmsub.vf'     ,   Format_OPV_4 ,    '101010 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfnmsub.vv'    ,   Format_OPV_3  ,    '101011 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfnmsub.vf'    ,   Format_OPV_4 ,    '101011 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfredmax.vs'      ,   Format_OPV  ,    '000111 - ----- ----- 001 ----- 1010111', tags=['fp_op']),

            Instr('vfredmin.vs'      ,   Format_OPV  ,    '000101 - ----- ----- 001 ----- 1010111', tags=['fp_op']),

            Instr('vfredsum.vs'      ,   Format_OPVV_F  ,    '000001 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfredosum.vs'     ,   Format_OPV  ,    '000011 - ----- ----- 001 ----- 1010111', tags=['fp_op']),

            Instr('vfwadd.vv'        ,   Format_OPV  ,    '110000 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwadd.vf'        ,   Format_OPVF ,    '110000 - ----- ----- 101 ----- 1010111', tags=['fp_op']),
            Instr('vfwadd.wv'        ,   Format_OPV  ,    '110100 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwadd.wf'        ,   Format_OPVF ,    '110100 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfwsub.vv'        ,   Format_OPV  ,    '110010 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwsub.vf'        ,   Format_OPVF ,    '110010 - ----- ----- 101 ----- 1010111', tags=['fp_op']),
            Instr('vfwsub.wv'        ,   Format_OPV  ,    '110110 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwsub.wf'        ,   Format_OPVF ,    '110110 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfwmul.vv'        ,   Format_OPV  ,    '111000 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwmul.vf'        ,   Format_OPVF ,    '111000 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfwmacc.vv'       ,   Format_OPV  ,    '111100 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwmacc.vf'       ,   Format_OPVF ,    '111100 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfwmsac.vv'       ,   Format_OPV  ,    '111110 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwmsac.vf'       ,   Format_OPVF ,    '111110 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfwnmsac.vv'      ,   Format_OPV  ,    '111111 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfwnmsac.vf'      ,   Format_OPVF ,    '111111 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfsgnj.vv'        ,   Format_OPV  ,    '001000 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfsgnj.vf'        ,   Format_OPVF ,    '001000 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfsgnjn.vv'       ,   Format_OPV  ,    '001001 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfsgnjn.vf'       ,   Format_OPVF ,    '001001 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfsgnjx.vv'       ,   Format_OPV  ,    '001010 - ----- ----- 001 ----- 1010111', tags=['fp_op']),
            Instr('vfsgnjx.vf'       ,   Format_OPVF ,    '001010 - ----- ----- 101 ----- 1010111', tags=['fp_op']),

            Instr('vfcvt.xu.f.v'     ,   Format_OPV_10  ,    '010010 - ----- 00000 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfcvt.x.f.v'      ,   Format_OPV_10  ,    '010010 - ----- 00001 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfcvt.f.xu.v'     ,   Format_OPV_9  ,    '010010 - ----- 00010 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfcvt.f.x.v'      ,   Format_OPV_9  ,    '010010 - ----- 00011 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfcvt.rtz.xu.f.v' ,   Format_OPV_10  ,    '010010 - ----- 00110 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfcvt.rtz.x.f.v'  ,   Format_OPV_10  ,    '010010 - ----- 00111 001 ----- 1010111', tags=['fp_op', 'nseq']),

            Instr('vfncvt.xu.f.w'    ,   Format_OPV_10  ,    '010010 - ----- 10000 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.x.f.w'     ,   Format_OPV_10  ,    '010010 - ----- 10001 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.f.xu.w'    ,   Format_OPV_9  ,    '010010 - ----- 10010 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.f.x.w'     ,   Format_OPV_9  ,    '010010 - ----- 10011 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.f.f.w'     ,   Format_OPV_11 ,    '010010 - ----- 10100 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.rod.f.f.w' ,   Format_OPV_11 ,    '010010 - ----- 10101 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.rtz.xu.f.w',   Format_OPV_10  ,    '010010 - ----- 10110 001 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfncvt.rtz.x.f.w' ,   Format_OPV_10  ,    '010010 - ----- 10111 001 ----- 1010111', tags=['fp_op', 'nseq']),

            Instr('vfslide1down.vf' , Format_OPVF_F, '001111 - ----- ----- 101 ----- 1010111', tags=['fp_op', 'nseq']),



            Instr('vfmv.v.f'         ,   Format_OPV_2 ,    '010111 - ----- ----- 101 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfmv.s.f'         ,   Format_OPVF ,    '010000 - 00000 ----- 101 ----- 1010111', tags=['fp_op', 'nseq']),
            Instr('vfmv.f.s'         ,   Format_OPV_1,    '010000 - ----- 00000 001 ----- 1010111', tags=['fp_op', 'nseq']),

            Instr('vle8.v'           ,   Format_OPVI  ,    '000 0 00 - 00000 ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            Instr('vle16.v'          ,   Format_OPVI  ,    '000 0 00 - 00000 ----- 101 ----- 0000111', tags=['vload']),
            Instr('vle32.v'          ,   Format_OPVI  ,    '000 0 00 - 00000 ----- 110 ----- 0000111', tags=['vload']),
            Instr('vle64.v'          ,   Format_OPVI  ,    '000 0 00 - 00000 ----- 111 ----- 0000111', tags=['vload']),

            Instr('vse8.v'           ,   Format_OPVS  ,    '000 0 00 - 00000 ----- 000 ----- 0100111', tags=['vstore']),
            Instr('vse16.v'          ,   Format_OPVS  ,    '000 0 00 - 00000 ----- 101 ----- 0100111', tags=['vstore']),
            Instr('vse32.v'          ,   Format_OPVS  ,    '000 0 00 - 00000 ----- 110 ----- 0100111', tags=['vstore']),
            Instr('vse64.v'          ,   Format_OPVS  ,    '000 0 00 - 00000 ----- 111 ----- 0100111', tags=['vstore']),

            Instr('vluxei8.v'        ,   Format_OPV  ,    '000 0 01 - ----- ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            Instr('vluxei16.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 101 ----- 0000111', tags=['vload']),
            Instr('vluxei32.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 110 ----- 0000111', tags=['vload']),
            Instr('vluxei64.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 111 ----- 0000111', tags=['vload']),

            Instr('vsuxei8.v'        ,   Format_OPV  ,    '000 0 01 - ----- ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm
            Instr('vsuxei16.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 101 ----- 0100111', tags=['vstore']),
            Instr('vsuxei32.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 110 ----- 0100111', tags=['vstore']),
            Instr('vsuxei64.v'       ,   Format_OPV  ,    '000 0 01 - ----- ----- 111 ----- 0100111', tags=['vstore']),

            Instr('vlse8.v'          ,   Format_OPV  ,    '000 0 10 - ----- ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            Instr('vlse16.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 101 ----- 0000111', tags=['vload']),
            Instr('vlse32.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 110 ----- 0000111', tags=['vload']),
            Instr('vlse64.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 111 ----- 0000111', tags=['vload']),

            Instr('vsse8.v'          ,   Format_OPV  ,    '000 0 10 - ----- ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm
            Instr('vsse16.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 101 ----- 0100111', tags=['vstore']),
            Instr('vsse32.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 110 ----- 0100111', tags=['vstore']),
            Instr('vsse64.v'         ,   Format_OPV  ,    '000 0 10 - ----- ----- 111 ----- 0100111', tags=['vstore']),


            Instr('vl1r.v'       ,   Format_OPV  ,    '000 0 001 01000 ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            # Instr('vl1re8.v'     ,   Format_OPV  ,    '000 0 001 01000 ----- 000 ----- 0000111'),# vd, (rs1), vm
            Instr('vl1re16.v'    ,   Format_OPV  ,    '000 0 001 01000 ----- 101 ----- 0000111', tags=['vload']),
            Instr('vl1re32.v'    ,   Format_OPV  ,    '000 0 001 01000 ----- 110 ----- 0000111', tags=['vload']),
            Instr('vl1re64.v'    ,   Format_OPV  ,    '000 0 001 01000 ----- 111 ----- 0000111', tags=['vload']),

            Instr('vl2r.v'       ,   Format_OPV  ,    '001 0 001 01000 ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            # Instr('vl2re8.v'     ,   Format_OPV  ,    '001 0 001 01000 ----- 000 ----- 0000111'),# vd, (rs1), vm
            Instr('vl2re16.v'    ,   Format_OPV  ,    '001 0 001 01000 ----- 101 ----- 0000111', tags=['vload']),
            Instr('vl2re32.v'    ,   Format_OPV  ,    '001 0 001 01000 ----- 110 ----- 0000111', tags=['vload']),
            Instr('vl2re64.v'    ,   Format_OPV  ,    '001 0 001 01000 ----- 111 ----- 0000111', tags=['vload']),

            Instr('vl4r.v'       ,   Format_OPV  ,    '011 0 001 01000 ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            # Instr('vl4re8.v'     ,   Format_OPV  ,    '011 0 001 01000 ----- 000 ----- 0000111'),# vd, (rs1), vm
            Instr('vl4re16.v'    ,   Format_OPV  ,    '011 0 001 01000 ----- 101 ----- 0000111', tags=['vload']),
            Instr('vl4re32.v'    ,   Format_OPV  ,    '011 0 001 01000 ----- 110 ----- 0000111', tags=['vload']),
            Instr('vl4re64.v'    ,   Format_OPV  ,    '011 0 001 01000 ----- 111 ----- 0000111', tags=['vload']),

            Instr('vl8r.v'       ,   Format_OPV  ,    '111 0 001 01000 ----- 000 ----- 0000111', tags=['vload']),# vd, (rs1), vm
            # Instr('vl8re8.v'     ,   Format_OPV  ,    '111 0 001 01000 ----- 000 ----- 0000111'),# vd, (rs1), vm
            Instr('vl8re16.v'    ,   Format_OPV  ,    '111 0 001 01000 ----- 101 ----- 0000111', tags=['vload']),
            Instr('vl8re32.v'    ,   Format_OPV  ,    '111 0 001 01000 ----- 110 ----- 0000111', tags=['vload']),
            Instr('vl8re64.v'    ,   Format_OPV  ,    '111 0 001 01000 ----- 111 ----- 0000111', tags=['vload']),


            Instr('vs1r.v'       ,   Format_OPV  ,    '000 0 001 01000 ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm
            Instr('vs2r.v'       ,   Format_OPV  ,    '001 0 001 01000 ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm
            Instr('vs4r.v'       ,   Format_OPV  ,    '011 0 001 01000 ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm
            Instr('vs8r.v'       ,   Format_OPV  ,    '111 0 001 01000 ----- 000 ----- 0100111', tags=['vstore']),# vd, (rs1), vm

            #                           V 1.0
            Instr('vsetvli' ,   Format_OPVLI,    '- ----------- ----- 111 ----- 1010111'), # zimm = {3'b000,vma,vta,vsew[2:0],vlmul[2:0]}

            #                           V 0.8
            # Instr('vsetvli' ,   'OPVLI',    '0 ----------- ----- 111 ----- 1010111'), # zimm = {7'b0000000,vsew[2:0],vlmul[1:0]}
            Instr('vsetvl'  ,   Format_OPVL ,    '1000000 ----- ----- 111 ----- 1010111'),

            #Instr('csrr', Format_IU,  '------- ----- 00000 010 ----- 1110011', decode='csr_decode'),
        ])
