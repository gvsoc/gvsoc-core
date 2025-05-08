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



class DecodeLeaf(object):
    def __init__(self, instr, opcode, others=False):
        self.instr = instr
        self.level = 0
        self.opcode = opcode
        self.others = others

    def dump(self, isaFile, str):
        isaFile.write(str)

    def gen(self, isaFile, isa):
        dump(isaFile, '// %s\n' % (self.instr.get_str()))
        self.instr.gen(isa, isaFile, self.opcode, self.others)

    def get_name(self):
        return self.instr.get_full_name()



class DecodeTree(object):
    def __init__(self, isa, instrs, mask=0xffffffff, opcode='0'):
        self.opcode = opcode
        self.level = 0
        self.subtrees = collections.OrderedDict({})
        self.firstBit = 0
        self.tree_id = isa.alloc_decoder_tree_id()
        self.isa = isa

        if len(instrs) == 0: return

        # Resize the mask to the min size of the instructions to take
        # into accpount only bits discriminating the various sizes
        # otherwise we could take invalid bits into account
        minSize = None
        for instr in instrs:
            if minSize == None or instr.len < minSize: minSize = instr.len
        currentMask = mask & ((1<<minSize)-1)

        # First eliminate the bits in the mask where no instruction has
        # a discriminating bit
        searchMask = 0
        for instr in instrs:
            for bit in range(0, minSize):
                if (currentMask & (1<<bit)) == 0: continue
                if instr.encoding[bit] in ['0', '1']: searchMask |= 1<<bit
        currentMask &= searchMask

        # Browse all instructions to find the biggest common opcode set
        searchMask = currentMask
        for instr in instrs:

            #print ('Parsing instr %s: %s' % (instr.encoding, instr.label))
            done = False
            firstBit = None
            size = instr.len

            for bit in range(0, size):
                if done or not instr.encoding[bit] in ['0', '1'] or (searchMask & (1<<bit)) == 0: #, '-']:
                    searchMask &= ~(1<<bit)
                    if firstBit != None:
                        done = True
                    continue
                elif firstBit == None:
                    firstBit = bit

            if firstBit != None and firstBit > self.firstBit: self.firstBit = firstBit

        # In case we found no common mask, maybe we still have some discriminating
        # bits where some instructions don't care
        # Find a common mask where the opcode is either fully defined or don't care
        if searchMask == 0 and len(instrs) > 1:
            # First find the biggest common discriminating bits
            searchMask = currentMask
            for instr in instrs:
                instrMask = 0
                for bit in range(0, size):
                    if (currentMask & (1<<bit)) == 0: continue
                    if instr.encoding[bit] in ['0', '1']:
                        instrMask |= 1<<bit
                if instrMask != 0:
                    searchMask &= instrMask

            for bit in range(0, size):
                if searchMask & (1<<bit):
                    self.firstBit = bit
                    break

        if searchMask == 0 and len(instrs) > 1:
            error = 'Error the following instructions have the same opcode:\n'
            for instr in instrs:
                error += instr.get_str() + '\n'
            raise Exception(error)

        currentMask = searchMask

        self.currentMask = currentMask
        mask = mask & ~currentMask

        # Now group them together in sub-groups depending on their
        # opcode for this mask
        groups = collections.OrderedDict({})
        for instr in instrs:
            size = instr.len

            opcodeList = ['']
            isOther = False
            # Extract opcode
            for bit in range(size-1, -1, -1):
                if (currentMask & (1<<bit)) == 0: continue
                instrBit = instr.encoding[bit]
                if instrBit in ['0', '1']:
                    instrBits = [instrBit]
                else:
                    isOther = True
                    break
                newList = []
                for bit in instrBits:
                    for opcode in opcodeList:
                        newList.append(opcode + bit)
                opcodeList = newList

            # Now register the instruction for each of its possible opcode
            if not isOther:
                for opcode in opcodeList:
                    if groups.get(opcode) == None: groups[opcode] = []
                    groups[opcode].append(instr)
            else:
                if groups.get('OTHERS') == None: groups['OTHERS'] = []
                groups['OTHERS'].append(instr)

        self.opcode_width = 0
        for opcode, instrs in groups.items():
            if opcode == 'OTHERS': continue
            if len(opcode) > self.opcode_width:
                self.opcode_width = len(opcode)
            names = []
            for instr in instrs:
                names.append(instr.label)

        for opcode, instrs in groups.items():
            if len(instrs) == 1 and currentMask == 0 or opcode == 'OTHERS':
                self.subtrees[opcode] = DecodeLeaf(instrs[0], self.opcode, others=opcode == 'OTHERS')
            else: self.subtrees[opcode] = DecodeTree(self.isa, instrs, mask, opcode)

        self.mask = currentMask

        self.needTree = False
        for opcode, subtree in self.subtrees.items():
            if len(opcode) != 0: self.needTree = True

    def dump(self, isaFile, str, diff=0):
        isaFile.write(str)

    def get_name(self):
        if self.needTree:
            return 'decoder_tree_%d' % self.tree_id
        else:
            return list(self.subtrees.values())[0].get_name()

    def gen(self, isaFile, isa):

        if len(self.subtrees) != 0:

            if self.needTree:
                others = None
                for opcode, subtree in self.subtrees.items():
                    if opcode == 'OTHERS':
                        others = subtree
                    else:
                        subtree.gen(isaFile, isa)
                if others != None:
                    others.gen(isaFile, isa)
            else:
                subtree = list(self.subtrees.values())[0]
                subtree.gen(isaFile, isa)


            if self.needTree:
                dump(isaFile, 'static iss_decoder_item_t *%s_groups[] = {' % self.get_name());
                for opcode, subtree in self.subtrees.items():
                    dump(isaFile, ' &%s,' % subtree.get_name())

                dump(isaFile, ' };\n')

                dump(isaFile, 'static iss_decoder_item_t %s = {\n' % (self.get_name()))
                dump(isaFile, '  .is_insn=false,\n')
                dump(isaFile, '  .is_active=false,\n')
                dump(isaFile, '  .opcode_others=0,\n')
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
    def __init__(self, name, instrs):
        self.name = name
        self.instrs = instrs
        self.instrs_dict = {}
        self.top_isa = None
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

        dump(isaFile, '#include <cpu/iss/include/iss.hpp>\n')
        dump(isaFile, '\n')

        tree.gen(isaFile, self)

        self.dump_tag_insns(isaFile, isaFileHeader)

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
        dump(isa_file_header, f'#define ISA_NB_TAGS {len(self.isa_tags_insns)}')
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
        dump(isaFile, f'      .args_order={{{", ".join(args_order)}}}\n')
        dump(isaFile, f'    }}\n')
        dump(isaFile, f'  }}\n')
        dump(isaFile, f'}};\n')
        dump(isaFile, f'\n')

class Insn(Instr):
    def __init__(self, encoding, label, format, decode=None, L=None,
                fast_handler=False, tags=[], isa_tags=[], is_macro_op=False):
        super(Insn, self).__init__(label, format, encoding, decode=decode, L=L,
            fast_handler=fast_handler, tags=tags, isa_tags=isa_tags, is_macro_op=is_macro_op)
