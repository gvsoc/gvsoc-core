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

instrLabels = collections.OrderedDict({})
insn_isa_tags = collections.OrderedDict({})
instrLabelsList = []
nb_insn = 0
nb_decoder_tree = 0

def append_insn_to_isa_tag(isa_tag, insn):
    global insn_isa_tags

    if insn_isa_tags.get(isa_tag) is None:
        insn_isa_tags[isa_tag] = []
    insn_isa_tags[isa_tag].append(insn)


def dump(isaFile, str, level=0):
    for i in range(0, level):
        isaFile += '  '
    isaFile += str
    return isaFile

class Const(object):
    def __init__(self, val):
        self.val = val

    def len(self):
        return 1

    #def gen(self, signed=False):
    #    return '%d' % (self.val)

    def gen_info(self, isaFile):
        return dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_UIM, .u= { .uim= %d } }, ' % self.val)

    def gen(self, isaFile):
        return isaFile

class Range(object):
    def __init__(self, first, width=1, shift=0):
        self.first = first
        self.width = width
        self.shift = shift

    def gen(self, isaFile, indent=0):
        return dump(isaFile, '{%d, %d, %d}, ' % (self.first, self.width, self.shift))

    def gen_info(self, isaFile):
        isaFile = dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_RANGE, .u= { .range_set= {.nb_ranges=1, .ranges={ ')
        isaFile = self.gen(isaFile)
        isaFile = dump(isaFile, '} } } }, ')

        return isaFile


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
        isaFile = dump(isaFile, '{ .type=ISS_DECODER_VALUE_TYPE_RANGE, .u= { .range_set= {.nb_ranges=%d, .ranges={ ' % (len(self.ranges)))
        for range in self.ranges:
            isaFile = range.gen(isaFile)
        isaFile = dump(isaFile, '} } } }, ')

        return isaFile

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

    def genExtract(self, isaFile, level):
        isaFile = self.base.genExtract(isaFile, level)
        if self.offset is not None:
            isaFile = self.offset.genExtract(isaFile, level)

        return isaFile

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
            isaFile = dump(isaFile, level, '  %s(pc, %s, %s);\n' % (funcName, self.base.genTraceIndirect(), self.offset.genTraceIndirect()))
        else:
            isaFile = dump(isaFile, level, '  %s(pc, %s, 0);\n' % (funcName, self.base.genTraceIndirect()))
        return isaFile

    def gen(self, isaFile, indent=0):
        if self.postInc:
            self.flags.append('ISS_DECODER_ARG_FLAG_POSTINC')
        if self.preInc:
            self.flags.append('ISS_DECODER_ARG_FLAG_PREINC')
        if self.offset.is_reg():
            isaFile = dump(isaFile, '%s{\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_INDIRECT_REG,\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
            isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
            isaFile = dump(isaFile, '%s    .indirect_reg= {\n' % (' '*indent))
            isaFile = dump(isaFile, '%s      .base_reg= ' % (' '*indent))
            isaFile = self.base.gen_info(isaFile)
            isaFile = dump(isaFile, '\n%s      .offset_reg= ' % (' '*indent))
            isaFile = self.offset.gen_info(isaFile)
            isaFile = dump(isaFile, '\n%s    },\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  },\n' % (' '*indent))
            isaFile = dump(isaFile, '%s},\n' % (' '*indent))
        else:
            isaFile = dump(isaFile, '%s{\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_INDIRECT_IMM,\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
            isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
            isaFile = dump(isaFile, '%s    .indirect_imm= {\n' % (' '*indent))
            isaFile = dump(isaFile, '%s      .reg= ' % (' '*indent))
            isaFile = self.base.gen_info(isaFile)
            isaFile = dump(isaFile, '\n%s      .imm= ' % (' '*indent))
            isaFile = self.offset.gen_info(isaFile)
            isaFile = dump(isaFile, '\n%s    },\n' % (' '*indent))
            isaFile = dump(isaFile, '%s  },\n' % (' '*indent))
            isaFile = dump(isaFile, '%s},\n' % (' '*indent))

        return isaFile

class SignedImm(OpcodeField):
    def __init__(self, id, ranges, isSigned=True):
        self.ranges = ranges
        self.id = id
        self.isSigned = isSigned
        self.isImm = True

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->sim[%d] = %s;\n' % (self.id, self.ranges.gen(signed=self.isSigned)))

    def genTrace(self, isaFile, level):
        return dump(isaFile, level, '  traceSetSimm(pc, pc->sim[%d]);\n' % (self.id))

    def genTraceIndirect(self):
        return 'pc->sim[%d]' % (self.id)

    def gen_info(self, isaFile):
        isaFile = dump(isaFile, '{ .is_signed=%d, .id=%d, .info= ' % (self.isSigned, self.id))
        isaFile = self.ranges.gen_info(isaFile)
        isaFile = dump(isaFile, '}, ')
        return isaFile

    def gen(self, isaFile, indent=0):
        isaFile = dump(isaFile, '%s{\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_SIMM,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .flags=ISS_DECODER_ARG_FLAG_NONE,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
        isaFile = dump(isaFile, '%s    .simm= ' % (' '*indent))
        isaFile = self.gen_info(isaFile)
        isaFile = dump(isaFile, '\n%s  },\n' % (' '*indent))
        isaFile = dump(isaFile, '%s},\n' % (' '*indent))
        return isaFile

class UnsignedImm(OpcodeField):
    def __init__(self, id, ranges, isSigned=False):
        self.ranges = ranges
        self.id = id
        self.isSigned = isSigned
        self.isImm = True

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->uim[%d] = %s;\n' % (self.id, self.ranges.gen(signed=self.isSigned)))

    def genTrace(self, isaFile, level):
        return dump(isaFile, level, '  traceSetUimm(pc, pc->uim[%d]);\n' % (self.id))

    def genTraceIndirect(self):
        return 'pc->uim[%d]' % (self.id)

    def gen_info(self, isaFile):
        isaFile = dump(isaFile, '{ .is_signed=%d, .id=%d, .info= ' % (self.isSigned, self.id))
        isaFile = self.ranges.gen_info(isaFile)
        isaFile = dump(isaFile, '}, ')
        return isaFile

    def gen(self, isaFile, indent=0):
        isaFile = dump(isaFile, '%s{\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_UIMM,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .flags=ISS_DECODER_ARG_FLAG_NONE,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
        isaFile = dump(isaFile, '%s    .uimm= ' % (' '*indent))
        isaFile = self.gen_info(isaFile)
        isaFile = dump(isaFile, '\n%s  },\n' % (' '*indent))
        isaFile = dump(isaFile, '%s},\n' % (' '*indent))
        return isaFile

class OutReg(OpcodeField):
    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->outReg[%d] = %s;\n' % (self.id, self.ranges.gen()))

    def genTrace(self, isaFile, level):
        return dump(isaFile, level, '  traceSetReg(pc, pc->outReg[%d], 1, %d);\n' % (self.id, self.dumpName))

    def is_out(self):
        return True

    def is_reg(self):
        return True

    def gen_info(self, isaFile):
        isaFile = dump(isaFile, '{ .id=%d, .flags=(iss_decoder_arg_flag_e)(%s), .dump_name=%d, .latency=%d, .info= ' % (self.id, ' | '.join(self.flags), 1 if self.dumpName else 0, self.latency))
        isaFile = self.ranges.gen_info(isaFile)
        isaFile = dump(isaFile, '}, ')
        return isaFile

    def gen(self, isaFile, indent=0):
        isaFile = dump(isaFile, '%s{\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_OUT_REG,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
        isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
        isaFile = dump(isaFile, '%s    .reg= ' % (' '*indent))
        isaFile = self.gen_info(isaFile)
        isaFile = dump(isaFile, '\n%s  },\n' % (' '*indent))
        isaFile = dump(isaFile, '%s},\n' % (' '*indent))
        return isaFile

class OutFReg(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_FREG'])

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->outReg[%d] = %s + NB_REGS;\n' % (self.id, self.ranges.gen()))

class OutReg64(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutReg64, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_REG64'])

class OutRegComp(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED'])

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->outReg[%d] = (%s) + 8;\n' % (self.id, self.ranges.gen()))

class OutFRegComp(OutReg):
    def __init__(self, id, ranges, dumpName=True):
        super(OutFRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED', 'ISS_DECODER_ARG_FLAG_FREG'])

    def genExtract(self, isaFile, level):
         return dump(isaFile, level, '  pc->outReg[%d] = (%s) + 8 + NB_REGS;\n' % (self.id, self.ranges.gen()))

class InReg(OpcodeField):
    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->inReg[%d] = %s;\n' % (self.id, self.ranges.gen()))

    def genTrace(self, isaFile, level):
        return dump(isaFile, level, '  traceSetReg(pc, pc->inReg[%d], 0, %d);\n' % (self.id, self.dumpName))

    def genTraceIndirect(self):
        return 'pc->inReg[%d]' % (self.id)

    def is_out(self):
        return False

    def is_reg(self):
        return True

    def gen_info(self, isaFile):
        isaFile = dump(isaFile, '{ .id=%d, .flags=(iss_decoder_arg_flag_e)(%s), .dump_name=%d, .latency=%d, .info= ' % (self.id, ' | '.join(self.flags), 1 if self.dumpName else 0, self.latency))
        isaFile = self.ranges.gen_info(isaFile)
        isaFile = dump(isaFile, '}, ')
        return isaFile

    def gen(self, isaFile, indent=0):
        isaFile = dump(isaFile, '%s{\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .type=ISS_DECODER_ARG_TYPE_IN_REG,\n' % (' '*indent))
        isaFile = dump(isaFile, '%s  .flags=(iss_decoder_arg_flag_e)(%s),\n' % (' '*indent, ' | '.join(self.flags)))
        isaFile = dump(isaFile, '%s  .u= {\n' % (' '*indent))
        isaFile = dump(isaFile, '%s    .reg= ' % (' '*indent))
        isaFile = self.gen_info(isaFile)
        isaFile = dump(isaFile, '\n%s  },\n' % (' '*indent))
        isaFile = dump(isaFile, '%s},\n' % (' '*indent))
        return isaFile

class InFReg(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFReg, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_FREG'])

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->inReg[%d] = %s + NB_REGS;\n' % (self.id, self.ranges.gen()))

class InReg64(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InReg64, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_REG64'])

class InRegComp(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED'])

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->inReg[%d] = (%s) + 8;\n' % (self.id, self.ranges.gen()))

class InFRegComp(InReg):
    def __init__(self, id, ranges, dumpName=True):
        super(InFRegComp, self).__init__(id=id, ranges=ranges, dumpName=dumpName, flags=['ISS_DECODER_ARG_FLAG_COMPRESSED', 'ISS_DECODER_ARG_FLAG_FREG'])

    def genExtract(self, isaFile, level):
        return dump(isaFile, level, '  pc->inReg[%d] = (%s) + 8 + NB_REGS;\n' % (self.id, self.ranges.gen()))

class Format(object):
    def __init__(self, args):
        pass

class DecodeLeaf(object):
    def __init__(self, instr, opcode, others=False):
        self.instr = instr
        self.level = 0
        self.opcode = opcode
        self.others = others

    def dump(self, isaFile, str):
        #for i in range(0, self.level):
        #    isaFile += '  ')
        isaFile += str

        return isaFile

    def gen(self, isaFile, isa):
        isaFile = self.dump(isaFile, '// %s\n' % (self.instr.getStr()))
        isaFile = self.instr.gen(isa, isaFile, self.opcode, self.others)
        #self.dump('static iss_decoder_item_t %s = { false, 0, { %d, 0, 0, NULL } }; \n\n' % (self.get_name(), self.firstBit))
#
#        #if self.instr.active != None: self.dump('if (!%s) goto error;\n' % (self.instr.active.replace('-', '_')))
        #self.instr.genCall(isaFile, self.level)

        return isaFile

    def get_name(self):
        return self.instr.get_full_name()

class DecodeTree(object):
    def __init__(self, instrs, mask, opcode):
        self.opcode = opcode
        self.level = 0
        self.subtrees = collections.OrderedDict({})
        self.firstBit = 0
        global nb_decoder_tree
        self.tree_id = nb_decoder_tree
        nb_decoder_tree += 1

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
                error += instr.getStr() + '\n'
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
            else: self.subtrees[opcode] = DecodeTree(instrs, mask, opcode)

        self.mask = currentMask

        self.needTree = False
        for opcode, subtree in self.subtrees.items():
            if len(opcode) != 0: self.needTree = True


    def dump(self, isaFile, str, diff=0):
        #for i in range(0, self.level+diff):
        #    isaFile += '  ')
        isaFile += str
        return isaFile

    def get_name(self):
        if self.needTree:
            return 'decoder_tree_%d' % self.tree_id
        else:
            return list(self.subtrees.values())[0].get_name()


    def gen(self, isaFile, isa, is_top=False):

        if len(self.subtrees) != 0:

            if self.needTree:
                others = None
                for opcode, subtree in self.subtrees.items():
                    if opcode == 'OTHERS':
                        others = subtree
                    else:
                        isaFile = subtree.gen(isaFile, isa)
                if others != None:
                    isaFile = others.gen(isaFile, isa)
            else:
                subtree = list(self.subtrees.values())[0]
                isaFile = subtree.gen(isaFile, isa)


            if self.needTree:
                isaFile = self.dump(isaFile, 'static iss_decoder_item_t *%s_groups[] = {' % self.get_name());
                for opcode, subtree in self.subtrees.items():
                    isaFile = self.dump(isaFile, ' &%s,' % subtree.get_name())
             
                isaFile = self.dump(isaFile, ' };\n')

                isaFile = self.dump(isaFile, '%siss_decoder_item_t %s = {\n' % ('' if is_top else 'static ', self.get_name()))
                isaFile = self.dump(isaFile, '  .is_insn=false,\n')
                isaFile = self.dump(isaFile, '  .is_active=false,\n')
                isaFile = self.dump(isaFile, '  .opcode_others=0,\n')
                isaFile = self.dump(isaFile, '  .opcode=0b%s,\n' % self.opcode)
                isaFile = self.dump(isaFile, '  .u={\n')
                isaFile = self.dump(isaFile, '    .group={\n')
                isaFile = self.dump(isaFile, '      .bit=%d,\n' % self.firstBit)
                isaFile = self.dump(isaFile, '      .width=%d,\n' % self.opcode_width)
                isaFile = self.dump(isaFile, '      .nb_groups=%d,\n' % len(self.subtrees))
                isaFile = self.dump(isaFile, '      .groups=%s_groups\n' % self.get_name())
                isaFile = self.dump(isaFile, '    }\n')
                isaFile = self.dump(isaFile, '  }\n')
                isaFile = self.dump(isaFile, '};\n')
                isaFile = self.dump(isaFile, '\n')
            
        return isaFile
        
class IsaSubset(object):
    def __init__(self, name, instrs, active=None, file="default", timingTable=None):
        self.name = name
        self.instrs = instrs
        self.active = active
        for instr in instrs:
            instr.isa = self
            instr.active = active
        if file == 'default': self.file = name
        else: self.file = file

        self.insn_registered = False

    def get_insns(self):
        if not self.insn_registered:
            self.insn_registered = True

            for insn in self.instrs:
                if len(insn.isa_tags) == 0:
                    append_insn_to_isa_tag(self.name, insn)
                else:
                    for isa_tag in insn.isa_tags:
                        append_insn_to_isa_tag(isa_tag, insn)

        return self.instrs

    def setPower(self, power):
        for instr in self.instrs:
            instr.setPower(power)

    def getOptions(self):
        options = collections.OrderedDict({})
        for instr in self.instrs:
            options.update(instr.getOptions())

        return options
        
class IsaDecodeTree(object):

    def __init__(self, name, subsets):
        self.subsets = subsets
        self.tree = None
        self.name = name
            
    def get_insns(self):
        result = []
        
        for subset in self.subsets:
            result += subset.get_insns()

        return result

    def dumpTree(self, isa, isaFile):
        instrs = self.get_insns()

        if self.tree == None:
            self.tree = DecodeTree(instrs, 0xffffffff, '0')

        if len(instrs) != 0:
            isaFile = self.tree.gen(isaFile, isa, is_top=True)

        return isaFile

    def dump_ref(self, isa, isaFile):
        return dump(isaFile, '  {(char *)"%s", &%s},\n' % (self.name, self.tree.get_name()))


class Resource(object):

    def __init__(self, name, instances=1):
        self.name = name
        self.instances = instances


class Isa(object):
    def __init__(self, name, trees):
        self.level = 0
        self.name = name
        self.trees = trees
        self.trees_dict = {}
        self.resources = []

        for tree in self.trees:
            self.trees_dict[tree.name] = tree

    def add_tree(self, tree):
        self.trees.append(tree)
        self.trees_dict[tree.name] = tree



    def get_resource_index(self, resource_name):
        index = 0
        for resource in self.resources:
            if resource.name == resource_name:
                return index
            index += 1

        return -1

    def add_resource(self, name, instances=1):
        self.resources.append(Resource(name, instances))


    def get_insns(self):
        result = []
        
        for tree in self.trees:
            result += tree.get_insns()

        return result

    def dump(self, isaFile, str):
        for i in range(0, self.level):
            isaFile += '  '
        isaFile += str
        return isaFile

    def get_tree(self, name):
        return self.trees_dict.get(name)

    def gen(self):

        isaFile = ""
        isaFileHeader = ""

        isaFile = self.dump(isaFile, '#include "cpu/iss/include/iss.hpp"\n')

        isaFile = self.dump(isaFile, '\n')

        isaFileHeader += '#ifndef __GV_ISA_%s__HPP_\n' % self.name
        isaFileHeader += '#define __GV_ISA_%s__HPP_\n' % self.name

        isaFileHeader += '#endif\n'

        isaFile = self.dump(isaFile, 'static iss_resource_t __iss_resources[]\n')
        isaFile = self.dump(isaFile, '{\n')
        for resource in self.resources:
            isaFile = self.dump(isaFile, '  {"%s", %d},\n' % (resource.name, resource.instances))
        isaFile = self.dump(isaFile, '};\n')
        isaFile = self.dump(isaFile, '\n')

        for tree in self.trees:
            isaFile = tree.dumpTree(self, isaFile)

        for isa_tag in insn_isa_tags.keys():
            isaFile = self.dump(isaFile, 'static iss_decoder_item_t *__iss_isa_tag_%s[] = {\n' % isa_tag)
            insn_list = []
            for insn in insn_isa_tags.get(isa_tag):
                insn_list.append('&' + insn.get_full_name())
            insn_list.append('NULL')
            isaFile = self.dump(isaFile, '  %s\n' % ', '.join(insn_list))
            isaFile = self.dump(isaFile, '};\n')
            isaFile = self.dump(isaFile, '\n')

        isaFile = self.dump(isaFile, 'iss_isa_tag_t __iss_isa_tags[] = {\n')
        for isa_tag in insn_isa_tags.keys():
            isaFile = self.dump(isaFile, '  {(char *)"%s", __iss_isa_tag_%s},\n' % (isa_tag, isa_tag))
        isaFile = self.dump(isaFile, '  {(char *)NULL, NULL}\n')
        isaFile = self.dump(isaFile, '};\n')
        isaFile = self.dump(isaFile, '\n')


        isaFile = self.dump(isaFile, 'static iss_isa_t __iss_isa_list[] = {\n')
        for tree in self.trees:
            isaFile = tree.dump_ref(self, isaFile)
        isaFile = self.dump(isaFile, '};\n')
        isaFile = self.dump(isaFile, '\n')

        isaFile = self.dump(isaFile, 'iss_isa_set_t __iss_isa_set = {\n')
        isaFile = self.dump(isaFile, '  .nb_isa=sizeof(__iss_isa_list)/sizeof(iss_isa_t),\n')
        isaFile = self.dump(isaFile, '  .isa_set=__iss_isa_list,\n')
        isaFile = self.dump(isaFile, '  .nb_resources=%d,\n' % len(self.resources))
        isaFile = self.dump(isaFile, '  .resources=__iss_resources,\n')
        isaFile = self.dump(isaFile, '};\n')

        return isaFile, isaFileHeader



InstrNames = []
timingTables = []

class TimingTable(object):
    def __init__(self, name, option, default):
        self.option = option
        self.default = default
        self.name = name
        self.groups = []
        timingTables.append(self)

    def getStructName(self):
        return 'timingTable_%s' % self.name

    def regGroup(self, group):
        self.groups.append(group)

    def gen(self, isaFile):

        isaFile += 'static char *%s_names[] = { ' % (self.getStructName())
        for group in self.groups:
            isaFile += '(char *)"%s", ' % group.name
        isaFile += '};\n\n'


        isaFile += 'static timingTable_t %s = { "%s", "%s", "%s", 0, %d, %s_names, NULL, NULL };\n' % (self.getStructName(), self.name, self.option, self.default, len(self.groups), self.getStructName())

        return isaFile

class IsaGroup(object):
    def __init__(self, name, offload=None, timingTable=None):
        self.name = name
        self.nbGroups = 0
        self.timingTable = timingTable
        self.offload = offload

    def regGroup(self, group):
        group.id = self.nbGroups
        self.nbGroups += 1
        if self.timingTable != None: self.timingTable.regGroup(group)

    def getTiming(self):
        if self.timingTable == None: return None
        else: return self.timingTable.getStructName()

    def getOffload(self):
        if self.offload == None: return None
        return self.offload

    def getOptions(self):
        if self.offload != None: return { self.offload[0] : self.offload }
        else: return collections.OrderedDict({})



class InstrGroup(object):
    def __init__(self, isaGroup, name, offload=None):
        self.isaGroup = isaGroup
        self.name = name
        self.offload = offload
        isaGroup.regGroup(self)

    def getTiming(self):
        return self.isaGroup.getTiming()

    def getOffload(self):
        if self.offload != None: return self.offload
        else: return self.isaGroup.getOffload()

    def getOptions(self):
        options = self.isaGroup.getOptions()
        if self.offload != None: options.update({self.offload[1] : self.offload })
        return options


defaultIsaGroup   = IsaGroup('ISA_GROUP_OTHER')
defaultInstrGroup = InstrGroup(defaultIsaGroup, 'INSTR_GROUP_OTHER')

class Instr(object):
    def __init__(self, label, type, encoding, decode=None, N=None, L=None, mapTo=None, power=None, group=None,
            fast_handler=False, tags=[], isa_tags=[], is_macro_op=False):
        global nb_insn

        self.insn_number = nb_insn
        self.tags = tags
        self.isa_tags = isa_tags
        self.out_reg_latencies = []
        self.latency = 0
        self.power_group = 0
        self.resource = None
        self.resource_latency = 0
        self.resource_bandwidth = 0
        self.is_macro_op = is_macro_op
        nb_insn += 1

        encoding = encoding[::-1].replace(' ', '')
        self.label = label
        if L != None: self.traceLabel = L
        else: self.traceLabel = label
        self.safeLabel = label.replace('.', '_')
        self.encoding = encoding
        self.decode = decode
        self.srcId = 0
        if mapTo != None: name = mapTo
        else: 
            if N == None: name = self.safeLabel
            else: name = N
        self.execFunc = '%s_exec' % (name)
        if not fast_handler:
            self.quick_execFunc = self.execFunc
        else:
            self.quick_execFunc = self.execFunc + '_fast'

        self.decodeFunc = '%s_decode_gen' % (self.safeLabel)
        index = 0
        while self.decodeFunc in InstrNames:
            self.decodeFunc = '%s_%d_decode_gen' % (self.safeLabel, index)
            index += 1
        InstrNames.append(self.decodeFunc)
        self.len = len(encoding.strip())
        self.power = power
        if group != None: self.group = group
        else: self.group = defaultInstrGroup

        self.offload = None
        if group != None: self.offload = group.getOffload()

        if instrLabels.get(self.traceLabel) == None:
            instrLabels[self.traceLabel] = len(instrLabels)
            instrLabelsList.append(self.traceLabel)
        
        self.id = instrLabels[self.traceLabel]

    def attach_resource(self, name, latency, bandwidth):
        self.resource = name
        self.resource_latency = latency
        self.resource_bandwidth = bandwidth

    def set_latency(self, latency):
        self.latency = latency

    def set_power_group(self, power_group):
        self.power_group = power_group

    def get_out_reg(self, reg):
        index = 0
        for arg in self.args:
            if arg.is_reg() and arg.is_out():
                if index == reg:
                    return arg
                index += 1
        return None

    def get_full_name(self):
        return ('insn_%d_%s_%s' % (self.insn_number, self.isa.name, self.getLabel())).replace('.', '_')

    def getLabel(self):
        return self.traceLabel

    def setPower(self, power):
        # Keep local power setup if it is setup to priviledge specialization
        if self.power == None: self.power = power

    def genArgsExtract(self, isaFile, level):
        for arg in self.args:
            arg.genExtract(isaFile, level)

        return isaFile

    def getStr(self):
        return '%s: %s' % (self.encoding[::-1], self.label)

    def getSrcId(self):
        srcId = self.srcId
        self.srcId += 1
        return srcId

    def dump(self, isaFile, str, level=0):
        for i in range(0, level):
            isaFile += '  '
        isaFile += str
        return isaFile

    def genCall(self, isaFile, level):
        return self.dump(isaFile, '%s(cpu, pc);\n' % (self.decodeFunc), level)

    def gen(self, isa, isaFile, opcode, others=False):

        name = self.get_full_name()

        isaFile = self.dump(isaFile, 'static iss_decoder_item_t %s = {\n' % (name))
        isaFile = self.dump(isaFile, '  .is_insn=true,\n')
        isaFile = self.dump(isaFile, '  .is_active=false,\n')
        isaFile = self.dump(isaFile, '  .opcode_others=%d,\n' % (1 if others else 0))
        isaFile = self.dump(isaFile, '  .opcode=0b%s,\n' % opcode)
        isaFile = self.dump(isaFile, '  .u={\n')
        isaFile = self.dump(isaFile, '    .insn={\n')
        isaFile = self.dump(isaFile, '      .handler=%s,\n' % self.execFunc)
        isaFile = self.dump(isaFile, '      .fast_handler=%s,\n' % self.quick_execFunc)
        isaFile = self.dump(isaFile, '      .decode=%s,\n' % ('NULL' if self.decode is None else self.decode))
        isaFile = self.dump(isaFile, '      .label=(char *)"%s",\n' % (self.getLabel()))
        isaFile = self.dump(isaFile, '      .size=%d,\n' % (self.len/8))
        isaFile = self.dump(isaFile, '      .nb_args=%d,\n' % (len(self.args)))
        isaFile = self.dump(isaFile, '      .latency=%d,\n' % (self.latency))
        isaFile = self.dump(isaFile, '      .args= {\n')
        if len(self.args) > 0:
            for arg in self.args:
                isaFile = arg.gen(isaFile, indent=8)
        isaFile = self.dump(isaFile, '      },\n')
        isaFile = self.dump(isaFile, '      .resource_id=%d,\n' % (-1 if self.resource is None else isa.get_resource_index(self.resource)))
        isaFile = self.dump(isaFile, '      .resource_latency=%d,\n' % self.resource_latency)
        isaFile = self.dump(isaFile, '      .resource_bandwidth=%d,\n' % self.resource_bandwidth)
        isaFile = self.dump(isaFile, '      .power_group=%d,\n' % (self.power_group))
        isaFile = self.dump(isaFile, '      .is_macro_op=%d,\n' % (self.is_macro_op))
        isaFile = self.dump(isaFile, '    }\n')
        isaFile = self.dump(isaFile, '  }\n')
        isaFile = self.dump(isaFile, '};\n')
        isaFile = self.dump(isaFile, '\n')

        return isaFile

    def getOptions(self):
        if self.group != None: return self.group.getOptions()
        else: return collections.OrderedDict({})
