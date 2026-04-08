#
# Copyright (C) 2026 OpenHW Group
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
from cpu.iss.isa_gen.isa_corev import Format_SB2, Format_LPOST, Format_LRPOST, Format_LR, Format_SPOST, Format_SRPOST, Format_SR, Format_HL0, Format_HL1, Format_RRRR, Format_RRRR2, Format_RRRU2, Format_RRRRU, Format_R1, Format_I1U, Format_I4U, Format_I5U, Format_I5U2, Format_BITREV

# CORE-V v2 uses standard RISC-V custom opcodes:
# Custom-0: 0x0B (0001011) - Branching & Load Immediate Post-Inc
# Custom-1: 0x2B (0101011) - Store Immediate Post-Inc & Register Load/Store
# Custom-2: 0x5B (1011011) - Multiplication / MAC / HWLoop

class CoreV2(IsaSubset):

    def __init__(self):
        instrs = []

        # --- Custom-0 (0x0B): Branching and Load Post-Increment ---
        # Note: names must match C++ handler names (case sensitive)
        instrs += [
            Instr('p.beqimm', Format_SB2, '------- ----- ----- 110 ----- 0001011', fast_handler=True, decode='bxx_decode', L='cv.beqimm'),
            Instr('p.bneimm', Format_SB2, '------- ----- ----- 111 ----- 0001011', fast_handler=True, decode='bxx_decode', L='cv.bneimm'),
            
            Instr('LB_POSTINC',    Format_LPOST, '------- ----- ----- 000 ----- 0001011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_POSTINC',   Format_LPOST, '------- ----- ----- 100 ----- 0001011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_POSTINC',    Format_LPOST, '------- ----- ----- 001 ----- 0001011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_POSTINC',   Format_LPOST, '------- ----- ----- 101 ----- 0001011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_POSTINC',    Format_LPOST, '------- ----- ----- 010 ----- 0001011', L='cv.lw' , fast_handler=True, tags=["load"]),
            
            Instr('p.elw',         Format_L,     '------- ----- ----- 011 ----- 0001011', L='cv.elw', tags=["load"]),
        ]

        # --- Custom-1 (0x2B): Store Post-Increment and Register Indexed Load/Store ---
        instrs += [
            Instr('SB_POSTINC',    Format_SPOST, '------- ----- ----- 000 ----- 0101011', L='cv.sb' , fast_handler=True),
            Instr('SH_POSTINC',    Format_SPOST, '------- ----- ----- 001 ----- 0101011', L='cv.sh' , fast_handler=True),
            Instr('SW_POSTINC',    Format_SPOST, '------- ----- ----- 010 ----- 0101011', L='cv.sw' , fast_handler=True),

            Instr('LB_RR_POSTINC',   Format_LRPOST,  '0000000 ----- ----- 011 ----- 0101011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR_POSTINC',  Format_LRPOST,  '0001000 ----- ----- 011 ----- 0101011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_RR_POSTINC',   Format_LRPOST,  '0000001 ----- ----- 011 ----- 0101011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_RR_POSTINC',  Format_LRPOST,  '0001001 ----- ----- 011 ----- 0101011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_RR_POSTINC',   Format_LRPOST,  '0000010 ----- ----- 011 ----- 0101011', L='cv.lw' , fast_handler=True, tags=["load"]),

            Instr('LB_RR',    Format_LR, '0000100 ----- ----- 011 ----- 0101011', L='cv.lb' , fast_handler=True, tags=["load"]),
            Instr('LBU_RR',   Format_LR, '0001100 ----- ----- 011 ----- 0101011', L='cv.lbu', fast_handler=True, tags=["load"]),
            Instr('LH_RR',    Format_LR, '0000101 ----- ----- 011 ----- 0101011', L='cv.lh' , fast_handler=True, tags=["load"]),
            Instr('LHU_RR',   Format_LR, '0001101 ----- ----- 011 ----- 0101011', L='cv.lhu', fast_handler=True, tags=["load"]),
            Instr('LW_RR',    Format_LR, '0000110 ----- ----- 011 ----- 0101011', L='cv.lw' , fast_handler=True, tags=["load"]),

            Instr('SB_RR_POSTINC',   Format_SRPOST, '0010000 ----- ----- 011 ----- 0101011',  L='cv.sb' , fast_handler=True),
            Instr('SH_RR_POSTINC',   Format_SRPOST, '0010001 ----- ----- 011 ----- 0101011',  L='cv.sh' , fast_handler=True),
            Instr('SW_RR_POSTINC',   Format_SRPOST, '0010010 ----- ----- 011 ----- 0101011',  L='cv.sw' , fast_handler=True),

            Instr('SB_RR',    Format_SR, '0010100 ----- ----- 011 ----- 0101011', L='cv.sb', fast_handler=True),
            Instr('SH_RR',    Format_SR, '0010101 ----- ----- 011 ----- 0101011', L='cv.sh', fast_handler=True),
            Instr('SW_RR',    Format_SR, '0010110 ----- ----- 011 ----- 0101011', L='cv.sw', fast_handler=True),
        ]

        # --- Custom-1 Plane A (0x2B, funct3=011): General ALU Operations ---
        instrs += [
            Instr('p.ror',    Format_R,  '0100000 ----- ----- 011 ----- 0101011', L='cv.ror'),
            Instr('p.ff1',    Format_R1, '0100001 00000 ----- 011 ----- 0101011', L='cv.ff1'),
            Instr('p.fl1',    Format_R1, '0100010 00000 ----- 011 ----- 0101011', L='cv.fl1'),
            Instr('p.clb',    Format_R1, '0100011 00000 ----- 011 ----- 0101011', L='cv.clb'),
            Instr('p.cnt',    Format_R1, '0100100 00000 ----- 011 ----- 0101011', L='cv.cnt'),
            Instr('p.abs',    Format_R1, '0101000 00000 ----- 011 ----- 0101011', L='cv.abs'),
            Instr('p.slet',   Format_R,  '0101001 ----- ----- 011 ----- 0101011', L='cv.slet'),
            Instr('p.sletu',  Format_R,  '0101010 ----- ----- 011 ----- 0101011', L='cv.sletu'),
            Instr('p.min',    Format_R,  '0101011 ----- ----- 011 ----- 0101011', L='cv.min'),
            Instr('p.minu',   Format_R,  '0101100 ----- ----- 011 ----- 0101011', L='cv.minu'),
            Instr('p.max',    Format_R,  '0101101 ----- ----- 011 ----- 0101011', L='cv.max'),
            Instr('p.maxu',   Format_R,  '0101110 ----- ----- 011 ----- 0101011', L='cv.maxu'),
            Instr('p.exths',  Format_R1, '0110000 00000 ----- 011 ----- 0101011', L='cv.exths'),
            Instr('p.exthz',  Format_R1, '0110001 00000 ----- 011 ----- 0101011', L='cv.exthz'),
            Instr('p.extbs',  Format_R1, '0110010 00000 ----- 011 ----- 0101011', L='cv.extbs'),
            Instr('p.extbz',  Format_R1, '0110011 00000 ----- 011 ----- 0101011', L='cv.extbz'),
            Instr('p.clipi',  Format_I1U, '0111000 ----- ----- 011 ----- 0101011', L='cv.clip'),
            Instr('p.clipui', Format_I1U, '0111001 ----- ----- 011 ----- 0101011', L='cv.clipu'),
            Instr('cv.clipr',  Format_R,  '0111010 ----- ----- 011 ----- 0101011'),
            Instr('cv.clipur', Format_R,  '0111011 ----- ----- 011 ----- 0101011'),
        ]

        # --- Custom-1 Plane A (0x2B, funct3=011): Register-Register Add/Sub with Normalization ---
        # funct7[31:29]=100, funct7[27]=sub, funct7[26]=round, funct7[25]=unsigned
        # Shift amount from register (Format_RRRR2), handlers p_addN_exec etc.
        instrs += [
            Instr('p.addN',    Format_RRRR2, '1000000 ----- ----- 011 ----- 0101011', L='cv.addN'),
            Instr('p.adduN',   Format_RRRR2, '1000001 ----- ----- 011 ----- 0101011', L='cv.adduN'),
            Instr('p.addRN',   Format_RRRR2, '1000010 ----- ----- 011 ----- 0101011', L='cv.addRN'),
            Instr('p.adduRN',  Format_RRRR2, '1000011 ----- ----- 011 ----- 0101011', L='cv.adduRN'),
            Instr('p.subN',    Format_RRRR2, '1000100 ----- ----- 011 ----- 0101011', L='cv.subN'),
            Instr('p.subuN',   Format_RRRR2, '1000101 ----- ----- 011 ----- 0101011', L='cv.subuN'),
            Instr('p.subRN',   Format_RRRR2, '1000110 ----- ----- 011 ----- 0101011', L='cv.subRN'),
            Instr('p.subuRN',  Format_RRRR2, '1000111 ----- ----- 011 ----- 0101011', L='cv.subuRN'),
        ]

        # --- Custom-1 Plane A (0x2B, funct3=011): Register Bit-Manipulation ---
        # funct7[31:27]=00110 (7'b0011xxx), instr[27:25] selects variant
        instrs += [
            Instr('p.extract',  Format_R,    '0011000 ----- ----- 011 ----- 0101011', L='cv.extractr'),
            Instr('p.extractu', Format_R,    '0011001 ----- ----- 011 ----- 0101011', L='cv.extractur'),
            Instr('p.insert',   Format_I5U2, '0011010 ----- ----- 011 ----- 0101011', L='cv.insertr'),
            Instr('p.bclr',     Format_R,    '0011100 ----- ----- 011 ----- 0101011', L='cv.bclrr'),
            Instr('p.bset',     Format_R,    '0011101 ----- ----- 011 ----- 0101011', L='cv.bsetr'),
        ]

        # --- Custom-2 (0x5B): Bit-Manipulation, Add/Sub Norm, Multiply, MAC ---
        # bits[14:13]=00: Bit manipulation (immediate)
        # Selector: {bits[31:30], bit[12]}
        instrs += [
            Instr('p.extracti',  Format_I4U,    '00----- ----- ----- 000 ----- 1011011', L='cv.extract'),
            Instr('p.extractui', Format_I4U,    '01----- ----- ----- 000 ----- 1011011', L='cv.extractu'),
            Instr('p.inserti',   Format_I5U,    '10----- ----- ----- 000 ----- 1011011', L='cv.insert'),
            Instr('p.bclri',     Format_I4U,    '00----- ----- ----- 001 ----- 1011011', L='cv.bclr'),
            Instr('p.bseti',     Format_I4U,    '01----- ----- ----- 001 ----- 1011011', L='cv.bset'),
            Instr('p.bitrev',    Format_BITREV, '11----- ----- ----- 001 ----- 1011011', L='cv.bitrev'),
        ]

        # --- Custom-2 (0x5B): Add/Sub Norm, Multiply, MAC ---
        # Using 'p.' prefix to use handlers from pulp_v2.hpp
        # v2 encoding: opcode=0x5B, bits[31:30] select variant, funct3 selects family:
        #   funct3=010: addN variants    funct3=011: subN variants
        #   funct3=100: mulsN variants   funct3=101: muluN variants
        #   funct3=110: macsN variants   funct3=111: macuN variants
        # bits[31:30]: 00=base, 01=hh(MAC/MUL)/unsigned(ADD/SUB), 10=round, 11=hh+round/unsigned+round

        # Add/Sub with Normalization (funct3=010 add, 011 sub)
        instrs += [
            Instr('p.addNi',         Format_RRRU2,'00----- ----- ----- 010 ----- 1011011', L='cv.addN'),
            Instr('p.adduNi',        Format_RRRU2,'01----- ----- ----- 010 ----- 1011011', L='cv.adduN'),
            Instr('p.addRNi',        Format_RRRU2,'10----- ----- ----- 010 ----- 1011011', L='cv.addRN'),
            Instr('p.adduRNi',       Format_RRRU2,'11----- ----- ----- 010 ----- 1011011', L='cv.adduRN'),
            Instr('p.subNi',         Format_RRRU2,'00----- ----- ----- 011 ----- 1011011', L='cv.subN'),
            Instr('p.subuNi',        Format_RRRU2,'01----- ----- ----- 011 ----- 1011011', L='cv.subuN'),
            Instr('p.subRNi',        Format_RRRU2,'10----- ----- ----- 011 ----- 1011011', L='cv.subRN'),
            Instr('p.subuRNi',       Format_RRRU2,'11----- ----- ----- 011 ----- 1011011', L='cv.subuRN'),
        ]

        # MUL operations (funct3=100 signed, 101 unsigned)
        instrs += [
            Instr('p.mulsN',         Format_RRRRU,'00----- ----- ----- 100 ----- 1011011', L='cv.mulsN'),
            Instr('p.mulhhsN',       Format_RRRRU,'01----- ----- ----- 100 ----- 1011011', L='cv.mulhhsN'),
            Instr('p.mulsNR',        Format_RRRRU,'10----- ----- ----- 100 ----- 1011011', L='cv.mulsRN'),
            Instr('p.mulhhsNR',      Format_RRRRU,'11----- ----- ----- 100 ----- 1011011', L='cv.mulhhsRN'),
            Instr('p.muluN',         Format_RRRRU,'00----- ----- ----- 101 ----- 1011011', L='cv.muluN'),
            Instr('p.mulhhuN',       Format_RRRRU,'01----- ----- ----- 101 ----- 1011011', L='cv.mulhhuN'),
            Instr('p.muluNR',        Format_RRRRU,'10----- ----- ----- 101 ----- 1011011', L='cv.muluRN'),
            Instr('p.mulhhuNR',      Format_RRRRU,'11----- ----- ----- 101 ----- 1011011', L='cv.mulhhuRN'),
        ]

        # MAC operations (funct3=110 signed, 111 unsigned)
        instrs += [
            Instr('p.macsN',         Format_RRRRU,'00----- ----- ----- 110 ----- 1011011', L='cv.macsN'),
            Instr('p.machhsN',       Format_RRRRU,'01----- ----- ----- 110 ----- 1011011', L='cv.machhsN'),
            Instr('p.macsNR',        Format_RRRRU,'10----- ----- ----- 110 ----- 1011011', L='cv.macsRN'),
            Instr('p.machhsNR',      Format_RRRRU,'11----- ----- ----- 110 ----- 1011011', L='cv.machhsRN'),
            Instr('p.macuN',         Format_RRRRU,'00----- ----- ----- 111 ----- 1011011', L='cv.macuN'),
            Instr('p.machhuN',       Format_RRRRU,'01----- ----- ----- 111 ----- 1011011', L='cv.machhuN'),
            Instr('p.macuNR',        Format_RRRRU,'10----- ----- ----- 111 ----- 1011011', L='cv.macuRN'),
            Instr('p.machhuNR',      Format_RRRRU,'11----- ----- ----- 111 ----- 1011011', L='cv.machhuRN'),
        ]

        # HWLoops — OPCODE_CUSTOM_1 = 0x2B, funct3=100 (Plane B)
        # RTL: cv32e40p_decoder.sv lines 1935-2008, instr[11:8] (rd[4:1]) selects instruction
        # instr[7] (rd[0]) = loop number (0 or 1)
        instrs += [
            Instr('lp.starti',Format_HL0,'------- ----- 00000 100 0000- 0101011', L='cv.starti'),
            Instr('lp.start', Format_HL0,'0000000 00000 ----- 100 0001- 0101011', L='cv.start'),
            Instr('lp.endi',  Format_HL0,'------- ----- 00000 100 0010- 0101011', L='cv.endi'),
            Instr('lp.end',   Format_HL0,'0000000 00000 ----- 100 0011- 0101011', L='cv.end'),
            Instr('lp.counti',Format_HL0,'------- ----- 00000 100 0100- 0101011', L='cv.counti'),
            Instr('lp.count', Format_HL0,'0000000 00000 ----- 100 0101- 0101011', L='cv.count'),
            Instr('lp.setupi',Format_HL1,'------- ----- ----- 100 0110- 0101011', L='cv.setupi'),
            Instr('lp.setup', Format_HL0,'------- ----- ----- 100 0111- 0101011', L='cv.setup'),
        ]

        # mac/msu using PULP handlers
        # Encoding from cv32e40p_decoder.sv line 1907 and xcvmac1p0 toolchain:
        # cv.mac: custom-1 (0x2B), funct3=011, funct7=1001000
        # cv.msu: custom-1 (0x2B), funct3=011, funct7=1001001 (bit[25]=1)
        instrs += [
            Instr('p.mac',           Format_RRRR, '1001000 ----- ----- 011 ----- 0101011', L='cv.mac'),
            Instr('p.msu',           Format_RRRR, '1001001 ----- ----- 011 ----- 0101011', L='cv.msu'),
        ]

        super().__init__(name='pulpv2', instrs=instrs)
