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
from cpu.iss.isa_gen.isa_rvv_timed import *
import os.path
import filecmp
import shutil
import hashlib


def extract_format(f, index):
    if f == '' or f is None: return None
    return f[0] if index >= len(f) else f[index]

ui12_3 = UnsignedImm(0, Range(12, 3))
"""An unsigned immediate generated from [12:3].
"""

def F_F(f=None, i=[]):
    """Format for 1 float output and 1 float input.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'x_x'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 0)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
    ] + i

def R_F(f=None, i=[]):
    """Format for 1 output and 1 float input.

    Parameters
    ----------
    f : str
        Format string. Can be 'x'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutReg(0, Range(7,  5)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
    ] + i

def F_R(f=None, i=[]):
    """Format for 1 float output and 1 input.

    Parameters
    ----------
    f : str
        Format string. Can be 'x'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 2)),
        InReg (0, Range(15, 5)),
    ] + i

def F_FF(f=None, i=[]):
    """Format for 1 float output and 2 float inputs.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'x_xx'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 0)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
        InFReg (1, Range(20, 5), f=extract_format(f, 3)),
    ] + i

def R_FF(f=None, i=[]):
    """Format for 1 output and 2 float inputs.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'xx'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutReg(0, Range(7,  5)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
        InFReg (1, Range(20, 5), f=extract_format(f, 3)),
    ] + i

def F_FFF(f:str='', i=[]) -> list:
    """Format for 1 float output and 3 float inputs.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'x_xxx'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 0)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
        InFReg (1, Range(20, 5), f=extract_format(f, 3)),
        InFReg (2, Range(27,  5), f=extract_format(f, 4)),
    ] + i

def FA_FF(f=None, i=[]):
    """Format for 1 float output and 2 float inputs. Last input is same as output.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'x_xxx'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 0)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
        InFReg (1, Range(7,  5), f=extract_format(f, 3), dumpName=False),
    ] + i

def FA_FFF(f=None, i=[]):
    """Format for 1 float output and 3 float inputs. Last input is same as output.

    Parameters
    ----------
    f : str
        Format string. Can be 'x' or 'x_xxx'. x can be d, s, h, ah, b or ab.
    i : List
        immediates to be extracted
    """
    if not isinstance(i, list):
        i = [i]
    return [
        OutFReg(0, Range(7,  5), f=extract_format(f, 0)),
        InFReg (0, Range(15, 5), f=extract_format(f, 2)),
        InFReg (1, Range(20, 5), f=extract_format(f, 3)),
        InFReg (2, Range(7,  5), f=extract_format(f, 4), dumpName=False),
    ] + i

def FL(f:str=''):
    """Format for float loads with 1 float ouput, 1 indirect intput. Indirect offset generated
    from [20:12].

    Parameters
    ----------
    f : str
        Format string. Can only be 'x'. x can be d, s, h, ah, b or ab.
    """
    return [
        OutFReg(0, Range(7,  5), f=f),
        Indirect(InReg (0, Range(15, 5)),
        SignedImm(0, Range(20, 12))),
    ]

def FS(f=None):
    """Format for float stores with 1 indirect intput and 1 float input. Indirect offset generated
    from [7:5], [25:5].

    Parameters
    ----------
    f : str
        Format string. Can only be 'x'. x can be d, s, h, ah, b or ab.
    """
    return [
        InFReg(1, Range(20, 5), f=f),
        Indirect(InReg(0, Range(15, 5)),
        SignedImm(0, Ranges([[7, 5, 0], [25, 7, 5]]))),
    ]










# Encodings for non-compressed instruction sets
        #   3 3 2 2 2 2 2       2 2 2 2 2       1 1 1 1 1       1 1 1       1 1
        #   1 0 9 8 7 6 5       4 3 2 1 0       9 8 7 6 5       4 3 2       1 0 9 8 7       6 5 4 3 2 1 0
        #   X X X X X X X   |   X X X X X   |   X X X X X   |   X X X   |   X X X X X   |   X X X X X X X
# R   #         f7        |      rs2      |      rs1      |     f3    |       rd      |      opcode
# RF  #         f7        |      rs2      |      rs1      |  ui[2:0]  |       rd      |      opcode
# RF4 #         f7        |      rs2      |      rs1      |  ui[2:0]  |     rd/rs3    |      opcode
# R2F #                  f7               |      rs1      |  ui[2:0]  |       rd      |      opcode
# R3F #                  f7               |      rs1      |     f3    |       rd      |      opcode
# R4U #    rs3     |  f2  |      rs2      |      rs1      |  ui[2:0]  |       rd      |      opcode
# RVF #         f7        |      rs2      |      rs1      |     f3    |       rd      |      opcode
# RVF2#                  f7               |      rs1      |     f3    |       rd      |      opcode
# RVF4#         f7        |      rs2      |      rs1      |     f3    |     rd/rs3    |      opcode
# RRRR#         f7        |      rs2      |      rs1      |     f3    |     rd/rs3    |      opcode
# RRRS#      f6      |     si[0|5:1]      |      rs2      |     f3    |     rd/rs1    |      opcode
# RRRU#      f6      |     ui[0|5:1]      |      rs2      |     f3    |     rd/rs1    |      opcode
#RRRU2#   f2 |   ui[4:0]  |      rs2      |      rs1      |     f3    |       rd      |      opcode
#RRRRU#   f2 |   ui[4:0]  |      rs2      |      rs1      |     f3    |     rd/rs3    |      opcode
# R1  #         f7        |       -       |      rs1      |     f3    |       rd      |      opcode
# RRU #      f6      |     ui[0|5:1]      |      rs1      |     f3    |       rd      |      opcode
# RRS #      f6      |     si[0|5:1]      |      rs1      |     f3    |       rd      |      opcode
# RRU2#     f3 |ui[7:6]|f1|ui[0|5:1]      |      rs1      |     f3    |       rd      |      opcode
# LR  #         f7        |      rs2      |      rs1      |     f3    |       rd      |      opcode        # Indirect addressing mode
# RR  #   f2 |     rs3    |      rs2      |      rs1      |     f3    |       rd      |      opcode
# SR  #   f2 |     rs3    |      rs2      |      rs1      |     f3    |       rd      |      opcode        # Indirect addressing mode
# I   #              si[11:0]             |      rs1      |     f3    |       rd      |      opcode
# L   #              si[11:0]             |      rs1      |     f3    |       rd      |      opcode        # Indirect addressing mode
# IU  #              ui[11:0]             |      rs1      |     f3    |       rd      |      opcode
# I1U #         f7        |    ui[4:0]    |      rs1      |     f3    |       rd      |      opcode
# I2U #             ui0[11:0]             |   ui1[4:0]    |     f3    |       rd      |      opcode
# I3U #    f4    |          ui[7:0]       |                    f13                    |      opcode
# I4U #   f2 | ui0[4:0]   |   ui1[4:0]    |      rs1      |     f3    |       rd      |      opcode
# I5U #   f2 | ui0[4:0]   |   ui1[4:0]    |      rs1      |     f3    |     rs2/rd    |      opcode
# IOU #                  f12              |   ui0[4:0]    |     f3    |       f5      |      opcode
# S   #       si[11:5]    |      rs2      |      rs1      |     f3    |     si[4:0]   |      opcode        # Indirect addressing mode
# S1  #   f2 |     rs3    |      rs2      |      rs1      |     f3    |       f5      |      opcode
# SB  #     si[12|10:5]   |      rs2      |      rs1      |     f3    |   si[4:1|11]  |      opcode
# SB2 #     si[12|10:5]   |    si[4:0]    |      rs1      |     f3    |   si[4:1|11]  |      opcode
# U   #                           ui[31:12]                           |       rd      |      opcode
# UJ  #                    si[20|10:1|11|19:15|14:12]                 |       rd      |      opcode
# HL0 #              ui1[11:0]            |      rs1      |     f3    |  opcode  |ui0[0]|      opcode
# HL1 #              ui1[11:0]            |    ui2[4:0]   |     f3    |  opcode  |ui0[0]|      opcode
# Z   #                 -                 |       -       |     f3    |        -      |      opcode

Format_R = [
    OutReg(0, Range(7,  5)),
    InReg (0, Range(15, 5)),
    InReg (1, Range(20, 5)),
]

Format_F = [
    InReg (0, Range(15, 5)),
]

Format_I = [
    OutReg(0, Range(7,  5)),
    InReg (0, Range(15, 5)),
    SignedImm(0, Range(20, 12)),
]

Format_Z = []

Format_L = [
    OutReg(0, Range(7,  5)),
    Indirect(InReg (0, Range(15, 5)),
    SignedImm(0, Range(20, 12))),
]

Format_LRES = [
    OutReg(0, Range(7,  5)),
    InReg (0, Range(15, 5)),
    UnsignedImm(0, Range(25, 1)),
    UnsignedImm(1, Range(26, 1)),
]

Format_IU = [
    OutReg(0, Range(7,  5)),
    InReg (0, Range(15, 5)),
    UnsignedImm(0, Range(20, 12)),
]

Format_IUR = [
    OutReg(0, Range(7,  5)),
    UnsignedImm(1, Range(15, 5)),
    UnsignedImm(0, Range(20, 12)),
]

Format_I1U = [
    OutReg(0, Range(7, 5)),
    InReg(0, Range(15, 5)),
    UnsignedImm(0, Range(20, 5)),
]

Format_I1U64 = [
    OutReg(0, Range(7, 5)),
    InReg(0, Range(15, 5)),
    UnsignedImm(0, Range(20, 6)),
]

Format_I3U = [
    UnsignedImm(0, Range(20, 8)),
]

Format_S = [
    InReg(1, Range(20, 5)),
    Indirect(InReg(0, Range(15, 5)),
    SignedImm(0, Ranges([[7, 5, 0], [25, 7, 5]]))),
]

Format_AMO = [
    OutReg(0, Range(7, 5)),
    InReg(1, Range(20, 5)),
    InReg(0, Range(15, 5)),
    UnsignedImm(0, Range(25, 1)),
    UnsignedImm(1, Range(26, 1)),
]

Format_INRR = [
    InReg(0, Range(15, 5)),
    InReg(1, Range(20, 5)),
]

Format_SB = [
    InReg(0, Range(15, 5)),
    InReg(1, Range(20, 5)),
    SignedImm(0, Ranges([[7, 1, 11], [8, 4, 1], [25, 6, 5], [31, 1, 12]])),
]

Format_U = [
    OutReg(0, Range(7, 5)),
    UnsignedImm(0, Range(12, 20, 12)),
]

Format_UJ = [
    OutReg(0, Range(7, 5)),
    SignedImm(0, Ranges([[12, 8, 12], [20, 1, 11], [21, 10, 1], [31, 1, 20]])),
]

Format_CMPUSH = [
    UnsignedImm(0, Range(4, 4)),
    UnsignedImm(1, Range(2, 2)),
]

# Encodings for compressed instruction set

       # X   X   X   X   X   X   X   X   X   X   X   X   X   X   X   X
# CR   #      func4    |      rd/rs1       |         rs2       |  op      #
# CR1  #      func4    |        rs1        |          f5       |  op      # rs2=0, rd=0, si=0
# CR2  #      func4    |        rd         |         rs2       |  op      # rs1=0
# CR3  #      func4    |        rs1        |         rs2       |  op      # rd=1, si=0
# CI1  #   func3 | si[5]|     rd/rs1       |      si[4:0]      |  op      #
# CI1U #   func3 | ui[5]|     rd/rs1       |      ui[4:0]      |  op      #
# CI3  #   func3 | ui[5]|       rd         |     ui[4:2|7:6]   |  op      # rs1=2, ui->si
# CI4  #   func3 | si[9]|      func5       |    si[4|6|8:7|5]  |  op      # rs1=2, rd=2
# CI5  #   func3 |si[17]|     rd/rs1       |      si[16:12]    |  op      # si->ui
# CI6  #   func3 | si[5]|       rd         |      si[4:0]      |  op      # rs1=0
# CSS  #   func3   |      ui[5:2|7:6]      |         rs2       |  op      # rs1=2, ui->si
# CIW  #   func3   |        ui[5:4|9:6|2|3]        |    rd'    |  op      # rs1=2, ui->si
# CL   #   func3   |      ui   |   rs1'    |   ui  |    rd'    |  op      # ui->si
# CL1  #                                   op                             # rd=0
# CS   #   func3   |  ui[5:3]  |   rs1'    |ui[6|2]|    rs2'   |  op      # ui->si
# CS2  #      func6            |  rd/rs1   | func  |    rs2    |  op      #
# CB1  #   func3   | si[8|4:3] |   rs1'    |   si[7:6|2:1|5]   |  op      # rs2=0
# CB2  #   func3 |ui[5]| func2 |  rd/rs1'  |       ui[4:0]     |  op      #
# CB2S #   func3 |si[5]| func2 |  rd/rs1'  |       si[4:0]     |  op      #
# CJ   #   func3   |         si[11|4|9:8|6|7|3:1|5]            |  op      # rd=0
# CJ1  #   func3   |         si[11|4|9:8|6|7|3:1|5]            |  op      # rd=1

Format_CR = [
    OutReg(0, Range(7, 5)),
    InReg(0, Range(7, 5)),
    InReg(1, Range(2, 5)),
]

Format_CR1 = [
    OutReg(0, Const(0)),
    InReg(0, Range(7, 5)),
    InReg(1, Const(0)),
    SignedImm(0, Const(0)),
]

Format_CR2 = [
    OutReg(0, Range(7, 5)),
    InReg(0, Const(0)),
    InReg(1, Range(2, 5)),
]

Format_CR3 = [
    OutReg(0, Const(1)),
    InReg(0, Range(7, 5)),
    InReg(1, Range(2, 5)),
    SignedImm(0, Const(0)),
]

Format_CI1 = [
    OutReg(0, Range(7, 5)),
    InReg(0, Range(7, 5)),
    SignedImm(0, Ranges([[2, 5, 0], [12, 1, 5]])),
]

Format_CI1U = [
    OutReg(0, Range(7, 5)),
    InReg(0, Range(7, 5)),
    UnsignedImm(0, Ranges([[2, 5, 0], [12, 1, 5]])),
]

Format_CI3 = [
    OutReg(0, Range(7, 5)),
    Indirect(InReg(0, Const(2)),
    SignedImm(0, Ranges([[4, 3, 2], [12, 1, 5], [2, 2, 6]]), isSigned=False)),
]

Format_DCI3 = [
    OutReg(0, Range(7, 5)),
    Indirect(InReg(0, Const(2)),
    SignedImm(0, Ranges([[5, 2, 3], [12, 1, 5], [2, 3, 6]]), isSigned=False)),
]

Format_FCI3 = [
    OutFReg(0, Range(7, 5)),
    Indirect(InReg(0, Const(2)),
    SignedImm(0, Ranges([[4, 3, 2], [12, 1, 5], [2, 2, 6]]), isSigned=False)),
]

Format_FCI3D = [
    OutFReg(0, Range(7, 5)),
    Indirect(InReg(0, Const(2)),
    SignedImm(0, Ranges([[5, 2, 3], [12, 1, 5], [2, 3, 6]]), isSigned=False)),
]

Format_CI4 = [
    OutReg(0, Const(2)),
    InReg(0, Const(2)),
    SignedImm(0, Ranges([[6, 1, 4], [2, 1, 5], [5, 1, 6], [3, 2, 7], [12, 1, 9]])),
]

Format_CI5 = [
    OutReg(0, Range(7, 5)),
    UnsignedImm(0, Ranges([[2, 5, 12], [12, 1, 17]]), isSigned=True),
]

Format_CI6 = [
    OutReg(0, Range(7, 5)),
    InReg(0, Const(0)),
    SignedImm(0, Ranges([[2, 5, 0], [12, 1, 5]])),
]

Format_CSS = [
    InReg(1, Range(2, 5)),
    Indirect(InReg(0, Const(2)), SignedImm(0, Ranges([[9, 4, 2], [7, 2, 6]]), isSigned=False)),
]

Format_DCSS = [
    InReg(1, Range(2, 5)),
    Indirect(InReg(0, Const(2)), SignedImm(0, Ranges([[10, 3, 3], [7, 3, 6]]), isSigned=False)),
]

Format_FCSS = [
    InFReg(1, Range(2, 5)),
    Indirect(InReg(0, Const(2)), SignedImm(0, Ranges([[9, 4, 2], [7, 2, 6]]), isSigned=False)),
]

Format_FCSSD = [
    InFReg(1, Range(2, 5)),
    Indirect(InReg(0, Const(2)), SignedImm(0, Ranges([[10, 3, 3], [7, 3, 6]]), isSigned=False)),
]

Format_CIW = [
    OutRegComp(0, Range(2, 3)),
    InReg(0, Const(2)),
    SignedImm(0, Ranges([[6, 1, 2], [5, 1, 3], [11, 2, 4], [7, 4, 6]]), isSigned=False),
]

Format_CL = [
    OutRegComp(0, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[6, 1, 2], [10, 3, 3], [5, 1, 6]]), isSigned=False)),
]

Format_CLD = [
    OutRegComp(0, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[10, 3, 3], [5, 2, 6]]), isSigned=False)),
]

Format_CFLD = [
    OutFRegComp(0, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[10, 3, 3], [5, 2, 6]]), isSigned=False)),
]

Format_FCL = [
    OutFRegComp(0, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[6, 1, 2], [10, 3, 3], [5, 1, 6]]), isSigned=False)),
]

Format_CS = [
    InRegComp(1, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[6, 1, 2], [10, 3, 3], [5, 1, 6]]), isSigned=False)),
]

Format_CFSD = [
    InRegComp(1, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[10, 3, 3], [5, 2, 6]]), isSigned=False)),
]

Format_CSD = [
    InRegComp(1, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[10, 3, 3], [5, 2, 6]]), isSigned=False)),
]

Format_FCS = [
    InFRegComp(1, Range(2, 3)),
    Indirect(InRegComp(0, Range(7, 3)), SignedImm(0, Ranges([[6, 1, 2], [10, 3, 3], [5, 1, 6]]), isSigned=False)),
]

Format_CS2 = [
    OutRegComp(0, Range(7, 3)),
    InRegComp(0, Range(7, 3)),
    InRegComp(1, Range(2, 3)),
]

Format_CB1 = [
    InRegComp(0, Range(7, 3)),
    InReg(1, Const(0)),
    SignedImm(0, Ranges([[3, 2, 1], [10, 2, 3], [2, 1, 5], [5, 2, 6], [12, 1, 8]])),
]

Format_CB2 = [
    OutRegComp(0, Range(7, 3)),
    InRegComp(0, Range(7, 3)),
    UnsignedImm(0, Ranges([[2, 5, 0], [12, 1, 5]])),
]

Format_CB2S = [
    OutRegComp(0, Range(7, 3)),
    InRegComp(0, Range(7, 3)),
    SignedImm(0, Ranges([[2, 5, 0], [12, 1, 5]])),
]

Format_CJ = [
    OutReg(0, Const(0)),
    SignedImm(0, Ranges([[3, 3, 1], [11, 1, 4], [2, 1, 5], [7, 1, 6], [6, 1, 7], [9, 2, 8], [8, 1, 10], [12, 1, 11]])),
]

Format_CJ1 = [
    OutReg(0, Const(1)),
    SignedImm(0, Ranges([[3, 3, 1], [11, 1, 4], [2, 1, 5], [7, 1, 6], [6, 1, 7], [9, 2, 8], [8, 1, 10], [12, 1, 11]])),
]

class Rv64i(IsaSubset):

    def __init__(self):
        super().__init__(name='rv64i', instrs=[
            Instr('lwu',   Format_L,    '------- ----- ----- 110 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('ld',    Format_L,    '------- ----- ----- 011 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('sd',    Format_S,    '------- ----- ----- 011 ----- 0100011', fast_handler=True, tags=["store"]),
            Instr('slli',  Format_I1U64,'000000- ----- ----- 001 ----- 0010011'),
            Instr('srli',  Format_I1U64,'000000- ----- ----- 101 ----- 0010011'),
            Instr('srai',  Format_I1U64,'010000- ----- ----- 101 ----- 0010011'),
            Instr('addiw', Format_I,    '------- ----- ----- 000 ----- 0011011'),
            Instr('slliw', Format_I1U,  '0000000 ----- ----- 001 ----- 0011011'),
            Instr('srliw', Format_I1U,  '0000000 ----- ----- 101 ----- 0011011'),
            Instr('sraiw', Format_I1U,  '0100000 ----- ----- 101 ----- 0011011'),
            Instr('addw',  Format_R,    '0000000 ----- ----- 000 ----- 0111011'),
            Instr('subw',  Format_R,    '0100000 ----- ----- 000 ----- 0111011'),
            Instr('sllw',  Format_R,    '0000000 ----- ----- 001 ----- 0111011'),
            Instr('srlw',  Format_R,    '0000000 ----- ----- 101 ----- 0111011'),
            Instr('sraw',  Format_R,    '0100000 ----- ----- 101 ----- 0111011'),
        ], includes=[
            '<cpu/iss/include/isa/rv64i.hpp>',
        ])



class Rv32i(IsaSubset):

    def __init__(self):
        super().__init__(name='rv32i', instrs=[
            Instr('lui',    Format_U,    '------- ----- ----- --- ----- 0110111', 'lui_decode'),
            Instr('auipc',  Format_U,    '------- ----- ----- --- ----- 0010111', 'auipc_decode'),
            Instr('jal',    Format_UJ,   '------- ----- ----- --- ----- 1101111', tags=["branch"], decode='jal_decode', fast_handler=True),
            Instr('jalr',   Format_I,    '------- ----- ----- 000 ----- 1100111', tags=["branch"], fast_handler=True),
            Instr('beq',    Format_SB,   '------- ----- ----- 000 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('bne',    Format_SB,   '------- ----- ----- 001 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('blt',    Format_SB,   '------- ----- ----- 100 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('bge',    Format_SB,   '------- ----- ----- 101 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('bltu',   Format_SB,   '------- ----- ----- 110 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('bgeu',   Format_SB,   '------- ----- ----- 111 ----- 1100011', tags=["branch"], decode='bxx_decode', fast_handler=True),
            Instr('lb',     Format_L,    '------- ----- ----- 000 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('lh',     Format_L,    '------- ----- ----- 001 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('lw',     Format_L,    '------- ----- ----- 010 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('lbu',    Format_L,    '------- ----- ----- 100 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('lhu',    Format_L,    '------- ----- ----- 101 ----- 0000011', fast_handler=True, tags=["load"]),
            Instr('sb',     Format_S,    '------- ----- ----- 000 ----- 0100011', fast_handler=True, tags=["store"]),
            Instr('sh',     Format_S,    '------- ----- ----- 001 ----- 0100011', fast_handler=True, tags=["store"]),
            Instr('sw',     Format_S,    '------- ----- ----- 010 ----- 0100011', fast_handler=True, tags=["store"]),
            Instr('addi',   Format_I,    '------- ----- ----- 000 ----- 0010011'),
            Instr('nop',    Format_Z,    '0000000 00000 00000 000 00000 0010011'),
            Instr('slti',   Format_I,    '------- ----- ----- 010 ----- 0010011'),
            Instr('sltiu',  Format_I,    '------- ----- ----- 011 ----- 0010011'),
            Instr('xori',   Format_I,    '------- ----- ----- 100 ----- 0010011'),
            Instr('ori',    Format_I,    '------- ----- ----- 110 ----- 0010011'),
            Instr('andi',   Format_I,    '------- ----- ----- 111 ----- 0010011'),
            Instr('slli',   Format_I1U64,'0000000 ----- ----- 001 ----- 0010011'),
            Instr('srli',   Format_I1U64,'0000000 ----- ----- 101 ----- 0010011'),
            Instr('srai',   Format_I1U64,'0100000 ----- ----- 101 ----- 0010011'),
            Instr('add',    Format_R,    '0000000 ----- ----- 000 ----- 0110011'),
            Instr('sub',    Format_R,    '0100000 ----- ----- 000 ----- 0110011'),
            Instr('sll',    Format_R,    '0000000 ----- ----- 001 ----- 0110011'),
            Instr('slt',    Format_R,    '0000000 ----- ----- 010 ----- 0110011'),
            Instr('sltu',   Format_R,    '0000000 ----- ----- 011 ----- 0110011'),
            Instr('xor',    Format_R,    '0000000 ----- ----- 100 ----- 0110011'),
            Instr('srl',    Format_R,    '0000000 ----- ----- 101 ----- 0110011'),
            Instr('sra',    Format_R,    '0100000 ----- ----- 101 ----- 0110011'),
            Instr('or',     Format_R,    '0000000 ----- ----- 110 ----- 0110011'),
            Instr('and',    Format_R,    '0000000 ----- ----- 111 ----- 0110011'),
            Instr('fence',  Format_I3U,  '0000--- ----- 00000 000 00000 0001111'),
            Instr('fence.i',Format_I,    '0000000 00000 00000 001 00000 0001111'),
            Instr('ecall',  Format_I,    '0000000 00000 00000 000 00000 1110011'),
            Instr('ebreak', Format_I,    '0000000 00001 00000 000 00000 1110011')
        ], includes=[
            '<cpu/iss/include/isa/rv32i.hpp>',
        ])



class Rv32m(IsaSubset):

    def __init__(self):
        super().__init__(name='rv32m', instrs=[
            Instr('mul',   Format_R, '0000001 ----- ----- 000 ----- 0110011', tags=['mul']),
            Instr('mulh',  Format_R, '0000001 ----- ----- 001 ----- 0110011', tags=['mulh']),
            Instr('mulhsu',Format_R, '0000001 ----- ----- 010 ----- 0110011', tags=['mulh']),
            Instr('mulhu', Format_R, '0000001 ----- ----- 011 ----- 0110011', tags=['mulh']),
            Instr('div',   Format_R, '0000001 ----- ----- 100 ----- 0110011', tags=['div']),
            Instr('divu',  Format_R, '0000001 ----- ----- 101 ----- 0110011', tags=['div']),
            Instr('rem',   Format_R, '0000001 ----- ----- 110 ----- 0110011', tags=['div']),
            Instr('remu',  Format_R, '0000001 ----- ----- 111 ----- 0110011', tags=['div']),

        ], includes=[
            '<cpu/iss/include/isa/rv32m.hpp>',
        ])



class Rv64m(IsaSubset):

    def __init__(self):
        super().__init__(name='rv64m', instrs=[
            Instr('mulw',  Format_R, '0000001 ----- ----- 000 ----- 0111011', tags=['mul']),
            Instr('divw',  Format_R, '0000001 ----- ----- 100 ----- 0111011', tags=['div']),
            Instr('divuw', Format_R, '0000001 ----- ----- 101 ----- 0111011', tags=['div']),
            Instr('remw',  Format_R, '0000001 ----- ----- 110 ----- 0111011', tags=['div']),
            Instr('remuw', Format_R, '0000001 ----- ----- 111 ----- 0111011', tags=['div']),
        ], includes=[
            '<cpu/iss/include/isa/rv64m.hpp>',
        ])



class Rv32a(IsaSubset):

    def __init__(self):
        super().__init__(name='rv32a', instrs=[
            Instr('lr.w',       Format_LRES,  '00010 -- 00000 ----- 010 ----- 0101111'),
            Instr('sc.w',       Format_AMO,   '00011 -- ----- ----- 010 ----- 0101111'),
            Instr('amoswap.w',  Format_AMO,   '00001 -- ----- ----- 010 ----- 0101111'),
            Instr('amoadd.w' ,  Format_AMO,   '00000 -- ----- ----- 010 ----- 0101111'),
            Instr('amoxor.w' ,  Format_AMO,   '00100 -- ----- ----- 010 ----- 0101111'),
            Instr('amoand.w' ,  Format_AMO,   '01100 -- ----- ----- 010 ----- 0101111'),
            Instr('amoor.w'  ,  Format_AMO,   '01000 -- ----- ----- 010 ----- 0101111'),
            Instr('amomin.w' ,  Format_AMO,   '10000 -- ----- ----- 010 ----- 0101111'),
            Instr('amomax.w' ,  Format_AMO,   '10100 -- ----- ----- 010 ----- 0101111'),
            Instr('amominu.w',  Format_AMO,   '11000 -- ----- ----- 010 ----- 0101111'),
            Instr('amomaxu.w',  Format_AMO,   '11100 -- ----- ----- 010 ----- 0101111')
        ], includes=[
            '<cpu/iss/include/isa/rv32a.hpp>',
        ])



class Rv64a(IsaSubset):

    def __init__(self):
        super().__init__(name='rv64a', instrs=[
            Instr('lr.d',       Format_LRES,  '00010 -- 00000 ----- 011 ----- 0101111'),
            Instr('sc.d',       Format_AMO,   '00011 -- ----- ----- 011 ----- 0101111'),
            Instr('amoswap.d',  Format_AMO,   '00001 -- ----- ----- 011 ----- 0101111'),
            Instr('amoadd.d' ,  Format_AMO,   '00000 -- ----- ----- 011 ----- 0101111'),
            Instr('amoxor.d' ,  Format_AMO,   '00100 -- ----- ----- 011 ----- 0101111'),
            Instr('amoand.d' ,  Format_AMO,   '01100 -- ----- ----- 011 ----- 0101111'),
            Instr('amoor.d'  ,  Format_AMO,   '01000 -- ----- ----- 011 ----- 0101111'),
            Instr('amomin.d' ,  Format_AMO,   '10000 -- ----- ----- 011 ----- 0101111'),
            Instr('amomax.d' ,  Format_AMO,   '10100 -- ----- ----- 011 ----- 0101111'),
            Instr('amominu.d',  Format_AMO,   '11000 -- ----- ----- 011 ----- 0101111'),
            Instr('amomaxu.d',  Format_AMO,   '11100 -- ----- ----- 011 ----- 0101111')
        ], includes=[
            '<cpu/iss/include/isa/rv64a.hpp>',
        ])



class Rv32f(IsaSubset):

    def __init__(self):
        super().__init__(name='rvf', instrs=[
            Insn('------- ----- ----- 010 ----- 0000111', 'flw',       FL('s'),            fast_handler=True, tags=["fload", 'fp_op']),
            Insn('------- ----- ----- 010 ----- 0100111', 'fsw',       FS('s'),            fast_handler=True, tags=['fp_op']),
            Insn('-----00 ----- ----- --- ----- 1000011', 'fmadd.s',   F_FFF('s', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----00 ----- ----- --- ----- 1000111', 'fmsub.s',   F_FFF('s', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----00 ----- ----- --- ----- 1001011', 'fnmsub.s',  F_FFF('s', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----00 ----- ----- --- ----- 1001111', 'fnmadd.s',  F_FFF('s', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('0000000 ----- ----- --- ----- 1010011', 'fadd.s',    F_FF('s', ui12_3),  tags=['fadd', 'fp_op']),
            Insn('0000100 ----- ----- --- ----- 1010011', 'fsub.s',    F_FF('s', ui12_3),  tags=['fadd', 'fp_op']),
            Insn('0001000 ----- ----- --- ----- 1010011', 'fmul.s',    F_FF('s', ui12_3),  tags=['fmul', 'fp_op']),
            Insn('0001100 ----- ----- --- ----- 1010011', 'fdiv.s',    F_FF('s', ui12_3),  tags=['fdiv', 'fp_op']),
            Insn('0101100 00000 ----- --- ----- 1010011', 'fsqrt.s',   F_F('s', ui12_3),   tags=['fdiv', 'fp_op']),
            Insn('0010000 ----- ----- 000 ----- 1010011', 'fsgnj.s',   F_FF('s'),          tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010000 ----- ----- 001 ----- 1010011', 'fsgnjn.s',  F_FF('s'),          tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010000 ----- ----- 010 ----- 1010011', 'fsgnjx.s',  F_FF('s'),          tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010100 ----- ----- 000 ----- 1010011', 'fmin.s',    F_FF('s'),          tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010100 ----- ----- 001 ----- 1010011', 'fmax.s',    F_FF('s'),          tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('1010000 ----- ----- 010 ----- 1010011', 'feq.s',     R_FF('s'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1010000 ----- ----- 001 ----- 1010011', 'flt.s',     R_FF('s'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1010000 ----- ----- 000 ----- 1010011', 'fle.s',     R_FF('s'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1100000 00000 ----- --- ----- 1010011', 'fcvt.w.s',  R_F('s', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1100000 00001 ----- --- ----- 1010011', 'fcvt.wu.s', R_F('s', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1101000 00000 ----- --- ----- 1010011', 'fcvt.s.w',  F_R('s', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1101000 00001 ----- --- ----- 1010011', 'fcvt.s.wu', F_R('s', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1110000 00000 ----- 000 ----- 1010011', 'fmv.x.s',   R_F('s'),           tags=['fmv', 'nseq', 'fp_op']),
            Insn('1110000 00000 ----- 001 ----- 1010011', 'fclass.s',  R_F('s'),           tags=['fother', 'nseq', 'fp_op']),
            Insn('1111000 00000 ----- 000 ----- 1010011', 'fmv.s.x',   F_R('s'),           tags=['fmv', 'nseq', 'fp_op']),

            # If RV64F supported
            Insn('1100000 00010 ----- --- ----- 1010011', 'fcvt.l.s',  R_F('s', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1100000 00011 ----- --- ----- 1010011', 'fcvt.lu.s', R_F('s', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1101000 00010 ----- --- ----- 1010011', 'fcvt.s.l',  F_R('s', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1101000 00011 ----- --- ----- 1010011', 'fcvt.s.lu', F_R('s', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
        ], includes=[
            '<cpu/iss/include/isa/rvf.hpp>',
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rv64i'):
            isa.disable_from_isa_tag('rv64f')



class Rv32d(IsaSubset):

    def __init__(self):
        super().__init__(name='rvd', instrs=[
            Insn('------- ----- ----- 011 ----- 0000111', 'fld',       FL('d'),            fast_handler=True, tags=['load', "fload", 'fp_op']),
            Insn('------- ----- ----- 011 ----- 0100111', 'fsd',       FS('d'),            fast_handler=True, tags=['fp_op']),
            Insn('-----01 ----- ----- --- ----- 1000011', 'fmadd.d',   F_FFF('d', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----01 ----- ----- --- ----- 1000111', 'fmsub.d',   F_FFF('d', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----01 ----- ----- --- ----- 1001011', 'fnmsub.d',  F_FFF('d', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('-----01 ----- ----- --- ----- 1001111', 'fnmadd.d',  F_FFF('d', ui12_3), tags=['fmadd', 'fp_op']),
            Insn('0000001 ----- ----- --- ----- 1010011', 'fadd.d',    F_FF('d', ui12_3),  tags=['fadd', 'fp_op']),
            Insn('0000101 ----- ----- --- ----- 1010011', 'fsub.d',    F_FF('d', ui12_3),  tags=['fadd', 'fp_op']),
            Insn('0001001 ----- ----- --- ----- 1010011', 'fmul.d',    F_FF('d', ui12_3),  tags=['fmul', 'fp_op']),
            Insn('0001101 ----- ----- --- ----- 1010011', 'fdiv.d',    F_FF('d', ui12_3),  tags=['fdiv', 'fp_op']),
            Insn('0101101 00000 ----- --- ----- 1010011', 'fsqrt.d',   F_F('d', ui12_3),   tags=['fdiv', 'fp_op']),
            Insn('0010001 ----- ----- 000 ----- 1010011', 'fsgnj.d',   F_FF('d', ui12_3),  tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010001 ----- ----- 001 ----- 1010011', 'fsgnjn.d',  F_FF('d', ui12_3),  tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010001 ----- ----- 010 ----- 1010011', 'fsgnjx.d',  F_FF('d', ui12_3),  tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010101 ----- ----- 000 ----- 1010011', 'fmin.d',    F_FF('d', ui12_3),  tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0010101 ----- ----- 001 ----- 1010011', 'fmax.d',    F_FF('d', ui12_3),  tags=['fconv', 'fnoncomp', 'fp_op']),
            Insn('0100000 00001 ----- --- ----- 1010011', 'fcvt.s.d',  F_F('s_d', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('0100001 00000 ----- --- ----- 1010011', 'fcvt.d.s',  F_F('d_s', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1010001 ----- ----- 010 ----- 1010011', 'feq.d',     R_FF('d'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1010001 ----- ----- 001 ----- 1010011', 'flt.d',     R_FF('d'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1010001 ----- ----- 000 ----- 1010011', 'fle.d',     R_FF('d'),          tags=['fother', 'nseq', 'fp_op']),
            Insn('1100001 00000 ----- --- ----- 1010011', 'fcvt.w.d',  R_F('d', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1100001 00001 ----- --- ----- 1010011', 'fcvt.wu.d', R_F('d', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1101001 00000 ----- --- ----- 1010011', 'fcvt.d.w',  F_R('d', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1101001 00001 ----- --- ----- 1010011', 'fcvt.d.wu', F_R('d', ui12_3),   tags=['fconv', 'nseq', 'fp_op']),
            Insn('1110001 00000 ----- 001 ----- 1010011', 'fclass.d',  R_F('d'),           tags=['fother', 'nseq', 'fp_op']),

            # # If RV64F supported
            Insn('1100001 00010 ----- --- ----- 1010011', 'fcvt.l.d',  R_F('d', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1100001 00011 ----- --- ----- 1010011', 'fcvt.lu.d', R_F('d', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1110001 00000 ----- 000 ----- 1010011', 'fmv.x.d',   R_F('d'),         tags=['fmv', 'nseq', 'fp_op']),
            Insn('1101001 00010 ----- --- ----- 1010011', 'fcvt.d.l',  F_R('d', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1101001 00011 ----- --- ----- 1010011', 'fcvt.d.lu', F_R('d', ui12_3), tags=['fconv', 'nseq', 'fp_op'], isa_tags=['rv64f']),
            Insn('1111001 00000 ----- 000 ----- 1010011', 'fmv.d.x',   F_R('d'),         tags=['fmv', 'nseq', 'fp_op']),
        ], includes=[
            '<cpu/iss/include/isa/rvd.hpp>',
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rv64i'):
            isa.disable_from_isa_tag('rv64f')


class Priv(IsaSubset):

    def __init__(self):
        super().__init__(name='priv', instrs=[
            Instr('csrrw', Format_IU,  '------- ----- ----- 001 ----- 1110011', decode='csr_decode', tags=['nseq']),
            Instr('csrrs', Format_IU,  '------- ----- ----- 010 ----- 1110011', decode='csr_decode', tags=['nseq']),
            Instr('csrrc', Format_IU,  '------- ----- ----- 011 ----- 1110011', decode='csr_decode', tags=['nseq']),
            Instr('csrrwi',Format_IUR, '------- ----- ----- 101 ----- 1110011', decode='csr_decode', tags=['nseq']),
            Instr('csrrsi',Format_IUR, '------- ----- ----- 110 ----- 1110011', decode='csr_decode', tags=['nseq']),
            Instr('csrrci',Format_IUR, '------- ----- ----- 111 ----- 1110011', decode='csr_decode', tags=['nseq']),

        ], includes=[
            '<cpu/iss/include/isa/priv.hpp>',
        ])



class TrapReturn(IsaSubset):

    def __init__(self):
        super().__init__(name='trap_return', instrs=[
            #Instr('uret',      Format_Z,   '0000000 00010 00000 000 00000 1110011', tags=["branch"]),
            Instr('sret',      Format_Z,   '0001000 00010 00000 000 00000 1110011', tags=["branch"]),
            #Instr('hret',      Format_Z,   '0010000 00010 00000 000 00000 1110011', tags=["branch"]),
            Instr('mret',      Format_Z,   '0011000 00010 00000 000 00000 1110011', tags=["branch"]),
            Instr('dret',      Format_Z,   '0111101 10010 00000 000 00000 1110011', tags=["branch"]),
            Instr('wfi',       Format_Z,   '0001000 00101 00000 000 00000 1110011', tags=["branch"]),

        ])



class PrivSmmu(IsaSubset):

    def __init__(self):
        super().__init__(name='priv_smmu', instrs=[
            Instr('sfence.vma',       Format_INRR,   '0001001 ----- ----- 000 00000 1110011'),

        ])



class Zcmp(IsaSubset):

    def __init__(self):
        super().__init__(name='zcmp', instrs=[
            # Compressed ISA
            Instr('cm.push'    , Format_CMPUSH,'101 110 00- -- --- 10', is_macro_op=True),
            Instr('cm.pop'     , Format_CMPUSH,'101 110 10- -- --- 10', is_macro_op=True),
            Instr('cm.popretz' , Format_CMPUSH,'101 111 00- -- --- 10', is_macro_op=True),
            Instr('cm.popret'  , Format_CMPUSH,'101 111 10- -- --- 10', is_macro_op=True),
        ])



class Rv32c(IsaSubset):

    def __init__(self):
        super().__init__(name='rv32c', instrs=[
            Instr('c.unimp',    Format_CI1, '000 000 000 00 000 00'),
            Instr('c.addi4spn', Format_CIW, '000 --- --- -- --- 00', fast_handler=True),
            Instr('c.lw',       Format_CL,  '010 --- --- -- --- 00', fast_handler=True, tags=["load"]),
            Instr('c.sw',       Format_CS,  '110 --- --- -- --- 00', fast_handler=True),
            Instr('c.nop',      Format_CI1, '000 000 000 00 000 01', fast_handler=True),
            Instr('c.addi',     Format_CI1, '000 --- --- -- --- 01', fast_handler=True),
            Instr('c.jal',      Format_CJ1, '001 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='jal_decode'),
            Instr('c.li',       Format_CI6, '010 --- --- -- --- 01', fast_handler=True),
            Instr('c.addi16sp', Format_CI4, '011 -00 010 -- --- 01', fast_handler=True),
            Instr('c.lui',      Format_CI5, '011 --- --- -- --- 01', fast_handler=True),
            Instr('c.srli',     Format_CB2, '100 -00 --- -- --- 01', fast_handler=True),
            Instr('c.srai',     Format_CB2, '100 -01 --- -- --- 01', fast_handler=True),
            Instr('c.andi',     Format_CB2S,'100 -10 --- -- --- 01', fast_handler=True),
            Instr('c.sub',      Format_CS2, '100 011 --- 00 --- 01', fast_handler=True),
            Instr('c.xor',      Format_CS2, '100 011 --- 01 --- 01', fast_handler=True),
            Instr('c.or',       Format_CS2, '100 011 --- 10 --- 01', fast_handler=True),
            Instr('c.and',      Format_CS2, '100 011 --- 11 --- 01', fast_handler=True),
            Instr('c.j',        Format_CJ,  '101 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='jal_decode'),
            Instr('c.beqz',     Format_CB1, '110 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='bxx_decode'),
            Instr('c.bnez',     Format_CB1, '111 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='bxx_decode'),
            Instr('c.slli',     Format_CI1U,'000 --- --- -- --- 10', fast_handler=True),
            Instr('c.lwsp',     Format_CI3, '010 --- --- -- --- 10', fast_handler=True, tags=["load"]),
            Instr('c.jr',       Format_CR1, '100 0-- --- 00 000 10', tags=["branch"], fast_handler=True),
            Instr('c.mv',       Format_CR2, '100 0-- --- -- --- 10', fast_handler=True),
            Instr('c.ebreak',   Format_CR,  '100 100 000 00 000 10'),
            Instr('c.jalr',     Format_CR3, '100 1-- --- 00 000 10', tags=["branch"], fast_handler=True),
            Instr('c.add',      Format_CR,  '100 1-- --- -- --- 10', fast_handler=True),
            Instr('c.swsp',     Format_CSS, '110 --- --- -- --- 10', fast_handler=True),
            Instr('c.sbreak',   Format_CI1, '100 000 000 00 000 10'),
            Instr('c.flwsp',    Format_FCI3, '011 --- --- -- --- 10', fast_handler=True, tags=['load', "fload"], isa_tags=['cf']),
            Instr('c.fswsp',    Format_FCSS, '111 --- --- -- --- 10', fast_handler=True, isa_tags=['cf']),
            Instr('c.fsw',      Format_FCS,  '111 --- --- -- --- 00', fast_handler=True, isa_tags=['cf']),
            Instr('c.flw',      Format_FCL,  '011 --- --- -- --- 00', fast_handler=True, tags=['load', "fload"], isa_tags=['cf']),
        ], includes=[
            '<cpu/iss/include/isa/rv32c.hpp>',
        ])


        if os.environ.get('CONFIG_GVSOC_USE_UNCOMPRESSED_LABELS') is not None:
            unconmpressed_names = [
                'addi', 'lw', 'sw', 'nop', 'addi', 'jal', 'addi', 'addi', 'lui', 'srli', 'srai', 'andi', 'sub', 'xor', 'or', 'and', 'j', 'beqz', 'bnez', 'slli',
                'lw', 'jr', 'mv', 'ebreak', 'jalr', 'add', 'sw', 'sbreak',
            ]

            for insn in self.instrs:
                label = unconmpressed_names.pop(0)
                insn.traceLabel = label

    def check_compatibilities(self, isa):
        if not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('cf')


class Rv64c(IsaSubset):

    def __init__(self):
        super().__init__(name='rv64c', instrs=[
            Instr('c.unimp',    Format_CI1, '000 000 000 00 000 00'),
            Instr('c.addi4spn', Format_CIW, '000 --- --- -- --- 00', fast_handler=True),
            Instr('c.ld',       Format_CLD, '011 --- --- -- --- 00', fast_handler=True, tags=["load"]),
            Instr('c.lw',       Format_CL,  '010 --- --- -- --- 00', fast_handler=True, tags=["load"]),
            Instr('c.fld',      Format_CFLD, '001 --- --- -- --- 00', fast_handler=True, tags=['load', "fload"]),
            Instr('c.sw',       Format_CS,  '110 --- --- -- --- 00', fast_handler=True),
            Instr('c.sd',       Format_CSD, '111 --- --- -- --- 00', fast_handler=True),
            Instr('c.fsd',      Format_CFSD, '101 --- --- -- --- 00', fast_handler=True),
            Instr('c.nop',      Format_CI1, '000 000 000 00 000 01', fast_handler=True),
            Instr('c.addi',     Format_CI1, '000 --- --- -- --- 01', fast_handler=True),
            Instr('c.addiw',    Format_CI1, '001 --- --- -- --- 01', fast_handler=True),
            Instr('c.li',       Format_CI6, '010 --- --- -- --- 01', fast_handler=True),
            Instr('c.addi16sp', Format_CI4, '011 -00 010 -- --- 01', fast_handler=True),
            Instr('c.lui',      Format_CI5, '011 --- --- -- --- 01', fast_handler=True),
            Instr('c.srli',     Format_CB2, '100 -00 --- -- --- 01', fast_handler=True),
            Instr('c.srai',     Format_CB2, '100 -01 --- -- --- 01', fast_handler=True),
            Instr('c.andi',     Format_CB2S,'100 -10 --- -- --- 01', fast_handler=True),
            Instr('c.sub',      Format_CS2, '100 011 --- 00 --- 01', fast_handler=True),
            Instr('c.xor',      Format_CS2, '100 011 --- 01 --- 01', fast_handler=True),
            Instr('c.or',       Format_CS2, '100 011 --- 10 --- 01', fast_handler=True),
            Instr('c.and',      Format_CS2, '100 011 --- 11 --- 01', fast_handler=True),
            Instr('c.subw',     Format_CS2, '100 111 --- 00 --- 01', fast_handler=True),
            Instr('c.addw',     Format_CS2, '100 111 --- 01 --- 01', fast_handler=True),
            Instr('c.j',        Format_CJ,  '101 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='jal_decode'),
            Instr('c.beqz',     Format_CB1, '110 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='bxx_decode'),
            Instr('c.bnez',     Format_CB1, '111 --- --- -- --- 01', tags=["branch"], fast_handler=True, decode='bxx_decode'),
            Instr('c.slli',     Format_CI1U,'000 --- --- -- --- 10', fast_handler=True),
            Instr('c.lwsp',     Format_CI3, '010 --- --- -- --- 10', fast_handler=True, tags=["load"]),
            Instr('c.ldsp',     Format_DCI3, '011 --- --- -- --- 10', fast_handler=True, tags=["load"]),
            Instr('c.jr',       Format_CR1, '100 0-- --- 00 000 10', tags=["branch"], fast_handler=True),
            Instr('c.mv',       Format_CR2, '100 0-- --- -- --- 10', fast_handler=True),
            Instr('c.ebreak',   Format_CR,  '100 100 000 00 000 10'),
            Instr('c.jalr',     Format_CR3, '100 1-- --- 00 000 10', tags=["branch"], fast_handler=True),
            Instr('c.add',      Format_CR,  '100 1-- --- -- --- 10', fast_handler=True),
            Instr('c.swsp',     Format_CSS, '110 --- --- -- --- 10', fast_handler=True),
            Instr('c.sdsp',     Format_DCSS, '111 --- --- -- --- 10', fast_handler=True),
            Instr('c.sbreak',   Format_CI1, '100 000 000 00 000 10'),
            Instr('c.fsdsp',    Format_FCSSD, '101 --- --- -- --- 10', isa_tags=['cf']),
            Instr('c.fldsp',    Format_FCI3D, '001 --- --- -- --- 10', tags=['load', "fload"], isa_tags=['cf']),
        ], includes=[
            '<cpu/iss_v2/include/isa/rv64c.hpp>',
        ])

    def check_compatibilities(self, isa):
        if not isa.has_isa('rvf'):
            isa.disable_from_isa_tag('cf')



class RiscvIsa(Isa):

    def __init__(self, name, isa, inc_priv=True, inc_supervisor=True, inc_user=False, no_hash=False, extensions=None):
        super().__init__(name, isa)

        misa = 0

        if extensions is not None:
            for extension in extensions:
                self.add_isa(extension)

        self.generated = False

        if isa[0:4] == 'rv32':
            self.word_size = 32
        elif isa[0:4] == 'rv64':
            self.word_size = 64
        else:
            raise RuntimeError('Isa should start with either rv32 or rv64')

        for isa in isa[4:]:

            if isa == 'i':
                misa |= 1 << 8
                self.add_isa(Rv32i())
                if self.word_size > 32:
                    self.add_isa(Rv64i())

            elif isa == 'm':
                misa |= 1 << 12
                self.add_isa(Rv32m())
                if self.word_size > 32:
                    self.add_isa(Rv64m())

            elif isa == 'a':
                misa |= 1 << 0
                self.add_isa(Rv32a())
                if self.word_size > 32:
                    self.add_isa(Rv64a())

            elif isa == 'c':
                misa |= 1 << 2
                if self.word_size == 32:
                    self.add_isa(Rv32c())
                else:
                    self.add_isa(Rv64c())

            elif isa == 'f':
                misa |= 1 << 5
                self.add_isa(Rv32f())

            elif isa == 'd':
                misa |= 1 << 3
                self.add_isa(Rv32d())

            elif isa == 'v':
                misa |= 1 << 3
                self.add_isa(Rv32v())

        if inc_priv:
            self.add_isa(Priv())
            self.add_isa(TrapReturn())

        if inc_supervisor:
            self.add_isa(PrivSmmu())

        # Now we need to disable specific instructions depending on the combination of
        # isa subsets
        self.check_compatibilities()

        if inc_supervisor:
            misa |= 1 << 18

        if inc_user:
            misa |= 1 << 20

        self.misa = misa

        if no_hash:
            self.full_name = f'isa_{self.name}'
        else:
            name = ''.join(self.isas.keys())
            name_hash = int(hashlib.md5(name.encode('utf-8')).hexdigest()[0:7], 16)

            self.full_name = f'isa_{self.name}_{name_hash}'

    def get_source(self):
        return f'{self.full_name}.cpp'

    def get_header(self):
        return f'{self.full_name}.hpp'

    def gen(self, component, builddir, installdir):

        if not self.generated:

            self.generated = True

            full_name = os.path.join(builddir, self.full_name)

            os.makedirs(os.path.dirname(full_name), mode=0o750, exist_ok=True)

            with open(f'{full_name}.cpp.new', 'w') as isaFile:
                with open(f'{full_name}.hpp.new', 'w') as isaFileHeader:
                    Isa.gen(self, isaFile, isaFileHeader)

            if not os.path.exists(f'{full_name}.cpp') or \
                    not filecmp.cmp(f'{full_name}.cpp.new', f'{full_name}.cpp', shallow=False):
                shutil.move(f'{full_name}.cpp.new', f'{full_name}.cpp')

            if not os.path.exists(f'{full_name}.hpp') or \
                    not filecmp.cmp(f'{full_name}.hpp.new', f'{full_name}.hpp', shallow=False):
                shutil.move(f'{full_name}.hpp.new', f'{full_name}.hpp')
