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

from __future__ import annotations

import collections



def dump(isaFile, str, level=0):
    for i in range(0, level):
        isaFile.write('  ')
    isaFile.write(str)



class Const(object):
    def __init__(self, val):
        self.val = val

    def len(self):
        return 1

    def gen_info(self, isaFile):
        dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_UIM, .u= { .uim= %d } }, ' % self.val)

    def gen(self, isaFile):
        pass



class Range(object):
    def __init__(self, first, width=1, shift=0):
        self.first = first
        self.width = width
        self.shift = shift

    def gen(self, isaFile, indent=0):
        dump(isaFile, '{%d, %d, %d}, ' % (self.first, self.width, self.shift))

    def gen_info(self, isaFile):
        dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_RANGE, .u= { .range_set= {.nb_ranges=1, .ranges={ ')
        self.gen(isaFile)
        dump(isaFile, '} } } }, ')

    def len(self):
        return 1



class Ranges(object):
    def __init__(self, fieldsList):
        self.ranges = []
        for range in fieldsList:
            if len(range) < 2: range.append(1)
            if len(range) < 3: range.append(0)
            self.ranges.append(Range(range[0], range[1], range[2]))

    def gen_info(self, isaFile, indent=0):
        dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_RANGE, .u= { .range_set= {.nb_ranges=%d, .ranges={ ' % (len(self.ranges)))
        for range in self.ranges:
            range.gen(isaFile)
        dump(isaFile, '} } } }, ')

    def len(self):
        return len(self.ranges)


class OpcodeField(object):
    def __init__(self, id, ranges, dumpName=True, flags=[]):
        self.ranges = ranges
        self.id = id
        self.isImm = False
        self.dumpName = dumpName
        self.flags = ['ISS_DECODER_ARG_FLAG_NONE'] + flags
        self.latency = 0

    def get_id(self):
        return self.id

    def is_reg(self):
        return False

    def set_latency(self, latency):
        self.latency = latency



class Indirect(OpcodeField):
    def __init__(self, base, offset=None, postInc=False, preInc=False):
        self.base = base
        self.offset = offset
        self.postInc = postInc
        self.preInc = preInc
        self.flags = ['ISS_DECODER_ARG_FLAG_NONE']

    def get_id(self):
        return self.base.get_id()

    def is_reg(self):
        return True

    def is_out(self):
        return False

    def genTrace(self, isaFile, level):
        if self.offset is not None and self.offset.isImm:
            funcName = 'traceSetIndirectImm'
        else:
            funcName = 'traceSetIndirectReg'
        if self.postInc:
            funcName += 'PostInc'
            self.flags.append('ISS_DECODER_ARG_FLAG_POSTINC')
        if self.preInc:
            funcName += 'PreInc'
            self.flags.append('ISS_DECODER_ARG_FLAG_PREINC')
        if self.offset is not None:
            dump(isaFile, level, '  %s(pc, %s, %s);\n' % (funcName, self.base.genTraceIndirect(), self.offset.genTraceIndirect()))
        else:
            dump(isaFile, level, '  %s(pc, %s, 0);\n' % (funcName, self.base.genTraceIndirect()))

    def gen(self, isaFile, indent=0):
        if self.postInc:
            self.flags.append('ISS_DECODER_ARG_FLAG_POSTINC')
        if self.preInc:
            self.flags.append('ISS_DECODER_ARG_FLAG_PREINC')
        if self.offset.is_reg():
            dump(isaFile, '%s{\n' % (' '*indent))
            dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_INDIRECT_REG,\n' % (' '*indent))
            dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
            dump(isaFile, '%s  .u= {\n' % (' '*indent))
            dump(isaFile, '%s    .indirect_reg= {\n' % (' '*indent))
            dump(isaFile, '%s      .base_reg= ' % (' '*indent))
            self.base.gen_info(isaFile)
            dump(isaFile, '\n%s      .offset_reg= ' % (' '*indent))
            self.offset.gen_info(isaFile)
            dump(isaFile, '\n%s    },\n' % (' '*indent))
            dump(isaFile, '%s  },\n' % (' '*indent))
            dump(isaFile, '%s},\n' % (' '*indent))
        else:
            dump(isaFile, '%s{\n' % (' '*indent))
            dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_INDIRECT_IMM,\n' % (' '*indent))
            dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
            dump(isaFile, '%s  .u= {\n' % (' '*indent))
            dump(isaFile, '%s    .indirect_imm= {\n' % (' '*indent))
            dump(isaFile, '%s      .reg= ' % (' '*indent))
            self.base.gen_info(isaFile)
            dump(isaFile, '\n%s      .imm= ' % (' '*indent))
            self.offset.gen_info(isaFile)
            dump(isaFile, '\n%s    },\n' % (' '*indent))
            dump(isaFile, '%s  },\n' % (' '*indent))
            dump(isaFile, '%s},\n' % (' '*indent))



class SignedImm(OpcodeField):
    def __init__(self, id, ranges, isSigned=True):
        self.ranges = ranges
        self.id = id
        self.isSigned = isSigned
        self.isImm = True

    def genTrace(self, isaFile, level):
        dump(isaFile, level, '  traceSetSimm(pc, pc->sim[%d]);\n' % (self.id))

    def genTraceIndirect(self):
        return 'pc->sim[%d]' % (self.id)

    def gen_info(self, isaFile):
        dump(isaFile, '{ .is_signed=%d, .id=%d, .info= ' % (self.isSigned, self.id))
        self.ranges.gen_info(isaFile)
        dump(isaFile, '}, ')

    def gen(self, isaFile, indent=0):
        dump(isaFile, '%s{\n' % (' '*indent))
        dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_SIMM,\n' % (' '*indent))
        dump(isaFile, '%s  .flags=ISS_DECODER_ARG_FLAG_NONE,\n' % (' '*indent))
        dump(isaFile, '%s  .u= {\n' % (' '*indent))
        dump(isaFile, '%s    .simm= ' % (' '*indent))
        self.gen_info(isaFile)
        dump(isaFile, '\n%s  },\n' % (' '*indent))
        dump(isaFile, '%s},\n' % (' '*indent))



class UnsignedImm(OpcodeField):
    def __init__(self, id, ranges, isSigned=False):
        self.ranges = ranges
        self.id = id
        self.isSigned = isSigned
        self.isImm = True

    def genTrace(self, isaFile, level):
        dump(isaFile, level, '  traceSetUimm(pc, pc->uim[%d]);\n' % (self.id))

    def genTraceIndirect(self):
        return 'pc->uim[%d]' % (self.id)

    def gen_info(self, isaFile):
        dump(isaFile, '{ .is_signed=%d, .id=%d, .info= ' % (self.isSigned, self.id))
        self.ranges.gen_info(isaFile)
        dump(isaFile, '}, ')

    def gen(self, isaFile, indent=0):
        dump(isaFile, '%s{\n' % (' '*indent))
        dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_UIMM,\n' % (' '*indent))
        dump(isaFile, '%s  .flags=ISS_DECODER_ARG_FLAG_NONE,\n' % (' '*indent))
        dump(isaFile, '%s  .u= {\n' % (' '*indent))
        dump(isaFile, '%s    .uimm= ' % (' '*indent))
        self.gen_info(isaFile)
        dump(isaFile, '\n%s  },\n' % (' '*indent))
        dump(isaFile, '%s},\n' % (' '*indent))



class OutReg(OpcodeField):
    def genTrace(self, isaFile, level):
        dump(isaFile, level, '  traceSetReg(pc, pc->outReg[%d], 1, %d);\n' % (self.id, self.dumpName))

    def is_out(self):
        return True

    def is_reg(self):
        return True

    def gen_info(self, isaFile):
        dump(isaFile, '{ .id=%d, .flags=(iss_decoder_arg_flag_e)(%s), .dump_name=%d, .latency=%d, .info= ' % (self.id, ' | '.join(self.flags), 1 if self.dumpName else 0, self.latency))
        self.ranges.gen_info(isaFile)
        dump(isaFile, '}, ')

    def gen(self, isaFile, indent=0):
        dump(isaFile, '%s{\n' % (' '*indent))
        dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_OUT_REG,\n' % (' '*indent))
        dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
        dump(isaFile, '%s  .u= {\n' % (' '*indent))
        dump(isaFile, '%s    .reg= ' % (' '*indent))
        self.gen_info(isaFile)
        dump(isaFile, '\n%s  },\n' % (' '*indent))
        dump(isaFile, '%s},\n' % (' '*indent))



class OutFReg(OutReg):
    def __init__(self, id, ranges, f=None, dumpName=True):
        flags = ['ISS_DECODER_ARG_FLAG_FREG']
        if f == 'h': flags.append('ISS_DECODER_ARG_FLAG_ELEM_16')
        elif f == 'ah': flags.append('ISS_DECODER_ARG_FLAG_ELEM_16A')
        elif f == 'b': flags.append('ISS_DECODER_ARG_FLAG_ELEM_8')
        elif f == 'bh': flags.append('ISS_DECODER_ARG_FLAG_ELEM_8A')
        elif f == 's': flags.append('ISS_DECODER_ARG_FLAG_ELEM_32')
        elif f == 'd': flags.append('ISS_DECODER_ARG_FLAG_ELEM_64')
        elif f == 'H': flags += ['ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'AH': flags += ['ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'B': flags += ['ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'BH': flags += ['ISS_DECODER_ARG_FLAG_ELEM_8A', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'S': flags += ['ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'D': flags += ['ISS_DECODER_ARG_FLAG_ELEM_64', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'sew': flags += ['ISS_DECODER_ARG_FLAG_ELEM_SEW']
        super(OutFReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=flags)

class OutFRegS(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegS, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegH(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegH, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegAH(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegAH, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegB(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegB, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegAB(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegAB, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_8B', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegVecS(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegVecS, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegVecH(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegVecH, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegVecAH(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegVecAH, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegVecB(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegVecB, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutFRegVecAB(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegVecAB, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_8B', 'ISS_DECODER_ARG_FLAG_FREG'])

class OutVReg(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutVReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_VREG'])

class OutVRegF(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutVRegF, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_VREG', 'ISS_DECODER_ARG_FLAG_FREG'])



class OutReg64(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutReg64, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_REG64'])



class OutRegComp(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED'])



class OutFRegComp(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED', 'ISS_DECODER_ARG_FLAG_FREG'])



class InReg(OpcodeField):
    def genTrace(self, isaFile, level):
        dump(isaFile, level, '  traceSetReg(pc, pc->inReg[%d], 0, %d);\n' % (self.id, self.dumpName))

    def genTraceIndirect(self):
        return 'pc->inReg[%d]' % (self.id)

    def is_out(self):
        return False

    def is_reg(self):
        return True

    def gen_info(self, isaFile):
        dump(isaFile, '{ .id=%d, .flags=(iss_decoder_arg_flag_e)(%s), .dump_name=%d, .latency=%d, .info= ' % (self.id, ' | '.join(self.flags), 1 if self.dumpName else 0, self.latency))
        self.ranges.gen_info(isaFile)
        dump(isaFile, '}, ')

    def gen(self, isaFile, indent=0):
        dump(isaFile, '%s{\n' % (' '*indent))
        dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_IN_REG,\n' % (' '*indent))
        dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
        dump(isaFile, '%s  .u= {\n' % (' '*indent))
        dump(isaFile, '%s    .reg= ' % (' '*indent))
        self.gen_info(isaFile)
        dump(isaFile, '\n%s  },\n' % (' '*indent))
        dump(isaFile, '%s},\n' % (' '*indent))



class InFReg(InReg):
    def __init__(self, id, ranges, f=None, dumpName=True):
        flags = ['ISS_DECODER_ARG_FLAG_FREG']
        if f == 'h': flags.append('ISS_DECODER_ARG_FLAG_ELEM_16')
        elif f == 'ah': flags.append('ISS_DECODER_ARG_FLAG_ELEM_16A')
        elif f == 'b': flags.append('ISS_DECODER_ARG_FLAG_ELEM_8')
        elif f == 'bh': flags.append('ISS_DECODER_ARG_FLAG_ELEM_8A')
        elif f == 's': flags.append('ISS_DECODER_ARG_FLAG_ELEM_32')
        elif f == 'd': flags.append('ISS_DECODER_ARG_FLAG_ELEM_64')
        elif f == 'H': flags += ['ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'AH': flags += ['ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'B': flags += ['ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'BH': flags += ['ISS_DECODER_ARG_FLAG_ELEM_8A', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'S': flags += ['ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_VEC']
        elif f == 'sew': flags += ['ISS_DECODER_ARG_FLAG_ELEM_SEW']
        super(InFReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=flags)

class InFRegS(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegS, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegH(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegH, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegAH(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegAH, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegB(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegB, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegAB(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegAB, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_ELEM_8A', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegVecS(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegVecS, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_32', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegVecH(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegVecH, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_16', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegVecAH(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegVecAH, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_16A', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegVecB(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegVecB, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_8', 'ISS_DECODER_ARG_FLAG_FREG'])

class InFRegVecAB(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegVecAB, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_ELEM_8A', 'ISS_DECODER_ARG_FLAG_FREG'])


class InVReg(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InVReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_VREG'])

class InVRegF(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InVRegF, self).__init__(id=id, ranges=ranges, dumpName=dumpName,
            flags=['ISS_DECODER_ARG_FLAG_VEC', 'ISS_DECODER_ARG_FLAG_VREG', 'ISS_DECODER_ARG_FLAG_FREG'])



class InReg64(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InReg64, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_REG64'])



class InRegComp(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED'])



class InFRegComp(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED', 'ISS_DECODER_ARG_FLAG_FREG'])



def decode_pattern(instr):
    # Bits the encoding of an instruction fixes, and their values
    mask = 0
    value = 0
    for bit, char in enumerate(instr.encoding):
        if char in ['0', '1']:
            mask |= 1 << bit
            if char == '1':
                value |= 1 << bit
    return mask, value


def decode_lowest_field(mask):
    # Lowest run of contiguous bits of a mask, as (first bit, width)
    first = (mask & -mask).bit_length() - 1
    width = 0
    while mask & (1 << (first + width)):
        width += 1
    return first, width


def decode_most_specific(instrs, patterns):
    # Among instructions all matching the same opcodes, the one whose pattern
    # fixes all the bits the others fix
    best = [instr for instr in instrs
        if all(patterns[other][0] & ~patterns[instr][0] == 0 for other in instrs)]

    if len(best) == 0:
        # Partly overlapping patterns are fine if the opcodes they all match
        # are decoded elsewhere: an instruction outside this leaf matching all
        # of them means the decoder never reaches it with one of them (it was
        # excluded on the way through an OTHERS group).
        mask = 0
        value = 0
        for instr in instrs:
            mask |= patterns[instr][0]
            value |= patterns[instr][1]
        for other, (other_mask, other_value) in patterns.items():
            if other not in instrs and other_mask & ~mask == 0 and (other_value ^ value) & other_mask == 0:
                return instrs[0]

    if len(best) != 1:
        error = 'The decoder cannot choose between the following instructions:\n'
        for instr in instrs:
            error += instr.get_str() + '\n'
        raise Exception(error)
    return best[0]


def decode_node(isa, instrs, patterns, checked, opcode, others):
    if all(patterns[instr][0] & ~checked[instr] == 0 for instr in instrs):
        return DecodeLeaf(decode_most_specific(instrs, patterns), opcode, others)
    return DecodeTree(isa, instrs, opcode, others, patterns, checked)



class DecodeLeaf(object):
    def __init__(self, instr, opcode, others=False):
        self.instr = instr
        self.opcode = opcode
        self.others = others
        self.wrapper_id = None

    def count_positions(self, positions):
        positions[self.instr] += 1

    def get_name(self):
        if self.wrapper_id is not None:
            return 'decoder_tree_%d' % self.wrapper_id
        return self.instr.get_full_name()

    def gen_node(self, isaFile, isa, positions, generated):
        if positions[self.instr] == 1:
            # Only position of the instruction, its own item carries the opcode
            dump(isaFile, '// %s\n' % (self.instr.get_str()))
            self.instr.gen(isa, isaFile, self.opcode, self.others)
            generated.add(self.instr)
            return

        # The instruction has several positions in the tree, each one is a group
        # with no opcode bits holding the instruction item
        if self.instr not in generated:
            dump(isaFile, '// %s\n' % (self.instr.get_str()))
            self.instr.gen(isa, isaFile, '0', True)
            generated.add(self.instr)

        self.wrapper_id = isa.alloc_decoder_tree_id()
        name = self.get_name()
        dump(isaFile, 'static iss_decoder_item_t *%s_groups[] = { &%s, };\n' % (name, self.instr.get_full_name()))
        dump(isaFile, 'static iss_decoder_item_t %s = {\n' % name)
        dump(isaFile, '  .is_insn=false,\n')
        dump(isaFile, '  .is_active=false,\n')
        dump(isaFile, '  .opcode_others=%d,\n' % (1 if self.others else 0))
        dump(isaFile, '  .opcode=0b%s,\n' % self.opcode)
        dump(isaFile, '  .u={\n')
        dump(isaFile, '    .group={\n')
        dump(isaFile, '      .bit=0,\n')
        dump(isaFile, '      .width=0,\n')
        dump(isaFile, '      .nb_groups=1,\n')
        dump(isaFile, '      .groups=%s_groups\n' % name)
        dump(isaFile, '    }\n')
        dump(isaFile, '  }\n')
        dump(isaFile, '};\n')
        dump(isaFile, '\n')



class DecodeTree(object):
    # Decoding group: the decoder extracts opcode_width bits at firstBit and
    # continues with the subtree of the same opcode, or with the OTHERS one if
    # none matches (see Decode::decode_opcode_group).
    #
    # An instruction goes into every subtree whose opcodes its pattern can
    # match, including the ones where the extracted bits are don't-care for
    # it, so that the decoder always ends on the instruction with the most
    # specific matching pattern. checked tells for each instruction which of
    # its fixed bits the decoder already compared on the way to this group.
    def __init__(self, isa, instrs, opcode='0', others=False, patterns=None, checked=None):
        self.isa = isa
        self.instrs = instrs
        self.opcode = opcode
        self.others = others
        self.tree_id = isa.alloc_decoder_tree_id()
        self.subtrees = collections.OrderedDict()
        self.firstBit = 0
        self.opcode_width = 0

        if patterns is None:
            patterns = {instr: decode_pattern(instr) for instr in instrs}
        if checked is None:
            checked = {instr: 0 for instr in instrs}

        if len(instrs) == 0:
            return

        remaining = {instr: patterns[instr][0] & ~checked[instr] for instr in instrs}

        if all(mask == 0 for mask in remaining.values()):
            # Only happens at the top, when a single pattern remains
            self.subtrees['OTHERS'] = DecodeLeaf(decode_most_specific(instrs, patterns), '0', True)
            return

        # Extract bits all instructions still have to check if any, otherwise
        # bits common to all instructions with bits to check, otherwise bits of
        # the instruction with the most bits to check. At least one instruction
        # fixes all of them, which makes each subtree progress.
        common = ~0
        for mask in remaining.values():
            common &= mask
        if common == 0:
            common = ~0
            for mask in remaining.values():
                if mask != 0:
                    common &= mask
        if common == 0:
            common = max(remaining.values(), key=lambda mask: bin(mask).count('1'))

        self.firstBit, self.opcode_width = decode_lowest_field(common)
        field_mask = ((1 << self.opcode_width) - 1) << self.firstBit

        def field_value(value):
            return (value >> self.firstBit) & ((1 << self.opcode_width) - 1)

        keys = []
        others_instrs = []
        for instr in instrs:
            mask, value = patterns[instr]
            if mask & field_mask == field_mask:
                if field_value(value) not in keys:
                    keys.append(field_value(value))
            else:
                others_instrs.append(instr)

        for key in keys:
            group = []
            group_checked = dict(checked)
            for instr in instrs:
                mask, value = patterns[instr]
                if (value ^ (key << self.firstBit)) & mask & field_mask == 0:
                    group.append(instr)
                    group_checked[instr] = checked[instr] | (mask & field_mask)

            opcode_str = format(key, '0%db' % self.opcode_width)
            self.subtrees[opcode_str] = decode_node(isa, group, patterns, group_checked, opcode_str, False)

        if len(others_instrs) != 0:
            self.subtrees['OTHERS'] = decode_node(isa, others_instrs, patterns, checked, '0', True)

    def count_positions(self, positions):
        for subtree in self.subtrees.values():
            subtree.count_positions(positions)

    def get_name(self):
        return 'decoder_tree_%d' % self.tree_id

    def gen(self, isaFile, isa):
        positions = collections.Counter()
        self.count_positions(positions)
        generated = set()

        self.gen_node(isaFile, isa, positions, generated)

        # Instructions shadowed everywhere by more specific ones are never
        # decoded but are still referenced by the instruction tables
        for instr in self.instrs:
            if instr not in generated:
                dump(isaFile, '// %s\n' % (instr.get_str()))
                instr.gen(isa, isaFile, '0', False)
                generated.add(instr)

    def gen_node(self, isaFile, isa, positions, generated):
        for subtree in self.subtrees.values():
            subtree.gen_node(isaFile, isa, positions, generated)

        dump(isaFile, 'static iss_decoder_item_t *%s_groups[] = {' % self.get_name())
        for subtree in self.subtrees.values():
            dump(isaFile, ' &%s,' % subtree.get_name())
        dump(isaFile, ' };\n')

        dump(isaFile, 'static iss_decoder_item_t %s = {\n' % (self.get_name()))
        dump(isaFile, '  .is_insn=false,\n')
        dump(isaFile, '  .is_active=false,\n')
        dump(isaFile, '  .opcode_others=%d,\n' % (1 if self.others else 0))
        dump(isaFile, '  .opcode=0b%s,\n' % self.opcode)
        dump(isaFile, '  .u={\n')
        dump(isaFile, '    .group={\n')
        dump(isaFile, '      .bit=%d,\n' % self.firstBit)
        dump(isaFile, '      .width=%d,\n' % self.opcode_width)
        dump(isaFile, '      .nb_groups=%d,\n' % len(self.subtrees))
        dump(isaFile, '      .groups=%s_groups\n' % self.get_name())
        dump(isaFile, '    }\n')
        dump(isaFile, '  }\n')
        dump(isaFile, '};\n')
        dump(isaFile, '\n')



class IsaSubset(object):
    def __init__(self, name: str, instrs: list["Instr"], includes:list[str]|None=None):
        self.name = name
        self.instrs = instrs
        self.instrs_dict = {}
        self.top_isa = None
        self.includes = includes if includes is not None else []
        for instr in instrs:
            instr.set_isa(self)
            self.instrs_dict[instr.name] = instr

    def add_insn_tag(self, insn, tag):
        if self.top_isa is not None:
            self.top_isa.add_insn_tag(insn, tag)

    def check_compatibilities(self, isa):
        pass

    def get_insns(self):
        return self.instrs

    def get_insn(self, name):
        return self.instrs_dict.get(name)

    def get_includes(self):
        return self.includes


class Resource(object):

    def __init__(self, name, instances=1):
        self.name = name
        self.instances = instances



class Isa(object):
    def __init__(self, name, isa_string):
        self.level = 0
        self.isa_string = isa_string
        self.nb_decoder_tree = 0
        self.name = name
        self.isas = {}
        self.isa_tags_insns = {}
        self.instrs_dict = {}
        self.includes = []
        self.implem_includes = []
        self.defines = {}

    def get_isa_subsets_includes(self):
        includes = []
        for isa in self.isas.values():
            includes += isa.get_includes()

        return includes

    def add_include(self, include: str):
        if not include in self.includes:
            self.includes.append(include)

    def add_implem_include(self, include: str):
        if not include in self.implem_includes:
            self.implem_includes.append(include)

    def add_define(self, name: str, value: str):
        self.defines[name] = value

    def alloc_decoder_tree_id(self):
        result = self.nb_decoder_tree
        self.nb_decoder_tree += 1
        return result

    def check_compatibilities(self):
        for isa in self.isas.values():
            isa.check_compatibilities(self)

    def has_isa(self, name):
        return self.isas.get(name) is not None

    def get_isa(self, name):
        return self.isas.get(name)

    def disable_from_isa_tag(self, tag):
        for insn in self.get_insns():
            if tag in insn.isa_tags:
                insn.set_active(False)

    def add_insn_tag(self, insn, tag):
        if self.isa_tags_insns.get(tag) is None:
                self.isa_tags_insns[tag] = []

        self.isa_tags_insns[tag].append(f'&{insn.get_full_name()}')

    def add_isa(self, isa):
        self.isas[isa.name] = isa
        isa.top_isa = self

        for insn in isa.get_insns():
            self.instrs_dict[insn.name] = insn
            for tag in insn.tags:
                if self.isa_tags_insns.get(tag) is None:
                    self.isa_tags_insns[tag] = []

                self.isa_tags_insns[tag].append(f'&{insn.get_full_name()}')

    def get_insn(self, name):
        return self.instrs_dict.get(name)

    def get_insns(self):
        result = []

        for isa in self.isas.values():
            result += isa.get_insns()

        return result

    def dump(self, isaFile, str):
        for i in range(0, self.level):
            isaFile.write('  ')
        isaFile.write(str)

    def gen(self, isaFile, isaFileHeader):

        tree = DecodeTree(self, self.get_insns())

        isaFileHeader.write('#pragma once\n')

        dump(isaFile, '#ifdef CONFIG_GVSOC_ISS_V2\n')
        dump(isaFile, '#include <cpu/iss_v2/include/iss.hpp>\n')
        dump(isaFile, '#else\n')
        dump(isaFile, '#include <cpu/iss/include/iss.hpp>\n')
        dump(isaFile, '#endif\n')
        dump(isaFile, '\n')

        tree.gen(isaFile, self)

        self.dump_tag_insns(isaFile, isaFileHeader)

        for key, value in self.defines.items():
            if value is None:
                dump(isaFileHeader, f'#define {value}\n')
            else:
                dump(isaFileHeader, f'#define {key} {value}\n')
        dump(isaFileHeader, '#ifdef __cplusplus\n')
        for include in self.includes:
            dump(isaFileHeader, f'#include {include}\n')
        dump(isaFileHeader, '#ifdef CONFIG_GVSOC_ISS_V2\n')
        dump(isaFileHeader, '#include <cpu/iss_v2/include/iss.hpp>\n')
        for include in self.implem_includes:
            dump(isaFileHeader, f'#include {include}\n')
        for include in self.get_isa_subsets_includes():
            dump(isaFileHeader, f'#include {include}\n')
        dump(isaFileHeader, '#endif\n')
        dump(isaFileHeader, '#endif\n')

        dump(isaFile, 'iss_isa_set_t __iss_isa_set = {\n')
        dump(isaFile, f'  .isa_set=&{tree.get_name()},\n')
        dump(isaFile, '  .tag_insns=tag_insn,\n')
        dump(isaFile, '  .isa_insns=isa_insn,\n')
        dump(isaFile, '  .insns=insns,\n')
        dump(isaFile, '  .initialized=false,\n')
        dump(isaFile, '};\n')
        dump(isaFile, '\n')

    def dump_tag_insns(self, isaFile, isa_file_header):

        tag_id = 0
        for tag_name, insns in self.isa_tags_insns.items():
            dump(isa_file_header, f'#define ISA_TAG_{tag_name.upper()}_ID {tag_id}\n')
            tag_id += 1
        dump(isa_file_header, f'#define ISA_NB_TAGS {len(self.isa_tags_insns)}\n')
        for tag_name, insns in self.isa_tags_insns.items():
            tag_struct_name = f'tag_insn_{tag_name}'
            dump(isaFile, f'static std::vector<iss_decoder_item_t *> {tag_struct_name} = {{ {", ".join(insns)} }};\n\n')

        dump(isaFile, f'static std::unordered_map<std::string, std::vector<iss_decoder_item_t *> *> tag_insn = {{\n')
        for tag_name in self.isa_tags_insns.keys():
            tag_struct_name = f'&tag_insn_{tag_name}'
            dump(isaFile, f'    {{ "{tag_name}", {tag_struct_name} }},\n')
        dump(isaFile, f'}};\n\n')

        dump(isaFile, f'static std::unordered_map<std::string, iss_decoder_item_t *> insns = {{\n')
        for insn in self.get_insns():
            dump(isaFile, f'    {{ "{insn.name}", &{insn.get_full_name()} }},\n')
        dump(isaFile, f'}};\n\n')

        for isa in self.isas.values():
            struct_name = f'isa_insn_{isa.name}'
            insn_list = []
            for insn in isa.get_insns():
                insn_list.append(f'&{insn.get_full_name()}')
            dump(isaFile, f'static std::vector<iss_decoder_item_t *> {struct_name} = {{ {", ".join(insn_list)} }};\n\n')

        dump(isaFile, f'static std::unordered_map<std::string, std::vector<iss_decoder_item_t *> *> isa_insn = {{\n')
        for isa in self.isas.values():
            struct_name = f'&isa_insn_{isa.name}'
            dump(isaFile, f'    {{ "{isa.name}", {struct_name} }},\n')
        dump(isaFile, f'}};\n\n')




class Instr(object):
    def __init__(self, label, format, encoding, decode=None, L=None,
            fast_handler=False, tags=[], isa_tags=[], is_macro_op=False):
        self.tags = tags + ['all']
        self.isa_tags = isa_tags
        self.out_reg_latencies = []
        self.latency = 0
        self.power_group = 0
        self.is_macro_op = is_macro_op
        self.format = format

        args_format = format.copy()

        # Sort arguments as model will access arguments based on their ID
        args_format.sort(key=lambda obj: obj.get_id())

        self.args_format = args_format
        self.label = label
        self.active = True
        self.isa = None

        # Reverse the encoding string
        encoding = encoding[::-1].replace(' ', '')
        if L != None:
            self.trace_label = L
        else:
            self.trace_label = label
        self.name = label.replace('.', '_')
        self.encoding = encoding
        self.decode = decode
        self.fast_handler = fast_handler
        self.set_exec_label(self.name)

        self.decode_func = '%s_decode_gen' % (self.name)
        self.len = len(encoding.strip())
        self.fields = []

    def add_field(self, name, value):
        self.fields.append([name, value])

    def add_tag(self, tag):
        self.tags.append(tag)
        if self.isa is not None:
            self.isa.add_insn_tag(self, tag)

    def set_exec_label(self, name):
        self.exec_func = f'{name}_exec'
        if not self.fast_handler:
            self.exec_func_fast = self.exec_func
        else:
            self.exec_func_fast = self.exec_func + '_fast'


    def set_isa(self, isa):
        self.isa = isa

    def set_latency(self, latency):
        self.latency = latency

    def set_power_group(self, power_group):
        self.power_group = power_group

    def get_out_reg(self, reg):
        index = 0
        for arg in self.args_format:
            if arg.is_reg() and arg.is_out():
                if index == reg:
                    return arg
                index += 1
        return None

    def get_full_name(self):
        return ('insn_%s_%s' % (self.isa.name, self.name)).replace('.', '_')

    def get_label(self):
        return self.trace_label

    def get_str(self):
        return '%s: %s' % (self.encoding[::-1], self.label)

    def dump(self, isaFile, str, level=0):
        for i in range(0, level):
            isaFile.write('  ')
        isaFile.write(str)

    def set_active(self, active):
        self.active = active

    def gen(self, isa, isaFile, opcode, others=False):

        name = self.get_full_name()

        insn_tags_value = []
        for tag_name in isa.isa_tags_insns.keys():
            insn_tags_value.append('1' if tag_name in self.tags else '0')

        dump(isaFile, f'static iss_decoder_item_t {name} = {{\n')
        dump(isaFile, f'  .is_insn=true,\n')
        dump(isaFile, f'  .is_active={ 1 if self.active else 0},\n')
        dump(isaFile, f'  .opcode_others={1 if others else 0},\n')
        dump(isaFile, f'  .opcode=0b{opcode},\n')
        dump(isaFile, f'  .u={{\n')
        dump(isaFile, f'    .insn={{\n')
        dump(isaFile, f'      .handler={self.exec_func},\n')
        dump(isaFile, f'      .fast_handler={self.exec_func_fast},\n')
        dump(isaFile, f'      .stub_handler=NULL,\n')
        dump(isaFile, f'      .decode={"NULL" if self.decode is None else self.decode},\n')
        dump(isaFile, f'      .label=(char *)"{self.get_label()}",\n')
        dump(isaFile, f'      .size={int(self.len/8)},\n')
        dump(isaFile, f'      .nb_args={len(self.args_format)},\n')
        dump(isaFile, f'      .latency={self.latency},\n')
        dump(isaFile, f'      .args= {{\n')
        args_order = [ -1 ] * len(self.args_format)
        nb_args = 0
        if len(self.args_format) > 0:
            for arg in self.args_format:
                if arg.is_reg() and arg.is_out():
                    index = self.format.index(arg)
                    arg.gen(isaFile, indent=8)
                    args_order[index] = str(nb_args)
                    nb_args += 1

        if len(self.args_format) > 0:
            for arg in self.args_format:
                if arg.is_reg() and not arg.is_out():
                    index = self.format.index(arg)
                    arg.gen(isaFile, indent=8)
                    args_order[index] = str(nb_args)
                    nb_args += 1
        if len(self.args_format) > 0:
            for arg in self.args_format:
                if not arg.is_reg():
                    index = self.format.index(arg)
                    arg.gen(isaFile, indent=8)
                    args_order[index] = str(nb_args)
                    nb_args += 1

        dump(isaFile, f'      }},\n')
        dump(isaFile, f'      .resource_id=-1,\n')
        dump(isaFile, f'      .block_id=-1,\n')
        dump(isaFile, f'      .power_group={self.power_group},\n')
        dump(isaFile, f'      .is_macro_op={1 if self.is_macro_op else 0},\n')
        dump(isaFile, f'      .tags={{{", ".join(insn_tags_value)}}},\n')
        dump(isaFile, f'      .args_order={{{", ".join(args_order)}}},\n')
        for field in self.fields:
            dump(isaFile, f'      .{field[0]}={field[1]},\n')


        dump(isaFile, f'    }}\n')
        dump(isaFile, f'  }}\n')
        dump(isaFile, f'}};\n')
        dump(isaFile, f'\n')

class Insn(Instr):
    def __init__(self, encoding, label, format, decode=None, L=None,
                fast_handler=False, tags=[], isa_tags=[], is_macro_op=False):
        super(Insn, self).__init__(label, format, encoding, decode=decode, L=L,
            fast_handler=fast_handler, tags=tags, isa_tags=isa_tags, is_macro_op=is_macro_op)
